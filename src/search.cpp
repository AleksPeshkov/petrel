#include "search.hpp"
#include "Uci.hpp"

#define RETURN_CUTOFF(visitor) { ReturnStatus status = visitor; if (status != ReturnStatus::Continue) { return status; }} ((void)0)

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
    alpha{MinusInfinity}, beta{PlusInfinity}, pvIndex{0}
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

ReturnStatus Node::negamax(Node* child, Ply R) const {
    child->depth = Ply{depth - R}; //TRICK: Ply >= 0
    /* assert (child->depth >= 0); */
    child->generateMoves();
    RETURN_IF_STOP (child->search());

    assert (Score{MinusInfinity} <= alpha && alpha < beta && beta <= Score{PlusInfinity});

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
                // reduced search full depth research (unless it was a null move search or leaf node)
                return negamax(child);
            }

            score = childScore;
            failHigh();
            return ReturnStatus::BetaCutoff;
        }

        assert (alpha < childScore && childScore < beta);
        assert (isPv()); // alpha < childScore < beta, so current window cannot be zero
        assert (currentMove); // null move in PV is not allowed

        if (!child->isPv()) {
            // Principal Variation Search (PVS) research with full window
            child->pvAncestor = child->ply;
            assert (child->isPv());
            child->alpha = -beta;
            assert (child->beta == -alpha);
            return negamax(child);
        }

        score = childScore;
        alpha = childScore;
        child->beta = -alpha;
        child->pvIndex = root.pvMoves.set(pvIndex, uciMove(currentMove.from(), currentMove.to()), child->pvIndex);
        updatePv();
    }

    if (ply == 0 && depth > 1 && root.limits.reached<RootMoveDeadline>()) {
        return ReturnStatus::Stop;
    }

    // set zero window for the next sibling move search
    child->alpha = child->beta.minus1(); // can be either 1 centipawn or 1 mate distance ply
    child->pvAncestor = pvAncestor;

    assert (child->beta == -alpha);
    return ReturnStatus::Continue;
}

ReturnStatus Node::search() {
    assert (Score{MinusInfinity} <= alpha && alpha < beta && beta <= Score{PlusInfinity});
    score = Score{NoScore};
    currentMove = {};
    bound = FailLow;

    if (moves().none()) {
        // checkmate or stalemate
        score = inCheck() ? Score::checkmated(ply) : Score{DrawScore};
        assert (!currentMove);
        return ReturnStatus::Continue;
    }

    if (parent) {
        assert (ply >= 1);

        // mate-distance pruning
        alpha = std::max(alpha, Score::checkmated(ply));
        beta  = std::min(beta, -Score::checkmated(ply) + Ply{1});
        if (!(alpha < beta)) {
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

    if (depth <= 0 && !inCheck()) {
        assert (depth == 0);
        eval = evaluate();
        return quiescence();
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

    assert (!currentMove);

    eval = evaluate();

    if (ply == MaxPly) {
        // no room to search deeper
        score = eval;
        assert (!currentMove);
        return ReturnStatus::Continue;
    }

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

    // prepare empty child node to make moves into
    Node node{this};
    const auto child = &node;

    // Null Move Pruning
    if (
        !inCheck()
        && !isPv()
        && Score{MinEval} <= beta && beta <= eval
        && depth >= 2 // overhead higher then gain at very low depth
        && MY.evaluation().piecesMat() > 0 // no null move if only pawns left (zugzwang)
    ) {
        canBeKiller = false;
        RETURN_CUTOFF (child->searchNullMove(Ply{3 + depth/6}));
    }

    if (isHit && ttSlot.hasMove()) {
        canBeKiller = ttSlot.canBeKiller();
        RETURN_CUTOFF (child->searchMove(ttSlot.from(), ttSlot.to()));
    }

    canBeKiller = false;
    RETURN_CUTOFF (goodCaptures(child, OP.nonKing()));
    canBeKiller = !inCheck();

    if (parent && !inCheck()) {
        RETURN_CUTOFF (child->searchIfPossible(parent->killer[0]));
        RETURN_CUTOFF (counterMove(child));
        RETURN_CUTOFF (followMove(child));

        RETURN_CUTOFF (child->searchIfPossible(parent->killer[1]));
        RETURN_CUTOFF (counterMove(child));
        RETURN_CUTOFF (followMove(child));

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

                RETURN_CUTOFF (goodNonCaptures(child, pi, bbMovesOf(pi) % badSquares, 2_ply));
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
                    RETURN_CUTOFF (goodNonCaptures(child, pi, bbMovesOf(pi) % badSquares, 3_ply));
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

ReturnStatus Node::goodNonCaptures(Node* child, Pi pi, Bb moves, Ply R) {
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
    assert (Score{MinusInfinity} <= alpha && alpha < beta && beta <= Score{PlusInfinity});
    assert (!inCheck());

    // stand pat
    score = eval;
    if (beta <= score) {
        assert (!currentMove);
        return ReturnStatus::BetaCutoff;
    }
    if (ply == MaxPly) {
        // no room to search deeper
        assert (!currentMove);
        return ReturnStatus::Continue;
    }
    if (alpha < score) {
        alpha = score;
    }

    // prepare empty child node to make moves into
    //TODO: create lighter quiescence node without zobrist hashing and repetition detection
    Node node{this};
    const auto child = &node;

    // impossible to capture the king, do not even try to save time
    return goodCaptures(child, OP.nonKing());
}

ReturnStatus Node::goodCaptures(Node* child, PiMask victims) {
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
ReturnStatus Node::counterMove(Node* child) {
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
ReturnStatus Node::followMove(Node* child) {
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

    return parent->negamax(this, R);
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

    return parent->negamax(this, parent->inCheck() ? 1_ply : R); // check extension
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

void Node::updatePv() const {
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
        root.pvScore = score;
        root.info_pv(depth);
    }
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

    for (depth = 1_ply; depth <= root.limits.depth; ++depth) {
        tt = root.tt.prefetch<TtSlot>(zobrist());
        setMoves(rootMovesClone);
        alpha = Score{MinusInfinity};
        beta = Score{PlusInfinity};
        auto returnStatus = search();

        root.newIteration();
        root.refreshTtPv(depth);

        RETURN_IF_STOP (returnStatus);

        if (root.limits.reached<IterationDeadline>()) { return ReturnStatus::Stop; }
    }

    return ReturnStatus::Continue;
}
