#include "Node.hpp"
#include "Uci.hpp"

#define RETURN_CUTOFF(visitor) { ReturnStatus returnStatus = visitor; \
    if (returnStatus != ReturnStatus::Continue) { return returnStatus; }} ((void)0)

TtSlot::TtSlot (const Node* n) : TtSlot{
    n->zobrist(),
    n->currentMove,
    n->score.toTt(n->ply),
    n->bound,
    n->depth,
    n->canBeKiller
} {}

Node::Node (const PositionMoves& p, const Uci& r) : PositionMoves{p}, root{r} {}

Node::Node (const Node* p) :
    PositionMoves{}, root{p->root}, parent{p}, grandParent{p->parent},
    ply{p->ply + 1}, depth{p->depth > 0 ? p->depth-1 : 0},
    alpha{-p->beta}, beta{-p->alpha}, isPv(p->isPv),
    pvIndex{p->pvIndex+1},
    killer1{grandParent ? grandParent->killer1 : Move{}},
    killer2{grandParent ? grandParent->killer2 : Move{}}
{
}

ReturnStatus Node::negamax(Node* child, Ply R) const {
    child->depth = depth - R; //TRICK: Ply >= 0
    child->generateMoves();
    RETURN_IF_STOP (child->search());

    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    auto childScore = -child->score;

    if (childScore <= alpha) {
        if (score < childScore) {
            score = childScore;
        }
    } else {
        if (beta <= childScore) {
            if (currentMove && depth > 1 && depth-1 > child->depth) {
                if (child->isPv) {
                    // rare case
                    child->alpha = -beta;
                    assert (child->beta == -alpha);
                }
                // research reduced search with full depth (unless it was a null move search)
                return negamax(child, 1);
            }

            //TODO: null move verification can be done here

            score = childScore;
            failHigh();
            return ReturnStatus::BetaCutoff;
        }

        assert (alpha < childScore && childScore < beta);
        assert (isPv); // alpha < childScore < beta, so current window cannot be zero
        assert (currentMove); // null move in PV is not allowed

        if (!child->isPv) {
            // Principal Variation Search (PVS) research with full window
            child->isPv = true;
            child->alpha = -beta;
            assert (child->beta == -alpha);
            assert (child->alpha < child->beta-1);
            return negamax(child, 1);
        }

        score = childScore;
        alpha = childScore;
        child->beta = -alpha;
        child->pvIndex = root.pvMoves.set(pvIndex, uciMove(currentMove), child->pvIndex);
        updatePv();
    }

    if (ply == 0 && root.limits.reached<RootMoveDeadline>()) {
        return ReturnStatus::Stop;
    }

    // set zero window for the next sibling move search
    assert (child->beta == -alpha);
    child->alpha = child->beta-1;
    child->isPv = false;
    return ReturnStatus::Continue;
}

ReturnStatus Node::search() {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);
    score = NoScore;
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
            score = DrawScore;
            assert (!currentMove);
            return ReturnStatus::Continue;
        }

        if (inCheck()) {
            // check extension
            depth = depth+1;
        }
    }

    if (depth <= 0 && !inCheck()) {
        assert (depth == 0);
        return quiescence();
    }

    assert (depth > 0);

    ++root.tt.reads;
    ttSlot = *tt;
    isHit = (ttSlot == zobrist());
    if (isHit) {
        if (ttSlot.move() && !isLegalMove(ttSlot.move())) {
            io::log("#illegal TT move");
            isHit = false;
        } else {
            ++root.tt.hits;

            if (ttSlot.draft() >= depth && !isPv) {
                Bound ttBound = ttSlot.bound();
                Score ttScore = ttSlot.score(ply);

                //TODO: refresh TT record if age is old
                if ((ttBound & FailHigh) && beta <= ttScore) {
                    score = ttScore;
                    currentMove = ttSlot.move();
                    return ReturnStatus::BetaCutoff;
                } else if ((ttBound & FailLow) && ttScore <= alpha) {
                    score = ttScore;
                    currentMove = ttSlot.move();
                    return ReturnStatus::Continue;
                }
            }
        }
    }

    // skip costly evaluate() while in check, as it should not be used
    eval = !inCheck() ? evaluate() : Score::checkmated(ply);

    if (ply == MaxPly) {
        // no room to search deeper
        score = eval;
        assert (!currentMove);
        return ReturnStatus::Continue;
    }

    // prepare empty child node to make moves into
    Node node{this};
    const auto child = &node;

    if (
        !inCheck()
        && !isPv
        && eval >= beta
    ) {
        // Static Null Move Pruning (Reverse Futility Pruning)
        if (depth <= 2) {
            auto delta = (depth == 1) ? 55 : 155;
            if (eval-delta >= beta) {
                score = eval;
                assert (!currentMove);
                return ReturnStatus::BetaCutoff;
            }
        }

        // Null Move Pruning
        if (
            depth >= 2 // overhead higher then gain at very low depth
            && MY.material().phase() > 0 // no null move if only pawns left (zugzwang)
        ) {
            canBeKiller = false;
            RETURN_CUTOFF (child->searchNullMove(4 + (depth-2)/4));
        }
    }

    if (isHit && ttSlot.move()) {
        canBeKiller = ttSlot.canBeKiller();
        RETURN_CUTOFF (child->searchMove(ttSlot.move()));
    }

    // impossible to capture the king, do not even try to save time
    canBeKiller = false;
    RETURN_CUTOFF (goodCaptures(child, OP.notKing()));
    canBeKiller = !inCheck();

    if (parent && !inCheck()) {
        // primary killer move, updated by previous siblings
        RETURN_CUTOFF (child->searchIfLegal(parent->killer1));

        // countermove heuristic: refutation of the last opponent's move
        Move opMove = parent->currentMove;
        if (opMove) {
            for (int i = 0; i < decltype(root.counterMove)::Size; ++i) {
                auto move = root.counterMove.get(i,
                    parent->colorToMove(), parent->MY.typeAt(opMove.from()), opMove.to()
                );
                if (isLegalMove(move)) {
                    RETURN_CUTOFF (child->searchMove(move));
                    break;
                }
            }
        }

        // follow move heuristic: continue the idea of our last made move
        if (grandParent) {
            Move myMove = grandParent->currentMove;
            if (myMove) {
                for (int i = 0; i < decltype(root.followMove)::Size; ++i) {
                    auto move = root.followMove.get(i,
                        grandParent->colorToMove(), grandParent->MY.typeAt(myMove.from()), myMove.to()
                    );
                    if (isLegalMove(move)) {
                        RETURN_CUTOFF (child->searchMove(move));
                        break;
                    }
                }
            }
        }

        // secondary killer move, backup of previous primary killer
        RETURN_CUTOFF (child->searchIfLegal(parent->killer2));

        // countermove heuristic: refutation of the last opponent's move
        if (opMove) {
            for (int i = 0; i < decltype(root.counterMove)::Size; ++i) {
                auto move = root.counterMove.get(i,
                    parent->colorToMove(), parent->MY.typeAt(opMove.from()), opMove.to()
                );
                if (isLegalMove(move)) {
                    RETURN_CUTOFF (child->searchMove(move));
                    break;
                }
            }
        }

        // follow move heuristic: continue the idea of our last made move
        if (grandParent) {
            Move myMove = grandParent->currentMove;
            if (myMove) {
                for (int i = 0; i < decltype(root.followMove)::Size; ++i) {
                    auto move = root.followMove.get(i,
                        grandParent->colorToMove(), grandParent->MY.typeAt(myMove.from()), myMove.to()
                    );
                    if (isLegalMove(move)) {
                        RETURN_CUTOFF (child->searchMove(move));
                        break;
                    }
                }
            }
        }

        // repeated killer heuristic (can change while searching descendants of previous killer3)
        while (isLegalMove(parent->killer3)) {
            RETURN_CUTOFF (child->searchMove(parent->killer3));
        }
    }

    // going to search only non-captures, mask out remaining unsafe captures to avoid redundant safety checks
    //TRICK: ~ is not a negate bitwise operation but byteswap -- flip opponent's bitboard
    Bb badSquares = ~(OP.bbPawnAttacks() | OP.bbSide());
    PiMask safePieces = {};
    PiMask figures = MY.figures(); // Q, R, B, N

    // Late Move Reductions: !inCheck()
    bool lmr = !inCheck();

    // Late Move Pruning: !inCheck() && !isPv && depth <= 2
    bool lmp = !inCheck() && !isPv && depth <= 2;

    // quiet non-pawn, non-king moves from unsafe to safe squares
    // skip king moves because they are safe anyway (unless in check)
    // castling move is a rook move, king moves rarely good in middlegame,
    // skip pawns to avoid wasting time on safety check as pawns
    Ply R = lmr ? 2 : 1;
    for (Pi pi : figures) {
        Square from = MY.squareOf(pi);

        if (!bbAttacked().has(from)) {
            // piece on safe square
            safePieces += pi;
            continue;
        }

        assert (OP.attackersTo(~from).any());

        // attacked by more valuable attacker
        if ((OP.attackersTo(~from) & OP.lessOrEqualValue(MY.typeOf(pi))).none()) {
            if (MY.bbPawnAttacks().has(from)) {
                // square defended by a pawn
                safePieces += pi;
                continue;
            }

            if (MY.attackersTo(from).popcount() >= OP.attackersTo(~from).popcount()) {
                // enough total defenders, possibly false positive
                safePieces += pi;
                continue;
            }
            //TODO: try protecting moves of other pieces
        }

        RETURN_CUTOFF (goodNonCaptures(child, pi, movesOf(pi) % badSquares, R));
    }

    R = lmr ? 3 : 1;
    while (safePieces.any()) {
        Pi pi = safePieces.leastValuable(); safePieces -= pi;

        RETURN_CUTOFF (goodNonCaptures(child, pi, movesOf(pi) % badSquares, R));
    }

    // iterate pawns from Rank7 to Rank2
    // underpromotion with or without capture and pawn pushes
    for (Square from : MY.bbPawns()) {
        Pi pi = MY.pieceAt(from);

        R = 1;
        if (lmr) {
            if (from.on(Rank7)) { R = 4; } // underpromotion
            else if (from.on(Rank6)) { R = 0; } // passed pawn push extension
            else if (from.on(Rank5)) { R = 1; } // advanced pawn push extension
            else { R = 2; } // default reduction for pawn moves
        }

        for (Square to : movesOf(pi)) {
            RETURN_CUTOFF (child->searchMove({from, to}, R));
        }
    }

    // king quiet moves (always safe), castling is rook move
    {
        // reduce king moves more in middle game
        R = (MY.material().phase() > 16) ? 3 : 2;

        if (!lmp || R == 2 || movesMade() == 0) { // late move pruning
            R = lmr ? R : Ply{1};
            Square from = MY.kingSquare();
            for (Square to : movesOf(Pi{TheKing})) {
                RETURN_CUTOFF (child->searchMove({from, to}, R));
            }
        }
    }

    // unsafe (losing) captures
    R = lmr ? 2 : 1;
    for (PiMask pieces = figures; pieces.any(); ) {
        Pi pi = pieces.leastValuable(); pieces -= pi;

        Square from = MY.squareOf(pi);
        for (Square to : movesOf(pi) & ~OP.bbSide()) {
            RETURN_CUTOFF (child->searchMove({from, to}, R));
        }
    }

    // unsafe (losing) non-captures
    if (!lmp || movesMade() == 0) { // late move pruning
        R = lmr ? 4 : 1;
        for (PiMask pieces = figures; pieces.any(); ) {
            Pi pi = pieces.leastValuable(); pieces -= pi;

            Square from = MY.squareOf(pi);
            for (Square to : movesOf(pi)) {
                RETURN_CUTOFF (child->searchMove({from, to}, R));
            }
        }
    }

    if (bound == FailLow) {
        // fail low, no good move found, write back previous TT move if any
        currentMove = isHit ? ttSlot.move() : Move{};
        *tt = TtSlot(this);
        ++root.tt.writes;
    }
    return ReturnStatus::Continue;
}

ReturnStatus Node::goodNonCaptures(Node* child, Pi pi, Bb moves, Ply R) {
    Square from = MY.squareOf(pi);
    PieceType ty = MY.typeOf(pi);
    assert (!ty.is(Pawn));
    PiMask opLessValue = OP.lessValue(ty);

    for (Square to : moves) {
        assert (!OP.bbPawnAttacks().has(~to));
        assert (isNonCapture({from, to}));

        if (bbAttacked().has(to)) {
            if ((OP.attackersTo(~to) & opLessValue).any()) {
                // skip move if square defended by less valued piece
                continue;
            }

            if ((MY.attackersTo(to) % PiMask{pi}).none()) {
                // skip move to defended square if nobody else attacks it
                continue;
            }
        }

        RETURN_CUTOFF (child->searchMove({from, to}, R));
    }

    return ReturnStatus::Continue;
}

ReturnStatus Node::quiescence() {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);
    assert (!inCheck());

    eval = evaluate();

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
    return goodCaptures(child, OP.notKing());
}

ReturnStatus Node::goodCaptures(Node* child, const PiMask& victims) {
    if (MY.promotables().any()) {
        // queen promotions with capture, always good
        for (Pi victim : OP.pieces() & OP.piecesOn(Rank1)) {
            Square to = ~OP.squareOf(victim);
            for (Pi pi : canMoveTo(to) & MY.promotables()) {
                Square from = MY.squareOf(pi);
                RETURN_CUTOFF (child->searchMove({from, to}));
            }
        }

        // queen promotions without capture
        for (Pi pawn : MY.promotables()) {
            Square from = MY.squareOf(pawn);
            Square to{File{from}, Rank8};
            RETURN_CUTOFF (child->searchIfLegal({from, to}));
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
        if (OP.bbPawnAttacks().has(~to) || OP.attackersTo(~to).popcount() >= MY.attackersTo(to).popcount()) {
            attackers &= MY.lessOrEqualValue(OP.typeOf(victim));
        }

        while (attackers.any()) {
            // LVA (least valuable attacker) order
            Pi pi = attackers.leastValuable(); attackers -= pi;

            Square from = MY.squareOf(pi);
            RETURN_CUTOFF (child->searchMove({from, to}));
        }
    }

    return ReturnStatus::Continue;
}

ReturnStatus Node::searchNullMove(Ply R) {
    RETURN_IF_STOP (root.limits.countNode());

    makeNullMove(parent);

    parent->currentMove = {};

    tt = root.tt.prefetch<TtSlot>(zobrist());
    repetitionHash = RepetitionHash{};

    return parent->negamax(this, R);
}

void Node::makeMove(Square from, Square to) {
    Position::makeMove(parent, from, to);
    tt = root.tt.prefetch<TtSlot>(zobrist());
    root.pvMoves.clearPly(pvIndex);
}

ReturnStatus Node::searchMove(Move move, Ply R) {
    RETURN_IF_STOP (root.limits.countNode());

    Square from = move.from();
    Square to = move.to();
    parent->clearMove(from, to);
    parent->currentMove = move;
    makeMove(from, to);

    if (rule50() < 2) { repetitionHash = {}; }
    else if (grandParent && !grandParent->currentMove) { repetitionHash = {}; } // null move resets repetition but not rule50
    else if (grandParent) { repetitionHash = RepetitionHash{grandParent->repetitionHash, grandParent->zobrist()}; }
    else { repetitionHash = root.repetitions.repetitionHash(colorToMove()); }

    return parent->negamax(this, R);
}

void Node::failHigh() const {
    // currentMove is null (after NMP), write back previous TT move instead
    if (!currentMove && isHit && ttSlot.move()) {
        currentMove = ttSlot.move();
    }

    if (depth > 0) {
        bound = FailHigh;
        *tt = TtSlot{this};
        ++root.tt.writes;
    }

    if (parent && canBeKiller) {
        assert (currentMove);
        parent->updateKillerMove(currentMove);
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
        parent->updateKillerMove(currentMove);
    }

    if (ply == 0) {
        root.pvScore = score;
        root.info_pv(depth);
    }
}

void Node::updateKillerMove(Move newKiller) const {
    if (killer1 != newKiller) {
        if (killer2 != newKiller) {
            if (killer3 != newKiller) {
                // fresh killer move
                killer2 = killer1;
                killer1 = newKiller;
            } else {
                // promote killer3 to killer1
                killer3 = killer2;
                killer2 = killer1;
                killer1 = newKiller;
            }
        } else {
            // promote killer2 to killer1
            killer2 = killer1;
            killer1 = newKiller;
        }
    }

    if (grandParent && grandParent->killer1 != newKiller && grandParent->killer2 != newKiller) {
        grandParent->killer3 = newKiller;
    }

    if (currentMove) {
        root.counterMove.set(colorToMove(),  MY.typeAt(currentMove.from()), currentMove.to(), newKiller);
    }

    if (parent && parent->currentMove) {
        root.followMove.set(parent->colorToMove(),  parent->MY.typeAt(parent->currentMove.from()), parent->currentMove.to(), newKiller);
    }
}

UciMove Node::uciMove(Move move) const {
    Square from = move.from();
    Square to = move.to();
    return UciMove{from, to, isSpecial(from, to), colorToMove(), root.chessVariant()};
}

constexpr Color Node::colorToMove() const { return root.colorToMove(ply); }

// insufficient mate material
bool Node::isDrawMaterial() const {
    auto& my = MY.material();
    auto& op = OP.material();

    if (my.hasMatingPieces() || op.hasMatingPieces()) { return false; }

    // here both sides can have only minors pieces
    auto myMinors = my.count(Knight) + my.count(Bishop);
    auto opMinors = op.count(Knight) + op.count(Bishop);

    // lone minors cannot mate
    if (myMinors <= 1 && opMinors <= 1) { return true; }

    if (myMinors == 2) {
        if (my.count(Bishop) == 0 && opMinors <= 1) { return true; }
        if (my.count(Bishop) == 1 && opMinors == 1) { return true; }
        if (my.count(Bishop) == 2 && op.count(Bishop) == 1) { return true; }
    }

    if (opMinors == 2) {
        if (op.count(Bishop) == 0 && opMinors <= 1) { return true; }
        if (op.count(Bishop) == 1 && myMinors == 1) { return true; }
        if (op.count(Bishop) == 2 && my.count(Bishop) == 1) { return true; }
    }

    return false;
}

bool Node::isRepetition() const {
    if (rule50() < 4) { return false; }

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
    tt = root.tt.prefetch<TtSlot>(zobrist());

    if (root.limits.reached<IterationDeadline>()) {
        // we have no time to search, return TT move immediately if found
        ++root.tt.reads;
        ttSlot = *tt;
        isHit = (ttSlot == zobrist());
        if (!isHit) {
            io::log("#no time, no TT record found");
        } else {
            Move ttMove = ttSlot.move();
            if (!isLegalMove(ttMove)) {
                if (ttMove) {
                    io::log("#no time, illegal TT move");
                } else {
                    io::log("#no time, TT null move found");
                }
            } else {
                if (root.limits.canPonder) {
                    Node node{this};
                    const auto child = &node;

                    child->makeMove(ttMove.from(), ttMove.to());
                    ++root.tt.reads;
                    child->ttSlot = *child->tt;
                    child->isHit = (child->ttSlot == child->zobrist());
                    if (child->isHit) {
                        Move ttMove2 = child->ttSlot.move();
                        if (ttMove2) {
                            child->generateMoves();
                            if (child->isLegalMove(ttMove2)) {
                                ++root.tt.hits;
                                root.pvMoves.clearPly(PvMoves::Index{child->pvIndex+1});
                                root.pvMoves.set(child->pvIndex, child->uciMove(ttMove2), PvMoves::Index{child->pvIndex+1});
                            }
                        }
                    }
                }

                ++root.tt.hits;
                root.pvScore = ttSlot.score(ply);
                root.pvMoves.set(pvIndex, uciMove(ttMove), PvMoves::Index{pvIndex+1});
                io::log("#no time, return move from TT");
                return ReturnStatus::Stop;
            }
        }
    }

    for (depth = 1; depth <= root.limits.depth; ++depth) {
        setMoves(rootMovesClone);
        alpha = MinusInfinity;
        beta = PlusInfinity;
        auto returnStatus = search();

        root.newIteration();
        refreshTtPv();

        RETURN_IF_STOP (returnStatus);

        root.info_iteration(depth);

        if (root.limits.reached<IterationDeadline>()) { return ReturnStatus::Stop; }
    }

    return ReturnStatus::Continue;
}

 // refresh PV in TT before new search iteration if it was occasionally overwritten
 void Node::refreshTtPv() {
    Position pos{root.position_};
    Score s = root.pvScore;
    Ply d = depth;

    const UciMove* pv = root.pvMoves;
    for (Move move; (move = *pv++);) {
        auto o = root.tt.addr<TtSlot>(pos.zobrist());
        *o = TtSlot{pos.zobrist(), move, s, ExactScore, d, false};
        ++root.tt.writes;

        //we cannot use makeZobrist() because of en passant legality validation
        pos.makeMove(move.from(), move.to());
        s = -s;
        d = d-1;
    }
}
