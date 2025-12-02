#include "Node.hpp"
#include "Uci.hpp"

#define RETURN_CUTOFF(visitor) { ReturnStatus status = visitor; if (status != ReturnStatus::Continue) { return status; }} ((void)0)

TtSlot::TtSlot (const Node* n) : TtSlot{
    n->zobrist(),
    n->currentMove,
    n->score,
    n->ply,
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
    killer2{grandParent ? grandParent->killer2 : Move{}},
    killer3{Move{}}
{}

ReturnStatus Node::negamax(Node* child, Ply R) const {
    child->depth = depth - R; //TRICK: Ply >= 0
    assert (child->depth >= 0);
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
            if (currentMove && depth > child->depth+1) {
                if (child->isPv) {
                    // rare case (the first move from PV with reduced depth)
                    child->alpha = -beta;
                    assert (child->beta == -alpha);
                } else {
                    assert (child->alpha == child->beta.minus1());
                }
                // reduced search full depth research (unless it was a null move search or leaf node)
                return negamax(child, 1);
            }

            score = childScore;
            failHigh();
            return ReturnStatus::BetaCutoff;
        }

        assert (alpha < childScore && childScore < beta);
        assert (isPv); // alpha < childScore < beta, so current window cannot be zero
        assert (currentMove); // null move in PV is not allowed

        if (!child->isPv) {
            // Principal Variation Search (PVS) research with full window
            child->alpha = -beta;

            child->isPv = true;
            assert (child->alpha < child->beta.minus1());

            assert (child->beta == -alpha);
            return negamax(child, 1);
        }

        score = childScore;
        alpha = childScore;
        child->beta = -alpha;
        child->pvIndex = root.pvMoves.set(pvIndex, uciMove(currentMove), child->pvIndex);
        updatePv();
    }

    if (ply == 0 && depth > 1 && root.limits.reached<RootMoveDeadline>()) {
        return ReturnStatus::Stop;
    }

    // set zero window for the next sibling move search
    child->alpha = child->beta.minus1(); // can be either 1 centipawn or 1 mate distance ply
    child->isPv = false;

    assert (child->beta == -alpha);
    return ReturnStatus::Continue;
}

ReturnStatus Node::search() {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);
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
            depth = depth+1;
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
            if (ttSlot.move() && !isLegalMove(ttSlot.move())) {
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
    }

    eval = evaluate();

    if (ply == MaxPly) {
        // no room to search deeper
        score = eval;
        assert (!currentMove);
        return ReturnStatus::Continue;
    }

    if (
        !inCheck()
        && !isPv
        && depth <= 2
    ) {
        auto delta = (depth == 1) ? 55_cp : 155_cp;
        if (MinEval <= beta && beta <= eval-delta) {
            // Static Null Move Pruning (Reverse Futility Pruning)
            score = eval;
            assert (!currentMove);
            return ReturnStatus::BetaCutoff;
        }
    }

    // prepare empty child node to make moves into
    Node node{this};
    const auto child = &node;

    // Null Move Pruning
    if (
        !inCheck()
        && !isPv
        && MinEval <= beta && beta <= eval
        && depth >= 2 // overhead higher then gain at very low depth
        && MY.evaluation().piecesMat() > 0 // no null move if only pawns left (zugzwang)
    ) {
        canBeKiller = false;
        RETURN_CUTOFF (child->searchNullMove(3 + depth/6));
    }

    if (isHit && ttSlot.move()) {
        canBeKiller = ttSlot.canBeKiller();
        RETURN_CUTOFF (child->searchMove(ttSlot.move()));
    }

    canBeKiller = false;
    RETURN_CUTOFF (goodCaptures(child, OP.nonKing()));
    canBeKiller = !inCheck();

    if (parent && !inCheck()) {
        // primary killer move, updated by previous siblings
        RETURN_CUTOFF (child->searchIfLegal(parent->killer1));

        // countermove heuristic: refutation of the last opponent's move
        Move opMove = parent->currentMove;
        if (opMove) {
            RETURN_CUTOFF (child->searchIfLegal( root.counterMove.get1(
                parent->colorToMove(), parent->MY.typeAt(opMove.from()), opMove.to()
            ) ));
        }

        if (grandParent) {
            // follow move heuristic: continue last made move
            Move myMove = grandParent->currentMove;
            if (myMove) {
                RETURN_CUTOFF (child->searchIfLegal( root.followMove.get1(
                    grandParent->colorToMove(), grandParent->MY.typeAt(myMove.from()), myMove.to()
                ) ));
            }
        }

        // secondary killer move, backup of previous primary killer
        RETURN_CUTOFF (child->searchIfLegal(parent->killer2));

        if (opMove) {
            RETURN_CUTOFF (child->searchIfLegal( root.counterMove.get2(
                parent->colorToMove(), parent->MY.typeAt(opMove.from()), opMove.to()
            ) ));
        }

        if (grandParent) {
            // follow move heuristic: continue last made move
            Move myMove = grandParent->currentMove;
            if (myMove) {
                RETURN_CUTOFF (child->searchIfLegal( root.followMove.get2(
                    grandParent->colorToMove(), grandParent->MY.typeAt(myMove.from()), myMove.to()
                ) ));
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
    PiMask safePieces = {}; // pieces on safe squares
    PiMask officers = MY.officers(); // Q, R, B, N

    // Weak Move Reduction condition: !inCheck()
    bool canR = !inCheck();

    // Weak Move Pruning: !inCheck() && !isPv && depth <= 2
    bool canP = !inCheck() && !isPv && depth <= 2;

    // quiet non-pawn, non-king moves from unsafe to safe squares
    // skip king moves because they are safe anyway (unless in check)
    // castling move is a rook move, king moves rarely good in middlegame,
    // skip pawns to avoid wasting time on safety check as pawns
    Ply R = canR ? 2 : 1;
    for (Pi pi : officers) {
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

    R = canR ? 3 : 1;
    while (safePieces.any()) {
        Pi pi = safePieces.leastValuable(); safePieces -= pi;

        RETURN_CUTOFF (goodNonCaptures(child, pi, movesOf(pi) % badSquares, R));
    }

    // iterate pawns from Rank7 to Rank2
    // underpromotion with or without capture and pawn pushes
    for (Square from : MY.bbPawns()) {
        Pi pi = MY.pieceAt(from);

        R = 1;
        if (canR) {
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
        R = (MY.evaluation().piecesMat() > 16) ? 3 : 2;

        if (!canP || R == 2 || movesMade() == 0) { // weak move pruning
            R = canR ? R : Ply{1};
            Square from = MY.kingSquare();
            for (Square to : movesOf(Pi{TheKing})) {
                RETURN_CUTOFF (child->searchMove({from, to}, R));
            }
        }
    }

    // unsafe (losing) captures
    R = canR ? 2 : 1;
    for (PiMask pieces = officers; pieces.any(); ) {
        Pi pi = pieces.leastValuable(); pieces -= pi;

        Square from = MY.squareOf(pi);
        for (Square to : movesOf(pi) & ~OP.bbSide()) {
            RETURN_CUTOFF (child->searchMove({from, to}, R));
        }
    }

    // unsafe (losing) non-captures
    if (!canP || movesMade() == 0) { // weak move pruning
        R = canR ? 4 : 1;
        for (PiMask pieces = officers; pieces.any(); ) {
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
    child->depth = 0;

    // impossible to capture the king, do not even try to save time
    return goodCaptures(child, OP.nonKing());
}

ReturnStatus Node::goodCaptures(Node* child, PiMask victims) {
    if (MY.promotables().any()) {
        // queen promotions with capture, always good
        for (Pi victim : OP.pieces() & OP.piecesOn(Rank{Rank1})) {
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

ReturnStatus Node::searchMove(Move move, Ply R) {
    RETURN_IF_STOP (root.limits.countNode());

    Square from = move.from();
    Square to = move.to();
    parent->clearMove(from, to);
    parent->currentMove = move;
    makeMove(from, to);

    if (rule50() < 2) { repetitionHash = {}; }
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

    for (depth = 1; depth <= root.limits.depth; ++depth) {
        tt = root.tt.prefetch<TtSlot>(zobrist());
        setMoves(rootMovesClone);
        alpha = Score{MinusInfinity};
        beta = Score{PlusInfinity};
        auto returnStatus = search();

        root.newIteration();
        root.refreshTtPv(depth);

        RETURN_IF_STOP (returnStatus);

        root.info_iteration(depth);

        if (root.limits.reached<IterationDeadline>()) { return ReturnStatus::Stop; }
    }

    return ReturnStatus::Continue;
}
