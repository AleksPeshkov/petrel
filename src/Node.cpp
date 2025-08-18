#include "Node.hpp"
#include "AttacksFrom.hpp"
#include "Uci.hpp"

#define RETURN_CUTOFF(visitor) { ReturnStatus status = visitor; \
    if (status == ReturnStatus::Stop) { return ReturnStatus::Stop; } \
    if (status == ReturnStatus::BetaCutoff) { return ReturnStatus::BetaCutoff; }} ((void)0)

TtSlot::TtSlot (Z z, Score s, Bound b, Move move, Ply d) :
    zobrist{z >> 40},
    score{s},
    bound{b},
    from{move.from()},
    to{move.to()},
    draft_{d}
{}

TtSlot::TtSlot (Node* n, Bound b) : TtSlot{n->zobrist(), n->score, b, n->childMove, n->draft} {}

Node::Node (NodeRoot& r) :
    PositionMoves{r}, parent{nullptr}, grandParent{nullptr}, root{r}, ply{0} {}

Node::Node (Node* n) :
    PositionMoves{}, parent{n}, grandParent{n->parent}, root{n->root}, ply{n->ply + 1},
    killer1{grandParent ? grandParent->killer1 : Move{}},
    killer2{grandParent ? grandParent->killer2 : Move{}}
{}

ReturnStatus Node::searchRoot() {
    root.newSearch();
    root.nodeCounter = { root.limits.nodes };

    auto rootMovesClone = moves;
    repMask = root.repetitions.repMask(colorToMove());
    origin = root.tt.prefetch<TtSlot>(zobrist());

    for (draft = 1; draft <= root.limits.depth; ++draft) {
        moves = rootMovesClone;
        movesMade = 0;
        score = NoScore;
        alpha = MinusInfinity;
        beta = PlusInfinity;

        auto status = search();
        updateTtPv();

        if (status == ReturnStatus::Stop) { return ReturnStatus::Stop; }

        root.uci.info_iteration(draft);
        root.newIteration();
    }

    if (root.limits.infinite || root.limits.ponder) {
        root.uci.waitStop();
    }

    return ReturnStatus::Continue;
}

 // refresh PV in TT if it was overwritten
 void Node::updateTtPv() {
    Node node{root};
    Score s = score;
    Ply d = draft;

    const Move* pv = root.pvMoves;
    for (Move move; (move = *pv++);) {
        auto o = root.tt.addr<TtSlot>(node.zobrist());
        *o = TtSlot{node.zobrist(), s, Exact, move, d};
        ++root.tt.writes;

        node.makeMove(move.from(), move.to());
        s = -s;
        d = d > 0 ? d-1 : 0;
    }
}

ReturnStatus Node::searchMove(Move move) {
    alpha = -parent->beta;
    beta = -parent->alpha;
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    draft = parent->draft > 0 ? parent->draft-1 : 0;

    RETURN_IF_STOP (root.countNode());
    parent->childMove = move;
    makeMove(parent, move);
    root.pvMoves.set(ply, UciMove{});

    origin = root.tt.prefetch<TtSlot>(zobrist());
    ++parent->movesMade;

    if (rule50() < 2) { repMask = RepetitionMask{}; }
    else if (grandParent) { repMask = RepetitionMask{grandParent->repMask, grandParent->zobrist()}; }
    else { repMask = root.repetitions.repMask(colorToMove()); }

    canBeKiller = false;

    if (moves.none()) {
        // checkmate or stalemate
        score = inCheck ? Score::checkmated(ply) : Score{DrawScore};
        return parent->negamax(this);
    }

    if (rule50().isDraw() || isRepetition() || isDrawMaterial()) {
        score = DrawScore;
        return parent->negamax(this);
    }

    if (ply == MaxPly) {
        // no room to search deeper
        score = evaluate();
        return parent->negamax(this);
    }

    if (draft == 0 && !inCheck) {
        RETURN_IF_STOP (quiescence());
        return parent->negamax(this);
    }

    score = NoScore;
    RETURN_IF_STOP (search());
    return parent->negamax(this);
}

ReturnStatus Node::negamax(Node* child) {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    auto childScore = -child->score;

    if (childScore > score) {
        score = childScore;

        if (score >= beta) {
            //beta cut off
            updateKillerMove();
            *origin = TtSlot{this, UpperBound};
            ++root.tt.writes;
            return ReturnStatus::BetaCutoff;
        }

        if (score > alpha) {
            alpha = score;

            root.pvMoves.set(ply, uciMove(childMove));
            *origin = TtSlot{this, Exact};
            ++root.tt.writes;

            if (ply == 0) {
                root.uci.info_pv(draft, score);
            }
        }
    }

    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);
    return ReturnStatus::Continue;
}

ReturnStatus Node::search() {
    // mate-distance pruning
    if (ply >= 1) {
        alpha = std::max(alpha, Score::checkmated(ply));
        if (alpha >= beta) { score = alpha; return ReturnStatus::BetaCutoff; }
    }

    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    Node node{this};
    const auto child = &node;

    canBeKiller = false;

    ++root.tt.reads;
    ttSlot = *origin;

    isHit = (ttSlot == zobrist());
    if (isHit) {
        ++root.tt.hits;
        RETURN_CUTOFF (child->searchIfLegal(ttSlot));
    }

    RETURN_CUTOFF (goodCaptures(child));
    RETURN_CUTOFF (safePromotions(child));
    RETURN_CUTOFF (notBadCaptures(child));
    RETURN_CUTOFF (allPromotions(child));
    //TODO: underpromotions

    canBeKiller = true;

    Pi lastPi = TheKing;
    Bb newMoves = {};

    //TODO: checking moves

    if (parent) {
        // killer move to be tried first
        RETURN_CUTOFF (child->searchIfLegal(parent->killer1));

        // counter moves may refute the last opponent move
        Move move = parent->childMove;
        PieceType ty = parent->MY.typeOf(move.from());
        RETURN_CUTOFF (child->searchIfLegal( root.counterMove(colorToMove(), ty, move.to()) ));

        RETURN_CUTOFF (child->searchIfLegal(parent->killer2));

        // try quiet moves of the last moved piece (unless it was captured)
        {
            Square from = parent->movedPieceTo();
            if (MY.bbSide().has(from)) {
                // last moved piece
                lastPi = MY.pieceAt(from);

                // new moves of the last moved piece
                newMoves = moves[lastPi];

                if (from != parent->movedPieceFrom()) {
                    // unless it was a pawn promotion move
                    newMoves %= parent->MY.attacksOf(lastPi);
                }

                // try new safe moves of the last moved piece
                for (Square to : newMoves % attackedSquares) {
                    RETURN_CUTOFF (child->searchMove(from, to));
                }

                // keep unsafe news moves for later
                newMoves &= attackedSquares;
            }
        }

        // new safe quiet moves, except for the last moved piece (or king)
        for (Pi pi : MY.pieces() - lastPi) {
            Square from = MY.squareOf(pi);
            for (Square to : moves[pi] % parent->MY.attacksOf(pi) % attackedSquares) {
                RETURN_CUTOFF (child->searchMove(from, to));
            }
        }
    }

    // all the rest safe quiet moves
    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);
        for (Square to : moves[pi] % attackedSquares) {
            RETURN_CUTOFF (child->searchMove(from, to));
        }
    }

    if (newMoves.any()) {
        Square from = parent->movedPieceTo();
        Pi pi = MY.pieceAt(from);

        // unsafe new moves of the last moved piece
        for (Square to : newMoves) {
            RETURN_CUTOFF (child->searchIfLegal({from, to}));
        }

        // the rest moves of the last moved piece
        for (Square to : moves[pi]) {
            RETURN_CUTOFF (child->searchMove(from, to));
        }
    }

    // unsafe (bad) captures
    RETURN_CUTOFF (allCaptures(child));

    // all the rest moves, LV first
    auto pieces = MY.pieces();
    while (pieces.any()) {
        Pi pi = pieces.leastValuable(); pieces -= pi;
        Square from = MY.squareOf(pi);

        for (Square to : moves[pi]) {
            RETURN_CUTOFF (child->searchMove(from, to));
        }
    }

    return ReturnStatus::Continue;
}

ReturnStatus Node::quiescence() {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);
    assert (!inCheck);

    //stand pat
    score = evaluate();
    if (beta <= score) {
        return ReturnStatus::BetaCutoff;
    }
    if (alpha < score) {
        alpha = score;
    }

    Node node{this};
    const auto child = &node;

    ttSlot = *origin;
    ++root.tt.reads;

    isHit = (ttSlot == zobrist());
    if (isHit) {
        ++root.tt.hits;
        RETURN_CUTOFF (child->searchIfLegal(ttSlot));
    }

    RETURN_CUTOFF (goodCaptures(child));
    RETURN_CUTOFF (safePromotions(child));
    RETURN_CUTOFF (notBadCaptures(child));
    RETURN_CUTOFF (allPromotions(child));

    return ReturnStatus::Continue;
}

ReturnStatus Node::goodCaptures(Node* child) {
    // MVV (most valuable victim) order
    for (Pi victim : OP.pieces() % PiMask{TheKing}) {
        Square to = ~OP.squareOf(victim);
        RETURN_CUTOFF (goodCaptures(child, to));
    }

    return ReturnStatus::Continue;
}

ReturnStatus Node::notBadCaptures(Node* child) {
    // MVV (most valuable victim) order
    for (Pi victim : OP.pieces() % PiMask{TheKing}) {
        Square to = ~OP.squareOf(victim);
        RETURN_CUTOFF (notBadCaptures(child, to));
    }

    return ReturnStatus::Continue;
}

ReturnStatus Node::allCaptures(Node* child) {
    // MVV (most valuable victim)
    for (Pi victim : OP.pieces() % PiMask{TheKing}) {
        Square to = ~OP.squareOf(victim);
        RETURN_CUTOFF (allCaptures(child, to));
    }

    return ReturnStatus::Continue;
}

ReturnStatus Node::goodCaptures(Node* child, Square to) {
    PiMask attackers = moves[to];

    if (!to.on(Rank8)) {
        // skip underpromotion pseudo moves
        attackers %= MY.promotables();
    }
    if (attackers.none()) { return ReturnStatus::Continue; }

    bool isVictimProtected = attackedSquares.has(to);
    assert (isVictimProtected == OP.attackersTo(~to).any());
    if (isVictimProtected) {
        // filter out more valuable attackers than the victim
        attackers &= MY.goodKillers( OP.typeOf(OP.pieceAt(~to)) );
    }

    while (attackers.any()) {
        // LVA (least valuable attacker)
        Pi attacker = attackers.leastValuable(); attackers -= attacker;

        Square from = MY.squareOf(attacker);
        RETURN_CUTOFF (child->searchMove(from, to));
    }

    return ReturnStatus::Continue;
}

ReturnStatus Node::notBadCaptures(Node* child, Square to) {
    PiMask attackers = moves[to];

    if (!to.on(Rank8)) {
        // skip underpromotion pseudo moves
        attackers %= MY.promotables();
    }
    if (attackers.none()) { return ReturnStatus::Continue; }

    bool isVictimProtected = attackedSquares.has(to);
    assert (isVictimProtected == OP.attackersTo(~to).any());
    if (isVictimProtected) {
        // filter out more valuable attackers than the victim
        attackers &= MY.notBadKillers( OP.typeOf(OP.pieceAt(~to)) );
    }

    while (attackers.any()) {
        // LVA (least valuable attacker)
        Pi attacker = attackers.leastValuable(); attackers -= attacker;

        Square from = MY.squareOf(attacker);
        RETURN_CUTOFF (child->searchMove(from, to));
    }

    return ReturnStatus::Continue;
}

ReturnStatus Node::allCaptures(Node* child, Square to) {
    PiMask attackers = moves[to];
    if (!to.on(Rank8)) {
        // skip underpromotion pseudo moves
        attackers %= MY.promotables();
    }

    while (attackers.any()) {
        // LVA (least valuable attacker)
        Pi attacker = attackers.leastValuable(); attackers -= attacker;

        Square from = MY.squareOf(attacker);
        RETURN_CUTOFF (child->searchMove(from, to));
    }

    return ReturnStatus::Continue;
}

// non capture queen promotions to the unattacked squares
ReturnStatus Node::safePromotions(Node* child) {
    for (Pi pi : MY.promotables()) {
        // skip moves to the attacked square
        for (Square to : moves[pi] % attackedSquares & Bb{Rank8}) {
            Square from = MY.squareOf(pi);
            RETURN_CUTOFF( child->searchMove(from, to) );
        }
    }

    return ReturnStatus::Continue;
}

// all non capture queen promotions
ReturnStatus Node::allPromotions(Node* child) {
    for (Pi pi : MY.promotables()) {
        for (Square to : moves[pi] & Bb{Rank8}) {
            Square from = MY.squareOf(pi);
            RETURN_CUTOFF( child->searchMove(from, to) );
        }
    }

    return ReturnStatus::Continue;
}

void Node::updateKillerMove() {
    if (!canBeKiller) { return; }
    if (!parent) { return; }

    if (parent->killer1 != childMove) {
        parent->killer2 = parent->killer1;
        parent->killer1 = childMove;
    }

    Move move = parent->childMove;
    PieceType ty = parent->MY.typeOf(move.from());
    root.counterMove.set(colorToMove(), ty, move.to(), childMove);
}

UciMove Node::uciMove(Square from, Square to) const {
    return UciMove{from, to, isSpecial(from, to), colorToMove(), root.uci.chessVariant()};
}

Color Node::colorToMove() const {
    return root.colorToMove(ply);
}

Score Node::evaluate() const {
    return Position::evaluate().clamp();
}

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

    const Z& z = zobrist();

    if (grandParent) {
        auto next = grandParent;
        while ((next = next->grandParent)) {
            if (next->zobrist() == z) {
                return true;
            }
            if (!next->repMask.has(z)) {
                return false;
            }
        }
    }

    return root.repetitions.has(colorToMove(), z);
}
