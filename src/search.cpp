#include "search.hpp"
#include "Uci.hpp"
#include "Position_impl.hpp"

#define RETURN_CUTOFF(visitor) { ReturnStatus status = visitor; if (status != ReturnStatus::Continue) { return status; }} ((void)0)

void SearchLimits::assertNodesOk() const {
    assert (0 <= nodesQuota_); assert (nodesQuota_ < QuotaLimit);
    //assert (0 <= nodes);
    assert (nodes_ <= nodesLimit_);
    assert (static_cast<decltype(nodesLimit_)>(nodesQuota_) <= nodes_);
}

ReturnStatus SearchLimits::refreshQuota() const {
    assertNodesOk();

    // expected nodesQuata_ == 0
    nodes_ -= nodesQuota_;
    //nodesQuota_ = 0; // keeps invariant, but redundant

    auto nodesRemaining = nodesLimit_ - nodes_;
    if (nodesRemaining >= QuotaLimit) {
        nodesQuota_ = QuotaLimit;
    }
    else {
        nodesQuota_ = static_cast<decltype(nodesQuota_)>(nodesRemaining);
        if (nodesQuota_ == 0) {
            // `go nodes` limit reached
            assertNodesOk();
            return ReturnStatus::Stop;
        }
    }

    assert (0 < nodesQuota_); assert (nodesQuota_ <= QuotaLimit);
    nodes_ += nodesQuota_; // allocate new nodesQuota

    return lastDeadlineReached();
}

template <SearchLimits::time_quota_t TimeQuota>
ReturnStatus SearchLimits::reachedTime() const {
    if (stop_.load(std::memory_order_seq_cst)) { return ReturnStatus::Stop; } // unconditional stop
    if (timePool_ == UnlimitedTime || pondering_.load(std::memory_order_relaxed)) { return ReturnStatus::Continue; }
    if (getNodes() < QuotaLimit) { return ReturnStatus::Continue; } // avoid early time check throttling

    auto timePool = timePool_;
    if (timeStrategy_ != ExactTime) {
        int timeQuota = TimeQuota;
        if (TimeQuota != MaxQuota) { timeQuota += lowMaterialQuotaBonus_; }

        timePool *= +timeStrategy_ * timeQuota;
        timePool /= +HardMove * MaxQuota;
    }

    bool deadlineReached = timePool < ::elapsedSince(searchStartTime_);
    return deadlineReached ? ReturnStatus::Stop : ReturnStatus::Continue;
}
ReturnStatus SearchLimits::lastDeadlineReached() const { return reachedTime<MaxQuota>(); }
ReturnStatus SearchLimits::iterationDeadlineReached() const { return reachedTime<IterationQuota>(); }

ReturnStatus SearchLimits::updateTimeStrategy(const PrincipalVariation& pv) const {
    if (timeStrategy_ == ExactTime) { return ReturnStatus::Continue; }

    auto bestMove = pv.move(0_ply);
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

TtSlot::TtSlot (const Node* n) : TtSlot{
    n->z(),
    n->score,
    n->ply,
    n->bound,
    n->depth,
    n->currentMove.from(),
    n->currentMove.to(),
    n->canBeKiller
} {}

Node::Node (Node* p) :
    //TRICK: no need to clean up PositionMoves{}
    root{p->root}, parent{p}, grandParent{p->parent}, ply{p->ply + 1_ply},
    pvPly{p->isPv() ? ply : p->pvPly}, pvIndex{+p->pvIndex+1},
    alpha{-p->beta}, beta{-p->alpha}
{
    if (grandParent) {
        killer[0] = grandParent->killer[0];
        killer[1] = grandParent->killer[1];
    }
}

void Node::assertOk() const {
    assert (alpha < beta);
    if (score.any()) {
        assert (score < beta || bound == FailHigh);
        assert (alpha <= score || bound == FailLow);
    }
    assert (Score{MateLoss} <= alpha);
    assert (beta <= Score{MateWin});
}

ReturnStatus Node::negamax(Ply R) {
    assert (child);
    child->depth = depth - R; //TRICK: Ply >= 0
    /* assert (child->depth >= 0); */
    child->generateMoves();
    RETURN_IF_STOP (child->search());
    assertOk();

    auto childScore = -child->score;

    if (childScore <= alpha) {
        if (score.none() || score < childScore) {
            score = childScore;
        }
    } else {
        // do not use R param as actual reduction can differ
        auto childR = child->currentR();

        // full depth research (unless it was a null move search)
        if (currentMove.any() && childR >= 2_ply) {
            if (child->isPv()) {
                // rare case (the first move from PV with reduced depth)
                child->alpha = -beta;
                assert (child->beta == -alpha);
            } else {
                assert (child->alpha == child->beta.minus1());
            }
            return negamax();
        }

        if (beta <= childScore) {
            score = childScore;
            failHigh();
            return ReturnStatus::Cutoff;
        }

        assert (childR <= 1_ply);
        assert (alpha < childScore && childScore < beta);
        assert (isPv()); // alpha < childScore < beta, so current window cannot be zero
        assert (currentMove.any()); // null move in PV is not allowed

        if (!child->isPv()) {
            child->pvPly = child->ply;
            assert (child->isPv());
            child->alpha = -beta;
            assert (child->beta == -alpha);
            // Principal Variation Search (PVS) research with full window and full depth
            return negamax();
        }

        score = childScore;
        alpha = childScore;
        child->beta = -alpha;
        RETURN_IF_STOP (updatePv());
    }

    // set zero window for the next sibling move search
    child->alpha = child->beta.minus1(); // can be either 1 centipawn or 1 mate distance ply
    child->pvPly = pvPly;

    assert (child->beta == -alpha);
    return ReturnStatus::Continue;
}

ReturnStatus Node::search() {
    score = Score{NoScore};
    eval  = Score{NoScore};
    bound = FailLow;
    currentMove = {};
    assertOk();

    if (!isRoot()) {
        if (inCheck()) {
            if (movesTotal() == 0) {
                // checkmate
                score = Score::mateLoss(ply);
                assert (currentMove.none());
                return ReturnStatus::Continue;
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

    do {
        if (depth == 0_ply) {
            ttHit = false;
            break;
        }

        ++root.tt.reads;
        ttSlot = *tt;

        ttHit = (ttSlot == z());
        if (!ttHit) {
            break;
        }

        Bound ttBound = ttSlot.bound();
        Score ttScore = ttSlot.score(ply);
        if (ttScore.none()) {
            // cleared TT or collision
            ttHit = false;
            break;
        }

        bool ttHasMove = ttSlot.hasMove();
        Square ttFrom = ttSlot.from();
        Square ttTo = ttSlot.to();
        if (ttHasMove && !isPossibleMove(ttFrom, ttTo)) {
            // collision
            ttHit = false;
            break;
        }

        ++root.tt.hits;

        if (!isPv() && depth <= ttSlot.draft()) {
            if (ttBound == ExactScore
                || (ttBound == FailHigh && beta <= ttScore)
                || (ttBound == FailLow && ttScore <= alpha)
            ) {
                score = ttScore;
                bound = ttBound;
                if (ttHasMove) {
                    assert (isPossibleMove(ttFrom, ttTo));
                    canBeKiller = ttSlot.canBeKiller();
                    currentMove = HistoryMove{MY.typeAt(ttFrom), ttFrom, ttTo};
                } else {
                    canBeKiller = false;
                    assert (currentMove.none());
                }
                return ReturnStatus::Cutoff;
            }
        }

        if (!inCheck()) {
            eval = evaluate();
            if (ttScore.isEval() &&
                (ttBound == ExactScore
                    || (ttBound == FailHigh && eval <= ttScore)
                    || (ttBound == FailLow && ttScore <= eval)
                )
            ) {
                eval = ttScore;
            }
        }
    } while(false);
    if (!ttHit && !inCheck()) {
        eval = evaluate();
    }

    assert ((inCheck() && eval.none()) || (!inCheck() && eval.isEval()));

    if (ply == MaxPly) {
        // no room to search deeper
        score = inCheck() ? Score::mateLoss(ply) : eval;
        assert (currentMove.none());
        return ReturnStatus::Continue;
    }

// search all moves:

    // prepare empty child node to make moves into
    //TRICK: dangling child pointer is dangerous, but it can be useful during debug
    Node node{this};
    this->child = &node;

    if (depth <= 0_ply && !inCheck()) {
        assert (depth == 0_ply);
        return quiescence();
    }

    assert (currentMove.none());

    if (!isPv() && !inCheck()) {
        if (depth <= 3_ply) {
            auto delta = (depth == 1_ply) ? 50_cp : (depth == 2_ply) ? 150_cp : 200_cp;
            if (Score{MinEval} <= beta && beta <= eval-delta) {
                // Static Null Move Pruning (Reverse Futility Pruning)
                score = eval;
                assert (currentMove.none());
                return ReturnStatus::Cutoff;
            } else {
                delta = (depth == 1_ply) ? 50_cp : (depth == 2_ply) ? 250_cp : 350_cp;
                if (eval+delta < alpha && alpha <= Score{MaxEval}) {
                    // Razoring
                    return quiescence();
                }
            }
        }

        // Null Move Pruning
        if (
            depth >= 2_ply // overhead higher then gain at very low depth
            && MY.material().canNullMove() // avoid null move in late endgame
            && Score{MinEval} <= beta && beta <= eval
        ) {
            canBeKiller = false;
            RETURN_CUTOFF (searchNullMove());
        }
    }

    if (ttHit && ttSlot.hasMove()) {
        canBeKiller = ttSlot.canBeKiller();
        RETURN_CUTOFF (searchMove(ttSlot.from(), ttSlot.to()));
    }

    if (isRoot()) {
        canBeKiller = false; // rootBestMoves can be anything
        for (auto move : root.rootBestMoves) {
            if (move.none()) { break; }
            RETURN_CUTOFF (searchIfPossible(move.from(), move.to()));
        }
    }

    canBeKiller = false;
    RETURN_CUTOFF (goodCaptures(OP.nonKing()));
    canBeKiller = !inCheck();

    if (parent && !inCheck()) {
        RETURN_CUTOFF (searchIfPossible(parent->killer[0]));
        RETURN_CUTOFF (counterMove());
        RETURN_CUTOFF (followMove());

        RETURN_CUTOFF (searchIfPossible(parent->killer[1]));
        RETURN_CUTOFF (counterMove());
        RETURN_CUTOFF (followMove());

        RETURN_CUTOFF (searchIfPossible(parent->killer[2]));
    }

    do {
        // going to search only non-captures, mask out remaining unsafe captures to avoid redundant safety checks
        //TRICK: ~ is not a negate bitwise operation but byteswap -- flip opponent's bitboard
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
        //TODO: double push attacks
        Bb pawnsThreatsFrom = ((OP.bbSide() - OP.bbPawns()).pForwardDiag() % OP_OCCUPIED).pForward();
        Bb potentialAttackers = MY.bbPawns() & ~pawnsThreatsFrom;
        for (Square from : potentialAttackers) {
            Square to{from.file(), from.rank().forward()};
            if (!bbMovesOf(MY.pi(from)).has(to)) { continue; }
            if (safeForOp(to)) { continue; }
            RETURN_CUTOFF (searchMove(from, to, 2_ply));
        }

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
                RETURN_CUTOFF (searchMove(from, to, 4_ply));
            }
        }

        // all remaining pawn moves
        // losing queen promotions, all underpromotions
        // losing passed pawns moves, all non passed pawns moves
        for (Square from : MY.bbPawns()) {
            Pi pi = MY.pi(from);
            for (Square to : bbMovesOf(pi)) {
                RETURN_CUTOFF (searchMove(from, to, 4_ply));
            }
        }

        // unsafe (losing) captures (N/B, R, Q order)
        for (PiMask pieces = MY.officers(); pieces.any(); ) {
            Pi pi = pieces.piLast(); pieces -= PiMask{pi};
            Square from{MY.sq(pi)};
            for (Square to : bbMovesOf(pi) & ~OP.bbSide()) {
                RETURN_CUTOFF (searchMove(from, to, 4_ply));
            }
        }

        if (depth <= 4_ply && !inCheck() && (!isPv() || movesMade() > 0)) { break; }

        // unsafe (losing) non-captures (N/B, R, Q order)
        for (PiMask pieces = MY.officers(); pieces.any(); ) {
            Pi pi = pieces.piLast(); pieces -= PiMask{pi};
            Square from{MY.sq(pi)};
            for (Square to : bbMovesOf(pi)) {
                RETURN_CUTOFF (searchMove(from, to, 5_ply));
            }
        }
    } while (false);

    if (bound == FailLow) {
        if (movesMade() == 0) {
            assert (!inCheck());
            assert (currentMove.none());
            score = alpha;
            return ReturnStatus::Continue;
        }
        assert (score.isOk(ply));
        // fail low, no good move found, write back previous TT move if any
        currentMove = ttMove();
        *tt = TtSlot{this};
        ++root.tt.writes;
    }
    return ReturnStatus::Continue;
}

ReturnStatus Node::goodNonCaptures(Pi pi, Bb moves, Ply R) {
    PieceType ty = MY.typeOf(pi);
    assert (!ty.is(Pawn));
    PiMask opLessValue = OP.lessValue(ty);
    Square from{MY.sq(pi)};
    for (Square to : moves) {
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

    assert (child);

    // stand pat
    score = eval;
    if (beta <= score) {
        assert (currentMove.none());
        return ReturnStatus::Cutoff;
    }
    if (alpha < score) {
        alpha = score;
        child->beta = -alpha;
    }

    assert (child->alpha == -beta);
    assert (child->beta == -alpha);

    assert (child->alpha == -beta);
    assert (child->beta == -alpha);
    child->assertOk();

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
                RETURN_CUTOFF (searchMove(from, to));
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
            RETURN_CUTOFF (searchMove(from, to));
        }
    }

    return ReturnStatus::Continue;
}

// Counter move heuristic: refutation of the last opponent's move
ReturnStatus Node::counterMove() {
    assert (parent);
    if (parent->currentMove.any()) {
        for (auto i : range<decltype(root.counterMove)::Index>()) {
            auto move = root.counterMove.get(i, colorToMove(), parent->currentMove);
            if (move.none()) { break; }
            if (isPossibleMove(move)) {
                return searchMove(move.from(), move.to());
            }
        }
    }
    return ReturnStatus::Continue;
}

// Follow up move heuristic: continue the idea of our last made move
ReturnStatus Node::followMove() {
    if (grandParent && grandParent->currentMove.any()) {
        for (auto i : range<decltype(root.followMove)::Index>()) {
            auto move = root.followMove.get(i, colorToMove(), grandParent->currentMove);
            if (move.none()) { break; }
            if (isPossibleMove(move)) {
                return searchMove(move.from(), move.to());
            }
        }
    }
    return ReturnStatus::Continue;
}

ReturnStatus Node::searchNullMove() {
    RETURN_IF_STOP (root.limits.countNode());

    currentMove = {};
    child->childNullMove();

    return negamax(4_ply + (depth-2_ply)/4);
}

void Node::childNullMove() {
    makeNullMove(parent);
    tt = root.tt.prefetch<TtSlot>(z());
    zHash = {};
}

ReturnStatus Node::searchMove(Square from, Square to, Ply R) {
    RETURN_IF_STOP (root.limits.countNode());

    currentMove = HistoryMove{MY.typeAt(from), from, to};
    clearMove(from, to);
    child->childMove(from, to);

    return negamax(finalR(R));
}

void Node::childMove(Square from, Square to) {
    makeMove(parent, from, to, [&](Z z){ tt = root.tt.prefetch<TtSlot>(z); });
    root.pv.clear(pvIndex);

    if (rule50() < 2_ply || ply <= 2_ply) { zHash = {}; }
    else { zHash = ZHash{grandParent->zHash, grandParent->z()}; }
}

Ply Node::finalR(Ply R) const {
    if (R <= 1_ply) { return R; }
    if (inCheck()) { return 1_ply; }

    // depth adaptive reduction
    if (depth <= 8_ply && R >= 4_ply) { R = R - 1_ply; }

    return R;
}

void Node::failHigh() {
    // currentMove is null (after NMP), write back previous TT move instead
    if (currentMove.none()) {
        currentMove = ttMove();
    }

    assert (score.isOk(ply));
    if (depth > 0_ply) {
        bound = FailHigh;
        *tt = TtSlot{this};
        ++root.tt.writes;
    }

    if (canBeKiller) {
        updateHistory(currentMove);
    }
}

ReturnStatus Node::updatePv() {
    assert (isPseudoLegal(currentMove));

    if (depth > 0_ply) {
        bound = ExactScore;
        *tt = TtSlot{this};
        ++root.tt.writes;
    }

    if (canBeKiller) {
        updateHistory(currentMove);
    }

    auto bestMove = uciMove(currentMove.from(), currentMove.to());
    if (!isRoot()) {
        child->pvIndex = root.pv.set(pvIndex, bestMove, child->pvIndex);
    } else {
        // unfinished iteration, so report depth-1
        pvIndex = root.pv.set(depth - 1_ply, score, bestMove, child->pvIndex);
        child->pvIndex = PrincipalVariation::Index{+pvIndex+1};

        RETURN_IF_STOP (root.limits.updateTimeStrategy(root.pv));

        ::insert_unique(root.rootBestMoves, bestMove);
        if (depth > 1_ply) { root.info_pv(); }
    }

    return ReturnStatus::Continue;
}

void Node::updateHistory(HistoryMove historyMove) const {
    assert (isPseudoLegal(historyMove));
    if (!parent) { return; }

    insert_unique(parent->killer, historyMove);
    if (parent->grandParent) {
        insert_unique<2>(parent->grandParent->killer, historyMove);
    }

    if (parent->currentMove.any()) {
        root.counterMove.set(colorToMove(), parent->currentMove, historyMove);
    }

    if (grandParent && grandParent->currentMove.any()) {
        root.followMove.set(colorToMove(), grandParent->currentMove, historyMove);
    }
}

constexpr Color Node::colorToMove() const { return root.colorToMove(ply); }

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
        auto next = grandParent;
        while (!next->zHash.none(z)) {
            next = next->grandParent;
            assert (next);
            if (next->z() == z) { return true; }
        }
        assert (next->ply > 0_ply);
    }

    // game history repetitions
    return rule50() >= ply && root.repetitions.has(colorToMove(), z);
}

void refreshTtPv(const PositionMoves& p, const PrincipalVariation& pv, const Tt& tt) {
    // clone position
    PositionMoves pos{p};

    Ply   ply   = 0_ply;
    Ply   depth = pv.depth();
    Score score = pv.score();
    auto  pmoves = pv.moves();

    for (UciMove move; (move = *pmoves++).any();) {
        assert (score.isOk(ply));
        assert ((pos.generateMoves(), pos.isPossibleMove(move.from(), move.to())));

        auto o = tt.addr<TtSlot>(pos.z());
        *o = TtSlot{pos.z(), score, ply, ExactScore, depth, move.from(), move.to(), false};
        ++tt.writes;

        //we cannot use makeZobrist() because of en passant legality validation
        pos.makeMoveNoEval(move.from(), move.to());
        score = -score;
        depth = depth - 1_ply;
        ply = ply + 1_ply;

        if (depth == 0_ply) { break; }
    }
}

ReturnStatus Node::searchRoot() {
    for (depth = 1_ply; depth.isOk(); ++depth) {
        tt = root.tt.prefetch<TtSlot>(z());
        alpha = Score{MateLoss};
        beta = Score{MateWin};

        RETURN_IF_STOP (search());
        root.pv.set(depth); // iteration fully completed

        RETURN_IF_STOP (root.limits.iterationDeadlineReached());
        if (depth >= root.limits.maxDepth()) { return ReturnStatus::Continue; }

        root.info_pv();
        setMoves(root.moves()); // refresh moves for next iteration
        ::refreshTtPv(*this, root.pv, root.tt);
    }

    return ReturnStatus::Continue;
}
