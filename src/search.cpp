#include <atomic>
#include "search.hpp"
#include "Uci.hpp"
#include "Position_impl.hpp"

void SearchLimits::assertNodesOk() const {
    assert (0 <= quotaCounter_);
    //assert (quotaCounter_ < static_cast<int>(quotaLimit_));
    //assert (0 <= nodes);
    assert (nodes_ <= nodesLimit_);
    assert (static_cast<decltype(nodesLimit_)>(quotaCounter_) <= nodes_);
}

ReturnStatus SearchLimits::refreshQuota() {
    assertNodesOk();

    // expected nodesQuata_ == 0
    nodes_ -= quotaCounter_;
    //quotaCounter_ = 0; // keeps invariant, but redundant

    auto remainingLimit = nodesLimit_ - nodes_;
    if (remainingLimit >= quotaLimit_) {
        quotaCounter_ = quotaLimit_;
    }
    else {
        quotaCounter_ = static_cast<decltype(quotaCounter_)>(remainingLimit);
        if (quotaCounter_ == 0) {
            // `go nodes` limit reached
            assertNodesOk();
            return ReturnStatus::Stop;
        }
    }

    assert (0 < quotaCounter_); assert (quotaCounter_ <= static_cast<int>(quotaLimit_));
    nodes_ += quotaCounter_; // allocate new nodesQuota

    return lastDeadlineReached();
}

template <SearchLimits::time_quota_t TimeQuota>
ReturnStatus SearchLimits::reachedTime() const {
    if (stop_.load(std::memory_order_seq_cst)) { return ReturnStatus::Stop; } // unconditional stop
    if (timePool_ == UnlimitedTime || pondering_.load(std::memory_order_relaxed)) { return ReturnStatus::Continue; }
    if (getNodes() < quotaLimit_) { return ReturnStatus::Continue; } // avoid early time check throttling

    auto timePool = timePool_;
    if (timeStrategy_ != ExactTime) {
        int timeQuota = TimeQuota;
        if (TimeQuota != MaxQuota) { timeQuota += lowMaterialQuotaBonus_; }

        timePool *= +timeStrategy_ * timeQuota;
        timePool /= +HardMove * MaxQuota; // 512
    }

    auto remainingTime = timePool - ::elapsedSince(searchStartTime_);
    if (remainingTime < 1ms) { quotaLimit_ = QuotaLimitSmall; }
    return remainingTime > 0ms ? ReturnStatus::Continue : ReturnStatus::Stop;
}
ReturnStatus SearchLimits::lastDeadlineReached() const { return reachedTime<MaxQuota>(); }
ReturnStatus SearchLimits::iterationDeadlineReached() const { return reachedTime<IterationQuota>(); }

ReturnStatus SearchLimits::updateTimeStrategy(const PrincipalVariation& pv) {
    if (timeStrategy_ == ExactTime) { return ReturnStatus::Continue; }

    auto bestMove = pv.getMove(0_ply);
    auto score = pv.score();
    auto depth = pv.depth();

    if (lastMove_.none()) {
        lastMove_ = bestMove;
    } else {
        if (lastMove_ != bestMove) {
            lastMove_ = bestMove;
            timeStrategy_ = HardMove; // best root move just have changed
            hardMoveDepth_ = depth;
        } else if (lastScore_.isEval() && score.isEval() && score + 40_cp <= lastScore_) {
            timeStrategy_ = HardMove; // root score have dropped
            hardMoveDepth_ = depth;
        } else if (timeStrategy_ == HardMove && hardMoveDepth_ + 2_ply <= depth) {
            timeStrategy_ = NormalMove; // no negative factors during full iteration
        }
    }

    lastScore_ = score;

    // good place to check time as there are no wasted search nodes
    // and timeStrategy_ just possibly changed
    return lastDeadlineReached();
}

void Node::clearNode() {
    assert (hasParent());
    pvPly = parent().isPv() ? ply : parent().pvPly;
    pvIndex = PrincipalVariation::Index{+parent().pvIndex + 1};
    alpha = -parent().beta;
    beta = -parent().alpha;
    killers[0] = hasGrandParent() ? grandParent().killers[0] : Move{};
    killers[1] = {};
}

void Node::assertOk() const {
    assert (alpha < beta);
    if (score.any()) {
        assert (score < beta || bound.is(FailHigh));
        assert (alpha <= score || bound.is(FailLow));
    }
    assert (Score{MateLoss} <= alpha);
    assert (beta <= Score{MateWin});
}

ReturnStatus Node::negamax(Ply R) {
    child().depth = depth - R; //TRICK: Ply >= 0
    /* assert (child().depth >= 0); */
    child().generateMoves();
    RETURN_IF_STOP (child().search());
    assertOk();

    auto childScore = -child().score;

    if (childScore <= alpha) {
        if (score.none() || score < childScore) {
            score = childScore;
        }
    } else {
        // do not use R param as actual reduction can differ
        auto childR = child().currentR();

        // full depth research (unless it was a null move search)
        if (currentMove.any() && childR >= 2_ply) {
            if (child().isPv()) {
                // rare case (the first move from PV with reduced depth)
                child().alpha = -beta;
                assert (child().beta == -alpha);
            } else {
                assert (child().alpha == child().beta.minus1());
            }
            return negamax();
        }

        if (beta <= childScore) {
            score = childScore;
            bound = FailHigh;
            // currentMove.none() after NMP
            if (currentMove.any()) {
                bestMove = currentMove;
                saveHistory();
            }
            return ReturnStatus::Cutoff;
        }

        assert (childR <= 1_ply);
        assert (alpha < childScore && childScore < beta);
        assert (isPv()); // alpha < childScore < beta, so current window cannot be zero
        assert (currentMove.any()); // null move in PV is not allowed

        if (!child().isPv()) {
            child().pvPly = child().ply;
            assert (child().isPv());
            child().alpha = -beta;
            assert (child().beta == -alpha);
            // Principal Variation Search (PVS) research with full window and full depth
            return negamax();
        }

        score = childScore;
        bound = ExactBound;
        assert (currentMove.any()); // null move in PV is not allowed
        bestMove = currentMove;

        if (!isRoot()) {
            child().pvIndex = The_uci.pv.set(pvIndex, bestMove, child().pvIndex);
        } else {
            // unfinished iteration, so report depth-1
            pvIndex = The_uci.pv.set(depth - 1_ply, score, bestMove, child().pvIndex);
            child().pvIndex = PrincipalVariation::Index{+pvIndex+1};

            RETURN_IF_STOP (The_uci.limits.updateTimeStrategy(The_uci.pv));

            if (depth > 1_ply) { The_uci.info_pv(); }
        }

        alpha = childScore;
        child().beta = -alpha;
    }

    // set zero window for the next sibling move search
    child().alpha = child().beta.minus1(); // can be either 1 centipawn or 1 mate distance ply
    child().pvPly = pvPly;

    assert (child().beta == -alpha);
    return ReturnStatus::Continue;
}

namespace {

struct TtRecord {
    TtSlot ttSlot;
    TtSlot* tt;
    bool ttHit;
};

constexpr TtRecord probe(TtSlot* tt, Z z, TtAge ttAge) {
    using std::bit_cast;
    auto ttSlot = bit_cast<TtSlot>(bit_cast<std::atomic<u64_t>*>(tt)->load(std::memory_order_relaxed));
    if (ttSlot == z) { return { ttSlot, tt, true }; }

    auto tt2 = bit_cast<TtSlot*>(bit_cast<std::uintptr_t>(tt) ^ sizeof(TtSlot));
    auto ttSlot2 = bit_cast<TtSlot>(bit_cast<std::atomic<u64_t>*>(tt2)->load(std::memory_order_relaxed));
    if (ttSlot2 == z) { return { ttSlot2, tt2, true }; }

    if (
        (ttSlot2.any() && ttSlot.draft() <= ttSlot2.draft())
        || ttAge.isOld(ttSlot.age()) // old
        || ttSlot.none() // zeroed
    ) {
        return {ttSlot, tt, false};
    } else {
        return {ttSlot2, tt2, false};
    }
}

} // end of anonymous namespace

ReturnStatus Node::search() {
    baseR = depth / 8;
    eval  = {};
    cEval = {};
    score = {};
    bound = FailLow;
    currentMove = {};
    bestMove = {};
    assertOk();

    if (!isRoot()) {
        if (inCheck()) {
            if (movesTotal() == 0) {
                // checkmate
                score = Score::mateLoss(ply);
                assert (currentMove.none());
                return ReturnStatus::Continue;
            } else if (depth <= 3_ply && movesTotal() == 1) {
                // single reply extension
                depth = depth + 1_ply;
            }

            // check extension
            depth = depth + 1_ply;
        } else {
            if (movesTotal() == 0) {
                // stalemate
                assert (!inCheck());
                score = Score{DrawScore};
                assert (currentMove.none());
                return ReturnStatus::Continue;
            }
        }

        if (isRepetition() || rule50().isDraw() || isDrawMaterial()) {
            score = Score{DrawScore};
            assert (currentMove.none());
            return ReturnStatus::Continue;
        }

        // mate-distance pruning
        alpha = std::max(alpha, Score::mateLoss(ply));
        if (!(alpha < std::min(beta, Score::mateWin(ply + 1_ply)))) {
            score = alpha;
            assert (currentMove.none());
            return ReturnStatus::Cutoff;
        }
    }

    // lookup Transposition Table
    do {
        assert (eval.none());
        assert (cEval.none());
        assert (score.none());
        assert (bestMove.none());
        if (depth == 0_ply) { break; }

        ++The_uci.tt.reads;
        auto [ttSlot, ttPtr, ttHit] = ::probe(tt, z(), The_uci.ttAge);
        tt = ttPtr; // place to write after completed search

        if (!ttHit || ttSlot.none()) { break; }

        if (ttSlot.ttMove(z()).any()) [[likely]] {
            auto ttMove = ttSlot.ttMove(z());
            if (!isPossibleMove(ttMove.from(), ttMove.to())) [[unlikely]] {
                // collision detection
                break;
            }
            bestMove = toMove(ttMove);
        }

        Score ttScore = ttSlot.score().fromTt(ply);
        if (ttScore.none()) [[unlikely]] {
            //io::error("prevented TT collision due invalid mate score");
            bestMove = {};
            break;
        }

        ++The_uci.tt.hits;

        Bound ttBound = ttSlot.bound(); assert (ttBound.any());
        if (!isPv() && depth <= ttSlot.draft() && (ttBound.is(ExactBound)
            || (ttBound.is(FailHigh) && beta <= ttScore)
            || (ttBound.is(FailLow) && ttScore <= alpha)
        )) {
            score = ttScore;
            bound = ttBound;
            if (!ttSlot.age().is(The_uci.ttAge)) {
                // refresh age
                ttSlot.setAge(The_uci.ttAge);
                *tt = ttSlot;
            }
            return ReturnStatus::Cutoff;
        }

        if (!inCheck()) [[likely]] {
            // reuse saved eval
            eval = ttSlot.eval();
            assert (eval.isEval());
            //assert (eval == evaluate()); // undetected collision

            if (ttScore.isEval() && (ttBound.is(ExactBound)
                || (ttBound.is(FailHigh) && eval <= ttScore)
                || (ttBound.is(FailLow) && ttScore <= eval)
            )) {
                cEval = ttScore;
            } else {
                cEval = eval;
            }
        }
    } while(false);

    if (eval.none() && !inCheck()) { cEval = eval = evaluate(); }
    assert ((inCheck() && eval.none()) || (!inCheck() && eval.isEval()));
    assert (bestMove.none() || isPossibleMove(bestMove));

    if (ply == MaxPly) {
        // no room to search deeper
        score = inCheck() ? Score::mateLoss(ply) : cEval;
        assert (currentMove.none());
        return ReturnStatus::Continue;
    }

// search all moves:

    // prepare empty child node to make moves into
    child().clearNode();

    if (depth <= 0_ply && !inCheck()) {
        assert (depth == 0_ply);
        return quiescence();
    }

    assert (currentMove.none());

    if (!isPv() && !inCheck()) {
        if (depth <= 3_ply) {
            auto delta = (depth == 1_ply) ? 50_cp : (depth == 2_ply) ? 150_cp : 200_cp;
            if (Score{MinEval} <= beta && beta <= cEval-delta) {
                // Static Null Move Pruning (Reverse Futility Pruning)
                score = cEval;
                assert (currentMove.none());
                return ReturnStatus::Cutoff;
            } else {
                delta = (depth == 1_ply) ? 50_cp : (depth == 2_ply) ? 250_cp : 350_cp;
                if (cEval+delta < alpha && alpha <= Score{MaxEval}) {
                    // Razoring
                    return quiescence();
                }
            }
        }

        // Null Move Pruning
        if (
            depth >= 2_ply // overhead higher then gain at very low depth
            && MY.material().canNullMove() // avoid null move in late endgame
            && Score{MinEval} <= beta && beta <= cEval
        ) {
            RETURN_CUTOFF (searchNullMove());
        }
    }

    // trying TT move first
    if (bestMove.any()) {
        RETURN_CUTOFF (searchMove(bestMove));
    }

    if (isRoot()) {
        for (auto move : The_uci.rootBestMoves) {
            if (move.none()) { break; }
            RETURN_CUTOFF (searchIfPossible(move));
        }
    }

    RETURN_CUTOFF (goodCaptures(OP.nonKing()));

    if (inCheck()) {
        if (hasParent()) { //TODO: use game history move when root in check
            RETURN_CUTOFF (searchIfPossible(
                The_uci.checkMoves.get(colorToMove(), MY.sqKing(), parent().currentMove)
            ));
        }
    } else {
        RETURN_CUTOFF (searchIfPossible(killers[0]));

        bool isDeep{ depth > ply };
        if (counterMove().any()) {
            RETURN_CUTOFF (contMove(isDeep ? DeepCounter : Counter, counterMove())); // ply-1
        }
        if (followupMove().any()) {
            RETURN_CUTOFF (contMove(isDeep ? DeepFollowup : Followup, followupMove())); // ply-2
        }

        RETURN_CUTOFF (searchIfPossible(killers[1]));

        if (counterMove().any()) {
            RETURN_CUTOFF (contMove(Counter, counterMove())); // ply-1
        }
        if (followupMove().any()) {
            RETURN_CUTOFF (contMove(Followup, followupMove())); // ply-2
        }
    }

    do {
        // going to search only non-captures, mask out remaining unsafe captures to avoid redundant safety checks
        //TRICK: ~ is not a negate bitwise operation but byteswap -- flip opponent's bitboard
        //TODO: mask out pinned enemy pawns
        Bb bbAvoid = ~(OP.bbPawnAttacks() | OP.bbSide());
        PiMask safePieces = {}; // pieces on safe squares

        // officers (Q, R, B/N order) moves from unsafe to safe squares
        for (Pi pi : MY.officers()) {
            Square from = MY.sq(pi);

            if (!bbAttacked().has(from)) {
                // piece is not attacked at all
                safePieces += PiMask{pi};
                continue;
            }

            assert (OP.attackersTo(~from).any());

            if (OP.attackersTo(~from).none(OP.lessOrEqualValue(MY.typeOf(pi)))) {
                // attacked by more valuable attacker

                if (MY.bbPawnAttacks().has(from) || safeForMe(from)) {
                    // piece is protected
                    safePieces += PiMask{pi};
                    continue;
                }
                //TODO: try protecting moves of other pieces
            }

            RETURN_CUTOFF (goodNonCaptures(pi, bbMovesOf(pi) % bbAvoid, 2_ply));
        }

        // safe passed pawns moves
        for (Square from : bbPassedPawns() % Bb{Rank7}) {
            Pi pi = MY.pi(from);
            for (Square to : bbMovesOf(pi)) {
                if (MY.bbPawnAttacks().has(to) || !safeForOp(to)) {
                    RETURN_CUTOFF (searchMove(from, to, from.on(Rank6) ? 1_ply : 2_ply));
                }
            }
        }

        // safe pawns pushes attacking non-pawns
        RETURN_CUTOFF (goodPawnsMovesTo(~(OP.bbSide() - OP.bbPawns()), 2_ply));

        if (depth <= 1_ply && !inCheck() && movesMade() >= 3) { break; }

        if (depth >= 6_ply && movesMade() >= 5) { baseR = baseR + 1_ply; } // LMR

        // safe officers moves
        while (safePieces.any()) {
            Pi pi = safePieces.piLast(); safePieces -= PiMask{pi};
            RETURN_CUTOFF (goodNonCaptures(pi, bbMovesOf(pi) % bbAvoid, 3_ply));
        }

        if (depth <= 2_ply && !inCheck() && (!isPv() || movesMade() > 0)) { break; }

        // king quiet moves (always safe), castling is a rook move
        {
            Pi pi{TheKing};
            Square from{MY.sqKing()};
            for (Square to : bbMovesOf(pi)) {
                RETURN_CUTOFF (searchMove(from, to, 3_ply));
            }
        }

        // remaining (losing) queen promotion moves
        for (Pi pi : MY.promotables()) {
            Square from{MY.sq(pi)};
            Square to{from.file(), Rank8};
            RETURN_CUTOFF (searchIfPossible(toMove(from, to, CanBeKiller::Yes), 3_ply));
        }

        // unsafe (losing) captures (N/B, R, Q order)
        for (PiMask pieces = MY.officers(); pieces.any(); ) {
            Pi pi = pieces.piLast(); pieces -= PiMask{pi};
            Square from{MY.sq(pi)};
            for (Square to : bbMovesOf(pi) & ~OP.bbSide()) {
                RETURN_CUTOFF (searchMove(from, to, 3_ply));
            }
        }

        // all remaining pawn moves:
        // all underpromotions, losing passed pawns moves, the rest pawns moves
        for (Square from : MY.bbPawns()) {
            Pi pi = MY.pi(from);
            for (Square to : bbMovesOf(pi)) {
                RETURN_CUTOFF (searchMove(from, to, 3_ply));
            }
        }

        if (depth <= 4_ply && !inCheck() && (!isPv() || movesMade() > 0)) { break; }

        // unsafe (losing) non-captures (N/B, R, Q order)
        for (PiMask pieces = MY.officers(); pieces.any(); ) {
            Pi pi = pieces.piLast(); pieces -= PiMask{pi};
            Square from{MY.sq(pi)};
            for (Square to : bbMovesOf(pi)) {
                RETURN_CUTOFF (searchMove(from, to, 4_ply));
            }
        }
    } while (false);

    if (movesMade() == 0) {
        // not stalemate, all moves pruned
        assert (bound.is(FailLow));
        assert (bestMove.none());
        assert (currentMove.none());
        assert (!inCheck());
        if (score.none()) { score = alpha; } // !score.none() if null move happened (null move not counted in movesMade())
        return ReturnStatus::Continue;
    }

    if (bound.is(ExactBound)) {
        assert (isPseudoLegal(bestMove));
        saveHistory();
        if (isRoot()) { ::insert_unique_compact(The_uci.rootBestMoves, bestMove); }
    } else {
        assert (bound.is(FailLow));
        assert (bestMove.none() || isPseudoLegal(bestMove));
        assert (depth > 0_ply);
        assert (score.isOk(ply));
        saveNode();
    }
    return ReturnStatus::Continue;
}

// safe pawns pushes attacking non-pawns
ReturnStatus Node::goodPawnsMovesTo(Bb target, Ply R) {
    Bb totallySafe = (Bb::full() % bbAttacked()) | (MY.bbPawnAttacks() % ~OP.bbPawnAttacks());

    Bb canAttackFrom = (target.pBackwardDiag() % OCCUPIED).pBackward(); // squares from where pawns can move to attack target
    for (Square from : MY.bbPawns() & canAttackFrom) {
        Square to{ from.file(), from.rank().forward() }; assert (!OCCUPIED.has(to));
        if ( bbMovesOf(MY.pi(from)).has(to) && (totallySafe.has(to) || !safeForOp(to)) ) {
            RETURN_CUTOFF (searchMove(from, to, R));
        }
    }

    // double push attacks
    canAttackFrom = (canAttackFrom % OCCUPIED).pBackward() & Bb{Rank2};
    for (Square from : MY.bbPawns() & canAttackFrom) {
        assert (!OCCUPIED.has(Square{ from.file(), Rank{Rank3} }));
        Square to{ from.file(), Rank{Rank4} }; assert (!OCCUPIED.has(to));
        if ( bbMovesOf(MY.pi(from)).has(to) && (totallySafe.has(to) || !safeForOp(to)) ) {
            RETURN_CUTOFF (searchMove(from, to, R));
        }
    }

    return ReturnStatus::Continue;
}

ReturnStatus Node::goodNonCaptures(Pi pi, Bb bbMoves, Ply R) {
    PieceType ty = MY.typeOf(pi);
    assert (!ty.is(Pawn));
    PiMask opLessValue = OP.lessValue(ty);
    Square from{MY.sq(pi)};
    for (Square to : bbMoves) {
        assert (!OP.bbPawnAttacks().has(~to));
        assert (isQuietMove(pi, to));

        if (bbAttacked().has(to)) {
            if ((OP.attackersTo(~to) & opLessValue).any()) {
                // square defended by less valued opponent's piece
                continue;
            }

            if (!(MY.bbPawnAttacks().has(to) || safeForMe(to))) {
                // skip move to the defended square
                continue;
            }
        }

        RETURN_CUTOFF (searchMove(from, to, R));
    }

    return ReturnStatus::Continue;
}

ReturnStatus Node::quiescence() {
    assertOk();
    assert (!inCheck());

    // stand pat
    score = cEval;
    if (beta <= score) {
        assert (currentMove.none());
        return ReturnStatus::Cutoff;
    }
    if (alpha < score) {
        alpha = score;
        child().beta = -alpha;
    }

    assert (child().alpha == -beta);
    assert (child().beta == -alpha);

    // impossible to capture the king, do not even try to save time
    return goodCaptures(OP.nonKing());
}

ReturnStatus Node::goodCaptures(PiMask victims) {
    // queen promotion moves, with and without capture
    for (Pi pi : MY.promotables()) {
        Bb queenPromos = bbMovesOf(pi) & Bb{Rank8}; // filter out underpromotions
        Square from{MY.sq(pi)};
        for (Square to : queenPromos) {
            if (!safeForOp(to) || OP.bbSide().has(~to)) {
                // move to safe square or always good promotion with capture
                RETURN_CUTOFF (searchMove(from, to, 1_ply, CanBeKiller::No));
            }
        }
    }

    // MVV (most valuable victim) order
    for (Pi victim : victims) {
        Square to = ~OP.sq(victim);

        // exclude underpromotions, should be no queen promotions anymore
        PiMask attackers = canMoveTo(to) % MY.promotables();
        if (attackers.none()) { continue; }

        // simple SEE function, checks only two cases:
        // 1) victim defended by at least one pawn
        // 2) attackers does not outnumber defenders (not precise, but effective condition)
        // then prune captures by more valuable attackers
        // the rest uncertain captures considered good enough to seek in QS
        //TODO: check for pinned attackers and defenders
        //TODO: check if bad capture makes discovered check
        //TODO: check if defending pawn is pinned and cannot recapture
        //TODO: try killer heuristics for uncertain and bad captures
        if (OP.bbPawnAttacks().has(~to) || !safeForMe(to)) {
            attackers &= MY.lessOrEqualValue(OP.typeOf(victim));
        }

        while (attackers.any()) {
            // LVA (least valuable attacker) order
            Pi pi = attackers.piLast(); attackers -= PiMask{pi};
            Square from{MY.sq(pi)};
            RETURN_CUTOFF (searchMove(from, to, 1_ply, CanBeKiller::No));
        }
    }

    return ReturnStatus::Continue;
}

ReturnStatus Node::searchNullMove() {
    RETURN_IF_STOP (The_uci.limits.countNode());

    //TRICK: null move not counted as movesMade()
    currentMove = {};
    child().childNullMove();

    return negamax(4_ply + (depth-2_ply)/4);
}

void Node::childNullMove() {
    makeNullMove(parent());
    childZHash = {};
    tt = The_uci.tt.prefetch<TtSlot>(z());
}

ReturnStatus Node::searchMove(Move move, Ply R) {
    RETURN_IF_STOP (The_uci.limits.countNode());

    assert (move.any());
    assert (isPseudoLegal(move));
    assert (isPossibleMove(move));

    Square from{move.from()};
    Square to{move.to()};

    currentMove = move;
    clearMove(from, to);
    child().childMove(from, to);

    return negamax(finalR(R));
}

void Node::childMove(Square from, Square to) {
    bool shouldResetZHash = makeMove(parent(), from, to, parent().childZHash, [&](Z z) {
        tt = The_uci.tt.prefetch<TtSlot>(z);
    });

    childZHash = ply <= 1_ply || shouldResetZHash ? ZHash{} : ZHash{parent().zHash(), parent().z()};
    The_uci.pv.clear(pvIndex);
}

constexpr Ply Node::finalR(Ply R) const {
    if (R <= 1_ply) { return R; }
    if (inCheck()) { return depth >= 6_ply ? 2_ply : 1_ply; } // plus check extension

    return baseR + R;
}

// counter and folloup move heuristic
ReturnStatus Node::contMove(ContIndex::_t ContType, Move move) {
    for (auto i : range<decltype(The_uci.contMoves)::Index>()) {
        auto contMove = The_uci.contMoves.get(ContType, i, colorToMove(), move);
        if (contMove.none()) { break; } // insert_unique_compact() garantees no holes
        if (isPossibleMove(contMove)) {
            return searchMove(contMove);
        }
    }
    return ReturnStatus::Continue;
}

constexpr Move Node::counterMove() const {
    return hasParent() ? parent().currentMove : Move{};
}

constexpr Move Node::followupMove() const {
    return hasGrandParent() ? grandParent().currentMove : Move{};
}

void Node::saveNode() {
    assert (bestMove.none() || isPseudoLegal(bestMove));
    assert ((inCheck() && eval.none()) || (!inCheck() && eval.isEval() /*&& eval == evaluate()*/));
    assert (score.isOk(ply));
    assert (tt);

    if (depth == 0_ply) { return; }

    *tt = TtSlot{ z(), eval, score.tt(ply), bound, depth, bestMove.ttMove(), The_uci.ttAge };
    ++The_uci.tt.writes;
}

void Node::saveHistory() {
    saveNode();

    if (bestMove.none() || bestMove.canBeKiller() == CanBeKiller::No) { return; }

    if (inCheck()) {
        if (hasParent()) {
            assert (parent().currentMove.any());
            The_uci.checkMoves.set(colorToMove(), MY.sqKing(), parent().currentMove, bestMove);
        }
        return;
    }

    insert_unique_pos(killers, bestMove);

    if (!hasParent()) { return; } // ply-1

    bool isDeep{ depth > ply };

    if (counterMove().any()) {
        The_uci.contMoves.set(Counter, colorToMove(), counterMove(), bestMove);
        if (isDeep) {
            The_uci.contMoves.set(DeepCounter, colorToMove(), counterMove(), bestMove);
        }
    }

    if (!hasGrandParent()) { return; } // ply-2
    insert_unique_pos<1>(grandParent().killers, bestMove);
    if (followupMove().any()) {
        The_uci.contMoves.set(Followup, colorToMove(), followupMove(), bestMove);
        if (isDeep) {
            The_uci.contMoves.set(DeepFollowup, colorToMove(), followupMove(), bestMove);
        }
    }
}

constexpr Color Node::colorToMove() const { return The_uci.colorToMove(ply); }

// insufficient mate material
bool Node::isDrawMaterial() const {
    auto my = MY.material();
    auto op = OP.material();

    if (my.hasMatingPieces() || op.hasMatingPieces()) { return false; }

    auto myBishops = my.count(Bishop);
    auto opBishops = op.count(Bishop);

    // here both sides can have only minors pieces
    auto myMinors = my.count(Knight) + myBishops;
    auto opMinors = op.count(Knight) + opBishops;

    if (myMinors >= 2 || opMinors >= 2) { return false; }

    // lone minor cannot mate
    if (myMinors <= 1 && opMinors <= 1) { return true; }

    // myMinors:opMinors == 2:1 | 1:2 | 2:0 | 0:2
    if (myMinors == 2) {
        if (myBishops == 0) { return true; }
        if (myBishops == 1 && opMinors == 1) { return true; }
        if (myBishops == 2 && opBishops == 1) { return true; }
    } else {
        assert (opMinors == 2);
        if (opBishops == 0) { return true; }
        if (opBishops == 1 && myMinors == 1) { return true; }
        if (opBishops == 2 && myBishops == 1) { return true; }
    }

    return false;
}

bool Node::isRepetition() const {
    if (rule50() < 4_ply) { return false; }

    Z z{ this->z() };

    if (ply > 4_ply) {
        // search tree repetitions (2-fold is draw); ply and ply-2 cannot be chess position repetitions
        auto* next = &grandParent();
        while (!next->zHash().none(z)) {
            next = &next->grandParent();
            assert (next);
            if (next->z() == z) { return true; }
        }
        assert (next->ply > 0_ply);
    }

    // game history repetitions
    return rule50() >= ply && (isPv()
        ? The_uci.repetitions.has3(colorToMove(), z)
        : The_uci.repetitions.has2(colorToMove(), z)
    );
}

ReturnStatus Node::searchRoot(const PositionMoves& pos) {
    static_cast<PositionMoves&>(*this) = pos;
    killers = {};

    for (depth = 1_ply; depth.isOk(); ++depth) {
        tt = The_uci.tt.prefetch<TtSlot>(z());
        alpha = Score{MateLoss};
        beta = Score{MateWin};

        RETURN_IF_STOP (search());
        The_uci.pv.set(depth); // iteration fully completed

        RETURN_IF_STOP (The_uci.limits.iterationDeadlineReached());
        if (depth >= The_uci.limits.maxDepth()) { return ReturnStatus::Continue; }

        The_uci.info_pv();
        setMoves(The_uci.moves()); // refresh moves for next iteration
        The_uci.ttAge.nextAge();

        // refresh PV in TT in case it was overwritten
        if (The_uci.limits.getNodes() > 1'000000) { The_uci.savePv(); }
    }

    return ReturnStatus::Continue;
}
