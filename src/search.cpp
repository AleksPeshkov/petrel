#include "search.hpp"
#include "Uci.hpp"

#define RETURN_CUTOFF(visitor) { ReturnStatus status = visitor; if (status != ReturnStatus::Continue) { return status; }} ((void)0)

constexpr void UciSearchLimits::assertNodesOk() const {
    assert (0 <= nodesQuota_);
    assert (nodesQuota_ < QuotaLimit);
    //assert (0 <= nodes);
    assert (nodes_ <= nodesLimit_);
    assert (static_cast<decltype(nodesLimit_)>(nodesQuota_) <= nodes_);
}

template <deadline_t Deadline>
bool UciSearchLimits::reached() const {
    if (isStopped()) { return true; }
    if (nodes_ == 0 || deadline_ == NoDeadline || ponder_.load(std::memory_order_relaxed)) { return false; }
    if (moveComplexity == MoveTime && Deadline != HardDeadline) { return false; }

    TimeInterval current = deadline_;
    if (moveComplexity != MoveTime) {
        current *= static_cast<int>(moveComplexity) * Deadline;
        current /= static_cast<int>(NormalMove) * HardDeadline;
    }

    bool isDeadlineReached = current < elapsedSinceStart();
    if (isDeadlineReached) {
        nodesLimit_ = nodes_;
        nodesQuota_ = 0;
        assertNodesOk();
        stop_.store(true, std::memory_order_release);
    }
    return isDeadlineReached;
}

ReturnStatus UciSearchLimits::refreshQuota() const {
    assertNodesOk();
    nodes_ -= nodesQuota_;

    auto nodesRemaining = nodesLimit_ - nodes_;
    if (nodesRemaining >= QuotaLimit) {
        nodesQuota_ = QuotaLimit;
    }
    else {
        nodesQuota_ = static_cast<decltype(nodesQuota_)>(nodesRemaining);
        if (nodesQuota_ == 0) {
            assertNodesOk();
            return ReturnStatus::Stop;
        }
    }

    if (reached<HardDeadline>()) {
        return ReturnStatus::Stop;
    }

    assert (0 < nodesQuota_ && nodesQuota_ <= QuotaLimit);
    nodes_ += nodesQuota_;
    --nodesQuota_; //count current node

    assertNodesOk();
    return ReturnStatus::Continue;
}

ReturnStatus UciSearchLimits::countNode() const {
    assertNodesOk();

    if (nodesQuota_ == 0 || isStopped()) {
        return refreshQuota();
    }

    assert (nodesQuota_ > 0);
    --nodesQuota_;

    assertNodesOk();
    return ReturnStatus::Continue;
}

void UciSearchLimits::updateMoveComplexity(UciMove bestMove) const {
    if (moveComplexity == MoveTime) { return; }

    if (!easyMove) {
        // Easy Move: root best move never changed
        easyMove = bestMove;
    } else if (easyMove != bestMove) {
        easyMove = bestMove;
        // Hard Move: root best move just have changed
        moveComplexity = HardMove;
    } else if (moveComplexity == HardMove) {
        // Normal Move: root best move have not changed during last two iterations
        moveComplexity = NormalMove;
    }
}

TtSlot::TtSlot (const Node* n) : TtSlot{
    n->zobrist(),
    n->score,
    n->ply,
    n->bound,
    n->depth,
    n->currentMove.from(),
    n->currentMove.to(),
    n->canBeKiller
} {}

Node::Node (const PositionMoves& p, const Uci& r) :
    PositionMoves{p}, root{r}, parent{nullptr}, grandParent{nullptr}, ply{0}, pvAncestor{0_ply},
    alpha{MateLoss}, beta{MateWin}, pvIndex{0}
{}

Node::Node (const Node* p) :
    PositionMoves{}, root{p->root}, parent{p}, grandParent{p->parent},
    ply{p->ply + 1}, pvAncestor{p->isPv() ? ply : p->pvAncestor},
    alpha{-p->beta}, beta{-p->alpha}, pvIndex{p->pvIndex+1}
{
    if (grandParent) {
        killer[0] = grandParent->killer[0];
        killer[1] = grandParent->killer[1];
    }
}

ReturnStatus Node::negamax(Ply R) const {
    assert (child);
    child->depth = Ply{depth - R}; //TRICK: Ply >= 0
    /* assert (child->depth >= 0); */
    child->generateMoves();
    RETURN_IF_STOP (child->search());
    assertOk();

    auto childScore = -child->score;

    if (childScore <= alpha) {
        if (score < childScore) {
            score = childScore;
        }
    } else {
        if (beta <= childScore) {
            if (currentMove && depth > child->depth+1) {
                if (child->isPv()) {
                    // rare case
                    child->alpha = -beta;
                    assert (child->beta == -alpha);
                } else {
                    assert (child->alpha == child->beta.minus1());
                }
                // full depth research (unless it was a null move search or leaf node)
                return negamax();
            }

            score = childScore;
            failHigh();
            return ReturnStatus::BetaCutoff;
        }

        assert (alpha < childScore && childScore < beta);
        assert (isPv()); // alpha < childScore < beta, so current window cannot be zero
        assert (currentMove); // null move in PV is not allowed

        if (!child->isPv()) {
            child->pvAncestor = child->ply;
            assert (child->isPv());
            child->alpha = -beta;
            assert (child->beta == -alpha);
            // Principal Variation Search (PVS) research with full window and full depth
            return negamax();
        }

        score = childScore;
        alpha = childScore;
        child->beta = -alpha;
        child->pvIndex = root.pvMoves.set(pvIndex, uciMove(currentMove.from(), currentMove.to()), child->pvIndex);
        RETURN_IF_STOP (updatePv());
    }

    // set zero window for the next sibling move search
    child->alpha = child->beta.minus1(); // can be either 1 centipawn or 1 mate distance ply
    child->pvAncestor = pvAncestor;

    assert (child->beta == -alpha);
    return ReturnStatus::Continue;
}

ReturnStatus Node::search() {
    score = Score{NoScore};
    currentMove = {};
    bound = FailLow;
    assertOk();

    if (moves().none()) {
        // checkmate or stalemate
        score = inCheck() ? Score::mateLoss(ply) : Score{DrawScore};
        assert (!currentMove);
        return ReturnStatus::Continue;
    }
    if (ply == MaxPly) {
        // no room to search deeper
        score = evaluate();
        assert (!currentMove);
        return ReturnStatus::Continue;
    }

    if (parent) {
        assert (ply >= 1);

        // mate-distance pruning
        alpha = std::max(alpha, Score::mateLoss(ply));
        if (!(alpha < std::min(beta, Score::mateWin(Ply{ply+1})))) {
            score = alpha;
            assert (!currentMove);
            return ReturnStatus::BetaCutoff;
        }

        if (rule50().isDraw() || isRepetition() || isDrawMaterial()) {
            score = Score{DrawScore};
            assert (!currentMove);
            return ReturnStatus::Continue;
        }

        if (inCheck()) {
            // check extension
            depth = Ply{depth+1};
        }
    }

    if (depth > 0) {
        ++root.tt.reads;
        ttSlot = *tt;
        isHit = (ttSlot == zobrist());
        if (isHit) {
            Square from = ttSlot.from();
            Square to = ttSlot.to();
            if (ttSlot.hasMove() && !isPossibleMove(from, to)) {
                isHit = false;
            } else {
                ++root.tt.hits;

                if (ttSlot.draft() >= depth && !isPv()) {
                    Bound ttBound = ttSlot.bound();
                    Score ttScore = ttSlot.score(ply);

                    //TODO: refresh TT record if age is old
                    if (
                        ((ttBound & FailHigh) && beta <= ttScore)
                        || ((ttBound & FailLow) && ttScore <= alpha)
                    ) {
                        score = ttScore;
                        assert (!currentMove);
                        if (ttSlot.hasMove()) {
                            assert (isPossibleMove(from, to));
                            canBeKiller = ttSlot.canBeKiller();
                            currentMove = HistoryMove{MY.typeAt(from), from, to};
                        }
                        return beta <= score ? ReturnStatus::BetaCutoff : ReturnStatus::Continue;
                    }
                }
            }
        }
    }

    // prepare empty child node to make moves into
    Node node{this};
    this->child = &node;

    auto returnStatus = searchMoves();

    this->child = nullptr; // better safe than sorry
    return returnStatus;
}

ReturnStatus Node::searchMoves() {
    assert (child);

    // skip costly evaluate() while in check, as it should not be used
    eval = !inCheck() ? evaluate() : Score::mateLoss(ply);

    if (depth <= 0 && !inCheck()) {
        assert (depth == 0);
        return quiescence();
    }

    assert (!currentMove);

    if (
        !inCheck()
        && !isPv()
        && depth <= 3
    ) {
        auto delta = (depth == 1) ? 50_cp : (depth == 2) ? 150_cp : 200_cp;
        if (Score{MinEval} <= beta && beta <= eval-delta) {
            // Static Null Move Pruning (Reverse Futility Pruning)
            score = eval;
            assert (!currentMove);
            return ReturnStatus::BetaCutoff;
        } else {
            delta = (depth == 1) ? 50_cp : (depth == 2) ? 250_cp : 350_cp;
            if (eval+delta < alpha && alpha <= Score{MaxEval}) {
                // Razoring
                return quiescence();
            }
        }
    }

    // Null Move Pruning
    if (
        !inCheck()
        && !isPv()
        && Score{MinEval} <= beta && beta <= eval
        && depth >= 2 // overhead higher then gain at very low depth
        && MY.evaluation().piecesMat() > 0 // no null move if only pawns left (zugzwang)
    ) {
        canBeKiller = false;
        RETURN_CUTOFF (child->searchNullMove(Ply{4 + (depth-2)/4}));
    }

    if (isHit && ttSlot.hasMove()) {
        canBeKiller = ttSlot.canBeKiller();
        RETURN_CUTOFF (child->searchMove(ttSlot.from(), ttSlot.to()));
    }

    if (!parent) {
        assert (ply == 0);
        canBeKiller = true;
        for (auto move : root.rootBestMoves) {
            if (!move) { break; }
            RETURN_CUTOFF (child->searchIfPossible(move.from(), move.to()));
        }
    }

    canBeKiller = false;
    RETURN_CUTOFF (goodCaptures(OP.nonKing()));
    canBeKiller = !inCheck();

    if (parent && !inCheck()) {
        RETURN_CUTOFF (child->searchIfPossible(parent->killer[0]));
        RETURN_CUTOFF (counterMove());
        RETURN_CUTOFF (followMove());

        RETURN_CUTOFF (child->searchIfPossible(parent->killer[1]));
        RETURN_CUTOFF (counterMove());
        RETURN_CUTOFF (followMove());

        RETURN_CUTOFF (child->searchIfPossible(parent->killer[2]));
    }

    {
        // Weak Move Pruning: !inCheck() && !isPv()
        bool canP1 = !inCheck() && !isPv() && depth <= 1;
        bool canP2 = !inCheck() && !isPv() && depth <= 2;
        bool canP4 = !inCheck() && !isPv() && depth <= 4;

        {
            // going to search only non-captures, mask out remaining unsafe captures to avoid redundant safety checks
            //TRICK: ~ is not a negate bitwise operation but byteswap -- flip opponent's bitboard
            Bb badSquares = ~(OP.bbPawnAttacks() | OP.bbSide());
            PiMask safePieces = {}; // pieces on safe squares

            // officers (Q, R, B/N order) moves from unsafe to safe squares
            for (Pi pi : MY.officers()) {
                Square from = MY.squareOf(pi);

                if (!bbAttacked().has(from)) {
                    // piece is not attacked at all
                    safePieces += pi;
                    continue;
                }

                assert (OP.attackersTo(~from).any());

                if ((OP.attackersTo(~from) & OP.lessOrEqualValue(MY.typeOf(pi))).none()) {
                    // attacked by more valuable attacker

                    if (MY.bbPawnAttacks().has(from) || safeForMe(from)) {
                        // piece is protected
                        safePieces += pi;
                        continue;
                    }
                    //TODO: try protecting moves of other pieces
                }

                RETURN_CUTOFF (goodNonCaptures(pi, bbMovesOf(pi) % badSquares, 2_ply));
            }

            // safe passed pawns moves
            for (Square from : bbPassedPawns() % Bb{Rank7}) {
                Pi pi = MY.piAt(from);
                for (Square to : bbMovesOf(pi)) {
                    if (safeForMe(to)) {
                        RETURN_CUTOFF (child->searchMove(from, to, from.on(Rank6) ? 1_ply : 2_ply));
                    }
                }
            }

            // safe officers moves
            if (!canP1 || movesMade() == 0) { // weak move pruning
                while (safePieces.any()) {
                    Pi pi = safePieces.piLeastValuable(); safePieces -= pi;
                    RETURN_CUTOFF (goodNonCaptures(pi, bbMovesOf(pi) % badSquares, 3_ply));
                }
            }
        }

        // all remaining pawn moves
        // losing queen promotions, all underpromotions
        // losing passed pawns moves, all non passed pawns moves
        for (Square from : MY.bbPawns()) {
            Pi pi = MY.piAt(from);
            for (Square to : bbMovesOf(pi)) {
                RETURN_CUTOFF (child->searchMove(from, to, 3_ply));
            }
        }

        // king quiet moves (always safe), castling is a rook move
        if (!canP2 || movesMade() == 0) { // weak move pruning
            for (Square to : bbMovesOf(Pi{TheKing})) {
                RETURN_CUTOFF (child->searchMove(Pi{TheKing}, to, 3_ply));
            }
        }

        // unsafe (losing) captures (N/B, R, Q order)
        for (PiMask pieces = MY.officers(); pieces.any(); ) {
            Pi pi = pieces.piLeastValuable(); pieces -= pi;
            for (Square to : bbMovesOf(pi) & ~OP.bbSide()) {
                RETURN_CUTOFF (child->searchMove(pi, to, 3_ply));
            }
        }

        // unsafe (losing) non-captures (N/B, R, Q order)
        if (!canP4 || movesMade() == 0) { // weak move pruning
            for (PiMask pieces = MY.officers(); pieces.any(); ) {
                Pi pi = pieces.piLeastValuable(); pieces -= pi;
                for (Square to : bbMovesOf(pi)) {
                    RETURN_CUTOFF (child->searchMove(pi, to, 4_ply));
                }
            }
        }
    }

    if (bound == FailLow) {
        // fail low, no good move found, write back previous TT move if any
        currentMove = ttMove();
        *tt = TtSlot(this);
        ++root.tt.writes;
    }
    return ReturnStatus::Continue;
}

ReturnStatus Node::goodNonCaptures(Pi pi, Bb moves, Ply R) {
    PieceType ty = MY.typeOf(pi);
    assert (!ty.is(Pawn));
    PiMask opLessValue = OP.lessValue(ty);

    for (Square to : moves) {
        assert (!OP.bbPawnAttacks().has(~to));
        assert (isNonCapture(pi, to));

        if (bbAttacked().has(to)) {
            if ((OP.attackersTo(~to) & opLessValue).any()) {
                // square defended by less valued opponent's piece
                continue;
            }

            if (!MY.bbPawnAttacks().has(to) && safeForOp(to)) {
                // skip move to the defended square
                continue;
            }
        }

        RETURN_CUTOFF (child->searchMove(pi, to, R));
    }

    return ReturnStatus::Continue;
}

ReturnStatus Node::quiescence() {
    assertOk();
    assert (!inCheck());

    assert (child);
    child->assertOk();

    // stand pat
    score = eval;
    if (beta <= score) {
        assert (!currentMove);
        return ReturnStatus::BetaCutoff;
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
        for (Square to : queenPromos) {
            if (safeForMe(to) || OP.bbSide().has(~to)) {
                // move to safe square or always good promotion with capture
                RETURN_CUTOFF (child->searchMove(pi, to));
            }
        }
    }

    // MVV (most valuable victim) order
    for (Pi victim : victims) {
        Square to = ~OP.squareOf(victim);

        // exclude underpromotions, should be no queen promotions anymore
        PiMask attackers = canMoveTo(to) % MY.promotables();
        if (attackers.none()) { continue; }

        // simple SEE function, checks only two cases:
        // 1) victim defended by at least one pawn
        // 2) attackers does not outnumber defenders (not precise, but effective condition)
        // then prune captures by more valuable attackers
        // the rest uncertain captures considered good enough to seek in QS
        //TODO: check for X-Ray attackers and defenders
        //TODO: check if bad capture makes discovered check
        //TODO: check if defending pawn is pinned and cannot recapture
        //TODO: try killer heuristics for uncertain and bad captures
        //TODO: try more complex and precise SEE
        if (OP.bbPawnAttacks().has(~to) || safeForOp(to)) {
            attackers &= MY.lessOrEqualValue(OP.typeOf(victim));
        }

        while (attackers.any()) {
            // LVA (least valuable attacker) order
            Pi pi = attackers.piLeastValuable(); attackers -= pi;

            RETURN_CUTOFF (child->searchMove(pi, to));
        }
    }

    return ReturnStatus::Continue;
}

// Counter move heuristic: refutation of the last opponent's move
ReturnStatus Node::counterMove() {
    if (parent && parent->currentMove) {
        for (auto i : range<decltype(root.counterMove)::Index>()) {
            auto move = root.counterMove.get(i, parent->colorToMove(), parent->currentMove);
            if (!move) { break; }
            if (isPossibleMove(move)) {
                return child->searchMove(move);
            }
        }
    }
    return ReturnStatus::Continue;
}

// Follow up move heuristic: continue the idea of our last made move
ReturnStatus Node::followMove() {
    if (grandParent && grandParent->currentMove) {
        for (auto i : range<decltype(root.followMove)::Index>()) {
            auto move = root.followMove.get(i, grandParent->colorToMove(), grandParent->currentMove);
            if (!move) { break; }
            if (isPossibleMove(move)) {
                return child->searchMove(move);
            }
        }
    }
    return ReturnStatus::Continue;
}

ReturnStatus Node::searchNullMove(Ply R) {
    RETURN_IF_STOP (root.limits.countNode());

    parent->currentMove = {};
    makeNullMove(parent);

    tt = root.tt.prefetch<TtSlot>(zobrist());
    repetitionHash = RepetitionHash{};

    return parent->negamax(R);
}

void Node::makeMove(Square from, Square to) {
    Position::makeMove(parent, from, to);
    tt = root.tt.prefetch<TtSlot>(zobrist());
    root.pvMoves.clearPly(pvIndex);
}

ReturnStatus Node::searchMove(Pi pi, Square to, Ply R) {
    return searchMove(parent->MY.squareOf(pi), to, R);
}

ReturnStatus Node::searchMove(Square from, Square to, Ply R) {
    RETURN_IF_STOP (root.limits.countNode());

    parent->clearMove(from, to);
    parent->currentMove = HistoryMove{parent->MY.typeAt(from), from, to};
    makeMove(from, to);

    if (rule50() < 2_ply) { repetitionHash = {}; }
    else if (grandParent) { repetitionHash = RepetitionHash{grandParent->repetitionHash, grandParent->zobrist()}; }
    else { repetitionHash = root.repetitions.repetitionHash(colorToMove()); }

    return parent->negamax(parent->inCheck() ? 1_ply : R); // check extension
}

void Node::failHigh() const {
    // currentMove is null (after NMP), write back previous TT move instead
    if (!currentMove) {
        currentMove = ttMove();
    }

    if (depth > 0) {
        bound = FailHigh;
        *tt = TtSlot{this};
        ++root.tt.writes;
    }

    if (parent && canBeKiller) {
        assert (currentMove);
        parent->updateHistory(currentMove);
    }
}

ReturnStatus Node::updatePv() const {
    if (depth > 0) {
        bound = ExactScore;
        *tt = TtSlot{this};
        ++root.tt.writes;
    }

    if (parent && canBeKiller) {
        assert (currentMove);
        parent->updateHistory(currentMove);
    }

    if (ply == 0) {
        const auto& bestMove = root.pvMoves[0];
        root.limits.updateMoveComplexity(bestMove);
        ::insert_unique(root.rootBestMoves, bestMove);

        root.pvScore = score;
        root.info_pv(depth);

        // good place to check as there are no wasted search nodes
        // and HardDeadline just possibly changed
        if (root.limits.reached<HardDeadline>()) {
            return ReturnStatus::Stop;
        }
    }

    return ReturnStatus::Continue;
}

void Node::updateHistory(HistoryMove historyMove) const {
    insert_unique(killer, historyMove);

    if (grandParent) {
        insert_unique<2>(grandParent->killer, historyMove);
    }

    if (currentMove) {
        root.counterMove.set(colorToMove(), currentMove, historyMove);
    }

    if (parent && parent->currentMove) {
        root.followMove.set(parent->colorToMove(),  parent->currentMove, historyMove);
    }
}

constexpr Color Node::colorToMove() const { return root.colorToMove(ply); }

// insufficient mate material
bool Node::isDrawMaterial() const {
    auto& my = MY.evaluation();
    auto& op = OP.evaluation();

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

    auto& z = zobrist();

    if (grandParent) {
        auto next = grandParent;
        while ((next = next->grandParent)) {
            if (next->zobrist() == z) {
                return true;
            }
            if (!next->repetitionHash.has(z)) {
                return false;
            }
        }
    }

    return root.repetitions.has(colorToMove(), z);
}

ReturnStatus Node::searchRoot() {
    auto rootMovesClone = moves();
    repetitionHash = root.repetitions.repetitionHash(colorToMove());

    Ply maxDepth = root.limits.maxDepth();

    auto moveCount = rootMovesClone.popcount();
    if (moveCount == 0) {
        root.pvScore = inCheck() ? Score::mateLoss(0_ply) : Score{DrawScore};
        return ReturnStatus::Continue;
    } else if (moveCount == 1) {
        // minimal search to get score and ponder move
        maxDepth = root.limits.canPonder ? 2_ply : 1_ply;
    }

    for (depth = 1_ply; depth <= maxDepth; ++depth) {
        tt = root.tt.prefetch<TtSlot>(zobrist());
        setMoves(rootMovesClone);
        alpha = Score{MateLoss};
        beta = Score{MateWin};
        auto returnStatus = search();

        root.newIteration();
        root.refreshTtPv(depth);

        RETURN_IF_STOP (returnStatus);

        if (root.limits.reached<IterationDeadline>()) { return ReturnStatus::Stop; }
    }

    return ReturnStatus::Continue;
}
