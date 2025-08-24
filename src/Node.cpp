#include "Node.hpp"
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
    PositionMoves{}, parent{n}, grandParent{n->parent}, root{n->root},
    ply{n->ply + 1}, draft{n->draft > 0 ? n->draft-1 : 0},
    alpha{-n->beta}, beta{-n->alpha},
    killer1{grandParent ? grandParent->killer1 : Move{}},
    killer2{grandParent ? grandParent->killer2 : Move{}}
{}

ReturnStatus Node::searchRoot() {
    root.newSearch();
    root.nodeCounter = { root.limits.nodes };

    auto rootMovesClone = moves();
    repMask = root.repetitions.repMask(colorToMove());
    origin = root.tt.prefetch<TtSlot>(zobrist());

    if (root.limits.iterationDeadlineReached()) {
        // we have no time to search, return TT move immediately if found
        ++root.tt.reads;
        ttSlot = *origin;
        isHit = (ttSlot == zobrist());
        if (isHit && static_cast<Move>(ttSlot)) {
            ++root.tt.hits;
            score = ttSlot;
            root.pvMoves.set(ply, uciMove(uciMove(ttSlot)));
            return ReturnStatus::Stop;
        }
    }

    for (draft = 1; draft <= root.limits.depth; ++draft) {
        setMoves(rootMovesClone);
        alpha = MinusInfinity;
        beta = PlusInfinity;
        auto status = searchMoves();

        root.newIteration();
        updateTtPv();

        if (status == ReturnStatus::Stop) { return ReturnStatus::Stop; }

        root.uci.info_iteration(draft);

        if (root.limits.iterationDeadlineReached()) { return ReturnStatus::Stop; }
    }

    if (root.limits.infinite || root.limits.ponder) {
        root.uci.waitStop();
    }

    return ReturnStatus::Continue;
}

 // refresh PV in TT if it was overwritten
 void Node::updateTtPv() {
    Position pos{root};
    Score s = score;
    Ply d = draft;

    const Move* pv = root.pvMoves;
    for (Move move; (move = *pv++);) {
        auto o = root.tt.addr<TtSlot>(pos.zobrist());
        *o = TtSlot{pos.zobrist(), s, Exact, move, d};
        ++root.tt.writes;

        //we cannot use makeZobrist() because of en passant legality validation
        pos.makeMove(move.from(), move.to());
        s = -s;
        d = d > 0 ? d-1 : 0;
    }
}

void Node::makeMove(Move move) {
    Square from = move.from();
    Square to = move.to();

    parent->childMove = move;
    parent->clearMove(from, to);
    Position::makeMove(parent, from, to);
    origin = root.tt.prefetch<TtSlot>(zobrist());

    if (rule50() <= 1) { repMask = RepetitionMask{}; }
    else if (grandParent) { repMask = RepetitionMask{grandParent->repMask, grandParent->zobrist()}; }
    else { repMask = root.repetitions.repMask(colorToMove()); }

    root.pvMoves.set(ply, UciMove{});
}

ReturnStatus Node::searchMove(Move move) {
    RETURN_IF_STOP (root.countNode());
    makeMove(move);

    RETURN_IF_STOP (search());
    return parent->negamax(this);
}

ReturnStatus Node::negamax(Node* child) {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    auto childScore = -child->score;

    if (beta <= childScore) {
        score = childScore;
        return betaCutoff();
    }

    if (alpha < childScore) {
        if (child->isZeroWindow) {
            // Principal Variation Search:
            // zero window search failed high, research with full window
            child->alpha = -beta;
            child->beta  = -alpha;
            child->isZeroWindow = false;
            RETURN_IF_STOP (child->search());
            return negamax(child);
        }

        score = childScore;
        alpha = childScore;
        RETURN_IF_STOP (updatePv());
    } else if (score < childScore) {
        score = childScore;
    }

    // set window for the next move search
    //child->alpha = -beta;
    child->alpha = -alpha - 1;
    child->beta  = -alpha;
    child->isZeroWindow = true;
    return ReturnStatus::Continue;
}

ReturnStatus Node::betaCutoff() {
    updateKillerMove();
    *origin = TtSlot{this, UpperBound};
    ++root.tt.writes;
    return ReturnStatus::BetaCutoff;
}

ReturnStatus Node::updatePv() {
    root.pvMoves.set(ply, uciMove(childMove));
    *origin = TtSlot{this, Exact};
    ++root.tt.writes;

    if (ply == 0) {
        root.uci.info_pv(draft, score);
        if (root.limits.updatePvDeadlineReached()) { return ReturnStatus::Stop; }
    }
    return ReturnStatus::Continue;
}

ReturnStatus Node::search() {
    if (rule50() >= 100 || isRepetition() || isDrawMaterial()) {
        score = DrawScore;
        return ReturnStatus::Continue;
    }

    generateMoves();
    return searchMoves();
}

ReturnStatus Node::searchMoves() {
    // mate-distance pruning
    alpha = std::max(alpha, Score::checkmated(ply));
    if (alpha >= beta) { score = alpha; return ReturnStatus::BetaCutoff; }

    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    if (moves().none()) {
        // checkmate or stalemate
        score = inCheck() ? Score::checkmated(ply) : Score{DrawScore};
        return ReturnStatus::Continue;
    }

    if (ply == MaxPly) {
        // no room to search deeper
        score = evaluate();
        return ReturnStatus::Continue;
    }

    if (draft == 0 && !inCheck()) {
        return quiescence();
    }

    Node node{this};
    const auto child = &node;

    score = NoScore;
    canBeKiller = false;

    ++root.tt.reads;
    ttSlot = *origin;
    isHit = (ttSlot == zobrist());
    if (isHit) {
        ++root.tt.hits;
        RETURN_CUTOFF (child->searchIfLegal(ttSlot));
    }

    PiMask victims = OP.pieces() - PiMask{TheKing};
    RETURN_CUTOFF (goodCaptures<false>(child, victims));

    canBeKiller = true;

    Pi lastPi = TheKing;
    Bb newMoves = {};

    //TODO: checking moves

    if (parent) {
        // killer move to be tried first
        RETURN_CUTOFF (child->searchIfLegal(parent->killer1));

        // counter moves may refute the last opponent move
        Move move = parent->childMove;
        PieceType ty = parent->MY.typeAt(move.from());
        RETURN_CUTOFF (child->searchIfLegal( root.counterMove(colorToMove(), ty, move.to()) ));

        RETURN_CUTOFF (child->searchIfLegal(parent->killer2));

        // try quiet moves of the last moved piece (unless it was captured)
        {
            Square from = parent->movedPieceTo();
            if (MY.bbSide().has(from)) {
                // last moved piece
                lastPi = MY.pieceAt(from);

                // new moves of the last moved piece
                newMoves = movesOf(lastPi);

                if (from != parent->movedPieceFrom()) {
                    // unless it was a pawn promotion move
                    newMoves %= parent->MY.attacksOf(lastPi);
                }

                // try new safe moves of the last moved piece
                for (Square to : newMoves % bbAttacked()) {
                    RETURN_CUTOFF (child->searchMove(from, to));
                }

                // keep unsafe news moves for later
                newMoves &= bbAttacked();
            }
        }

        // new safe quiet moves, except for the last moved piece (or king)
        for (Pi pi : MY.pieces() - lastPi) {
            Square from = MY.squareOf(pi);
            for (Square to : movesOf(pi) % parent->MY.attacksOf(pi) % bbAttacked()) {
                RETURN_CUTOFF (child->searchMove(from, to));
            }
        }
    }

    // all the rest safe quiet moves
    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);
        for (Square to : movesOf(pi) % bbAttacked()) {
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
        for (Square to : movesOf(pi)) {
            RETURN_CUTOFF (child->searchMove(from, to));
        }
    }

    // unsafe (bad) captures
    RETURN_CUTOFF (allCaptures<false>(child, victims));

    // all the rest moves, including underpromotions with or without capture, LVA order
    auto pieces = MY.pieces();
    while (pieces.any()) {
        Pi pi = pieces.leastValuable(); pieces -= pi;
        Square from = MY.squareOf(pi);

        for (Square to : movesOf(pi)) {
            RETURN_CUTOFF (child->searchMove(from, to));
        }
    }

    return ReturnStatus::Continue;
}

ReturnStatus Node::quiescence() {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);
    assert (!inCheck());

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

    canBeKiller = false;

    ++root.tt.reads;
    ttSlot = *origin;
    isHit = (ttSlot == zobrist());
    if (isHit) {
        ++root.tt.hits;
        RETURN_CUTOFF (child->searchIfLegal(ttSlot));
    }

    // Delta prunning
    PieceType lowestVictimType = ::deltaPrunning(alpha - score);
    PiMask victims = OP.pieces() % OP.lessValue(lowestVictimType);

    RETURN_CUTOFF (goodCaptures<false>(child, victims));
    RETURN_CUTOFF (allCaptures<false>(child, victims));

    return ReturnStatus::Continue;
}

template <bool inQS>
ReturnStatus Node::goodCaptures(Node* child, const PiMask& victims) {
    if (MY.promotables().any()) {
        // queen promotion with capture, always good
        for (Pi victim : OP.pieces() & OP.piecesOn(Rank8)) {
            Square to = ~OP.squareOf(victim);
            for (Pi attacker : MY.attackersTo(to) & MY.promotables()) {
                Square from = MY.squareOf(attacker);
                RETURN_CUTOFF (child->searchMove(from, to));
            }
        }

        // queen promotion without capture
        if (MY.promotables().any()) {
            // queen promotions without capture
            for (Pi pawn : MY.promotables()) {
                Square from = MY.squareOf(pawn);
                Square to{File{from}, Rank8};
                RETURN_CUTOFF (child->searchIfLegal({from, to}));
            }
        }
    }

    // MVV (most valuable victim) order
    for (Pi victim : victims) {
        Square to = ~OP.squareOf(victim);
        RETURN_CUTOFF (goodCaptures<inQS>(child, to));
    }

    return ReturnStatus::Continue;
}

template <bool inQS>
ReturnStatus Node::goodCaptures(Node* child, Square to) {
    // exclude promotions
    PiMask attackers = canMoveTo(to) % MY.promotables();
    if (attackers.none()) { return ReturnStatus::Continue; }

    Score victimValue = OP.scoreAt(~to);
    Score delta = alpha - score;
    assert (delta >= 0);

    // delta prunning
    // after this capture opponent will fail high with stand pat score >= beta
    // so we just skip this insufficient valued victim capture
    if (victimValue < delta) {
        if constexpr (inQS) {
            for (Pi attacker : attackers) {
                // remove insufficient capture completelly from QS
                clearMove(attacker, to);
            }
        }
        return ReturnStatus::Continue;
    }

    if (attackers.isSingleton()) {
        Pi attacker = attackers.index();
        Square from = MY.squareOf(attacker);
        if (bbAttacked().has(to)) {
            //TODO: check X-Ray attacks
            // attacker is singleton and victim is protected
            // skip captures of the more valuable attacker
            if (victimValue <= MY.scoreAt(from) + delta) {
                if constexpr (inQS) { clearMove(attacker, to); }
                return ReturnStatus::Continue;
            }
        }

        return child->searchMove(from, to);
    }

    auto opPawnAttacks = OP.bbPawns().pawnAttacks();
    if (opPawnAttacks.has(~to)) {
        // victim is protected by at least one pawn
        // try captures of less or equal value types of attackers
        attackers &= MY.lessOrEqualValue( OP.typeOf(OP.pieceAt(~to)) );

        while (attackers.any()) {
            // LVA (least valuable attacker) order
            Pi attacker = attackers.leastValuable(); attackers -= attacker;

            Square from = MY.squareOf(attacker);
            if (victimValue > MY.scoreAt(from) + delta) {
                return child->searchMove(from, to);
            }
        }
    }

    // case of non pawn defenders
    // TODO: more exact SEE

    while (attackers.any()) {
        // LVA (least valuable attacker) order
        Pi attacker = attackers.leastValuable(); attackers -= attacker;

        Square from = MY.squareOf(attacker);
        RETURN_CUTOFF (child->searchMove(from, to));
    }

    return ReturnStatus::Continue;
}

template <bool inQS>
ReturnStatus Node::allCaptures(Node* child, const PiMask& victims) {
    // MVV (most valuable victim)
    for (Pi victim : victims) {
        Square to = ~OP.squareOf(victim);

        if constexpr (inQS) {
            // delta prunning
            Score victimValue = OP.scoreAt(~to);
            Score delta = alpha - score;
            if (victimValue < delta) { continue; }
        }

        RETURN_CUTOFF (allCaptures<inQS>(child, to));
    }

    return ReturnStatus::Continue;
}

template <bool inQS>
ReturnStatus Node::allCaptures(Node* child, Square to) {
    // exclude promotions
    PiMask attackers = canMoveTo(to) % MY.promotables();

    while (attackers.any()) {
        // LVA (least valuable attacker)
        Pi attacker = attackers.leastValuable(); attackers -= attacker;

        Square from = MY.squareOf(attacker);
        RETURN_CUTOFF (child->searchMove(from, to));
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
    PieceType ty = parent->MY.typeAt(move.from());
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
