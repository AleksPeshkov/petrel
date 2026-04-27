#include "search.hpp"
#include "Uci.hpp"

#define RETURN_CUTOFF(visitor) { ReturnStatus status = visitor; if (status != ReturnStatus::Continue) { return status; }} ((void)0)

TtSlot::TtSlot (const Node* n) : TtSlot{
    n->z(),
    n->score,
    n->ply,
    n->bound,
    TtMove{n->currentMove.from(), n->currentMove.to(), n->canBeKiller ? CanBeKiller::Yes : CanBeKiller::No},
    n->depth
} {}

Node::Node (const PositionMoves& p, const Uci& r) :
    PositionMoves{p}, root{r}, parent{nullptr}, grandParent{nullptr}, ply{0},
    alpha{MateLoss}, beta{MateWin}, pvPly{0_ply}, pvIndex{0}
{}

Node::Node (const Node* p) :
    PositionMoves{}, root{p->root}, parent{p}, grandParent{p->parent}, ply{p->ply + 1_ply},
    alpha{-p->beta}, beta{-p->alpha},
    pvPly{p->isPv() ? ply : p->pvPly},
    pvIndex{p->pvIndex.v()+1}
{
    if (grandParent) {
        killer[0] = grandParent->killer[0];
        killer[1] = grandParent->killer[1];
    }
}

// child->depth = depth - R
ReturnStatus Node::negamax(Ply R) const {
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
        // do not use R param as real reduction can be adjusted later
        auto childR = child->depthR();

        if (currentMove.any() && childR >= 2_ply) {
            if (child->isPv()) {
                // rare case (the first move from PV with reduced depth)
                child->alpha = -beta;
                assert (child->beta == -alpha);
            } else {
                assert (child->alpha == child->beta.minus1());
            }
            // full depth research (unless it was a null move search or leaf node)
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
    currentMove = {};
    score = Score{NoScore};
    bound = FailLow;
    assertOk();

    if (moves().none()) {
        // checkmate or stalemate
        score = inCheck() ? Score::mateLoss(ply) : Score{DrawScore};
        assert (currentMove.none());
        return ReturnStatus::Continue;
    }
    if (ply == MaxPly) {
        // no room to search deeper
        score = evaluate();
        assert (currentMove.none());
        return ReturnStatus::Continue;
    }

    if (!isRoot()) {
        // mate-distance pruning
        alpha = std::max(alpha, Score::mateLoss(ply));
        if (!(alpha < std::min(beta, Score::mateWin(ply + 1_ply)))) {
            score = alpha;
            assert (currentMove.none());
            return ReturnStatus::Cutoff;
        }

        if (rule50().isDraw() || isRepetition() || isDrawMaterial()) {
            score = Score{DrawScore};
            assert (currentMove.none());
            return ReturnStatus::Continue;
        }
    }

    if (!inCheck()) {
        eval = evaluate();
    } else {
        eval = Score{NoScore};
        if (!isRoot()) {
            // check extension
            depth = depth + 1_ply;
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

        auto ttMove = ttSlot.ttMove();
        Square ttFrom = ttMove.from();
        Square ttTo = ttMove.to();
        if (ttMove.any() && !isPossibleMove(ttFrom, ttTo)) {
            // collision
            ttHit = false;
            break;
        }

        ++root.tt.hits;

        if (!isPv()
            && ttSlot.draft() >= depth
            && (ttBound == ExactScore
                || (ttBound == FailHigh && beta <= ttScore)
                || (ttBound == FailLow && ttScore <= alpha)
            )
        ) {
            score = ttScore;
            bound = ttBound;
            if (ttMove.any()) {
                assert (isPossibleMove(ttFrom, ttTo));
                canBeKiller = ttMove.canBeKiller() == CanBeKiller::Yes;
                currentMove = historyMove(ttMove);
            } else {
                canBeKiller = false;
                assert (currentMove.none());
            }
            return ReturnStatus::Cutoff;
        }

        if (!inCheck() && ttScore.isEval()) {
            if (ttBound == ExactScore
                || (ttBound == FailHigh && eval <= ttScore)
                || (ttBound == FailLow && ttScore <= eval)
            ) {
                eval = ttScore;
                break;
            }
        }
    } while(false);

    assert ((inCheck() && eval.none()) || (!inCheck() && eval.isEval()));
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
            RETURN_CUTOFF (child->searchNullMove(4_ply + (depth-2_ply)/4));
        }
    }

    if (ttHit && ttSlot.ttMove().any()) {
        canBeKiller = ttSlot.ttMove().canBeKiller() == CanBeKiller::Yes;
        RETURN_CUTOFF (child->searchMove(ttSlot.ttMove().from(), ttSlot.ttMove().to()));
    }

    if (isRoot()) {
        canBeKiller = false; // rootBestMoves can be anything
        for (auto move : root.rootBestMoves) {
            if (move.none()) { break; }
            RETURN_CUTOFF (child->searchIfPossible(historyMove(move.from(), move.to(), CanBeKiller::No)));
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
                safePieces += pi;
                continue;
            }

            assert (OP.attackersTo(~from).any());

            if (OP.attackersTo(~from).none(OP.lessOrEqualValue(MY.typeOf(pi)))) {
                // attacked by more valuable attacker

                if (MY.bbPawnAttacks().has(from) || safeForMe(from)) {
                    // piece is protected
                    safePieces += pi;
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
                    RETURN_CUTOFF (child->searchMove(from, to, from.on(Rank6) ? 1_ply : 2_ply));
                }
            }
        }

        // safe pawns pushes attacking non-pawns
        //TODO: double push attacks
        Bb pawnsThreatsFrom = ((OP.bbSide() - OP.bbPawns()).pawnAttacks() % OP_OCCUPIED) >> 8;
        Bb potentialAttackers = MY.bbPawns() & ~pawnsThreatsFrom;
        for (Square from : potentialAttackers) {
            Square to{File{from}, Rank{from}.forward()};
            if (!bbMovesOf(MY.pi(from)).has(to)) { continue; }
            if (safeForOp(to)) { continue; }
            RETURN_CUTOFF (child->searchMove(from, to, 2_ply));
        }

        // safe officers moves
        while (safePieces.any()) {
            Pi pi = safePieces.piLast(); safePieces -= pi;
            RETURN_CUTOFF (goodNonCaptures(pi, bbMovesOf(pi) % bbAvoid, 3_ply));
        }

        if (depth <= 2_ply && !inCheck() && (!isPv() || movesMade() > 0)) { break; }

        // king quiet moves (always safe), castling is a rook move
        {
            Pi pi{TheKing};
            Square from{MY.sqKing()};
            for (Square to : bbMovesOf(pi)) {
                RETURN_CUTOFF (child->searchMove(from, to, 4_ply));
            }
        }

        // all remaining pawn moves
        // losing queen promotions, all underpromotions
        // losing passed pawns moves, all non passed pawns moves
        for (Square from : MY.bbPawns()) {
            Pi pi = MY.pi(from);
            for (Square to : bbMovesOf(pi)) {
                RETURN_CUTOFF (child->searchMove(from, to, 4_ply));
            }
        }

        // unsafe (losing) captures (N/B, R, Q order)
        for (PiMask pieces = MY.officers(); pieces.any(); ) {
            Pi pi = pieces.piLast(); pieces -= pi;
            Square from{MY.sq(pi)};
            for (Square to : bbMovesOf(pi) & ~OP.bbSide()) {
                RETURN_CUTOFF (child->searchMove(from, to, 4_ply));
            }
        }

        if (depth <= 4_ply && !inCheck() && (!isPv() || movesMade() > 0)) { break; }

        // unsafe (losing) non-captures (N/B, R, Q order)
        for (PiMask pieces = MY.officers(); pieces.any(); ) {
            Pi pi = pieces.piLast(); pieces -= pi;
            Square from{MY.sq(pi)};
            for (Square to : bbMovesOf(pi)) {
                RETURN_CUTOFF (child->searchMove(from, to, 5_ply));
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
        assert (isNonCapture(pi, to));

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

        RETURN_CUTOFF (child->searchMove(from, to, R));
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
                RETURN_CUTOFF (child->searchMove(from, to));
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
            Pi pi = attackers.piLast(); attackers -= pi;
            Square from{MY.sq(pi)};
            RETURN_CUTOFF (child->searchMove(from, to));
        }
    }

    return ReturnStatus::Continue;
}

// Counter move heuristic: refutation of the last opponent's move
ReturnStatus Node::counterMove() {
    assert (parent);
    if (parent->currentMove.any()) {
        for (auto i : range<decltype(root.counterMove)::Index>()) {
            auto move = root.counterMove.get(i, parent->colorToMove(), parent->currentMove);
            if (move.none()) { break; }
            if (isPossibleMove(move)) {
                return child->searchMove(move);
            }
        }
    }
    return ReturnStatus::Continue;
}

// Follow up move heuristic: continue the idea of our last made move
ReturnStatus Node::followMove() {
    if (grandParent && grandParent->currentMove.any()) {
        for (auto i : range<decltype(root.followMove)::Index>()) {
            auto move = root.followMove.get(i, grandParent->colorToMove(), grandParent->currentMove);
            if (move.none()) { break; }
            if (isPossibleMove(move)) {
                return child->searchMove(move);
            }
        }
    }
    return ReturnStatus::Continue;
}

ReturnStatus Node::searchNullMove(Ply R) {
    RETURN_IF_STOP (root.limits.countNode());

    //TRICK: null move not counted as movesMade()
    parent->currentMove = {};
    makeNullMove(parent);

    tt = root.tt.prefetch<TtSlot>(z());
    repHash = {};

    return parent->negamax(R);
}

ReturnStatus Node::searchMove(HistoryMove move, Ply R) {
    RETURN_IF_STOP (root.limits.countNode());

    assert (move.any());
    assert (parent->isPseudoLegal(move));
    assert (parent->isPossibleMove(move));
    parent->currentMove = move;

    Square from{move.from()};
    Square to{move.to()};

    parent->clearMove(from, to);
    makeMove(parent, from, to);

    tt = root.tt.prefetch<TtSlot>(z());
    root.pv.clear(pvIndex);

    if (rule50() < 2_ply) { repHash = {}; }
    else if (grandParent) { repHash = RepHash{grandParent->repHash, grandParent->z()}; }
    else { repHash = root.repetitions.repHash(colorToMove()); }

    R = parent->adjustDepthR(R);
    return parent->negamax(R);
}

Ply Node::adjustDepthR(Ply R) const {
    if (R <= 1_ply) { return R; }
    if (inCheck()) { return 1_ply; }

    // depth adaptive reduction
    if (depth <= 8_ply && R >= 4_ply) { R = R - 1_ply; }

    return R;
}

void Node::failHigh() const {
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

ReturnStatus Node::updatePv() const {
    assert (isPseudoLegal(currentMove));

    if (depth > 0_ply) {
        bound = ExactScore;
        *tt = TtSlot{this};
        ++root.tt.writes;
    }

    if (canBeKiller) {
        updateHistory(currentMove);
    }

    if (!isRoot()) {
        child->pvIndex = root.pv.set(pvIndex, currentMove, child->pvIndex);
    } else {
        // unfinished iteration, so report depth-1
        pvIndex = root.pv.set(depth - 1_ply, score, currentMove, child->pvIndex);
        child->pvIndex = PrincipalVariation::Index{pvIndex.v() + 1};

        RETURN_IF_STOP (root.limits.updateMoveComplexity(currentMove, score));

        ::insert_unique(root.rootBestMoves, currentMove);
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
        root.counterMove.set(parent->colorToMove(), parent->currentMove, historyMove);
    }

    if (grandParent && grandParent->currentMove.any()) {
        root.followMove.set(grandParent->colorToMove(), grandParent->currentMove, historyMove);
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

    if (grandParent) {
        auto next = grandParent;
        while ((next = next->grandParent)) {
            if (next->z() == z()) {
                return true;
            }
            if (!next->repHash.has(z())) {
                return false;
            }
        }
    }

    return root.repetitions.has(colorToMove(), z());
}

void refreshTtPv(const PositionMoves& p, const PrincipalVariation& pv, const Tt& tt) {
    // clone position
    PositionMoves pos{p};

    Ply   ply   = 0_ply;
    Ply   depth = pv.depth();
    Score score = pv.score();
    auto  pmoves = pv.moves();

    for (HistoryMove move; (move = *pmoves++).any();) {
        assert (score.isOk(ply));
        assert (pos.isPseudoLegal(move));
        assert ((pos.generateMoves(), pos.isPossibleMove(move.from(), move.to())));

        auto o = tt.addr<TtSlot>(pos.z());
        *o = TtSlot{pos.z(), score, ply, ExactScore, TtMove{move.from(), move.to(), CanBeKiller::No}, depth};
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
    auto rootMovesClone = moves(); // copy all moves (filtered by `go moves` command)
    repHash = root.repetitions.repHash(colorToMove());

    for (depth = 1_ply; depth <= root.limits.maxDepth(); ++depth) {
        tt = root.tt.prefetch<TtSlot>(z());
        setMoves(rootMovesClone);
        alpha = Score{MateLoss};
        beta = Score{MateWin};

        RETURN_IF_STOP (search());
        root.pv.set(depth); // iteration fully completed

        RETURN_IF_STOP (root.limits.iterationDeadlineReached());

        root.info_pv();
        root.newIteration();
        ::refreshTtPv(*this, root.pv, root.tt);
    }

    return ReturnStatus::Continue;
}
