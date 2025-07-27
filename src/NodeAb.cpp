#include "NodeAb.hpp"
#include "out.hpp"
#include "AttacksFrom.hpp"
#include "SearchRoot.hpp"
#include "PositionFen.hpp"

NodeControl NodeAb::visit(Move move) {
    alpha = -parent->beta;
    beta = -parent->alpha;
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    // mate-distance pruning
    alpha = std::max(alpha, Score::checkmated(ply));
    if (alpha >= beta) { return NodeControl::BetaCutoff; }

    score = NoScore;
    draft = parent->draft > 0 ? parent->draft-1 : 0;

    RETURN_IF_ABORT (root.countNode());
    parent->currentMove = parent->uciMove(move);
    makeMove(parent, move);
    root.pvMoves.set(ply, UciMove{});
    ++parent->movesMade;

    bool inCheck = NodeAb::inCheck();

    if (ply == MaxPly) {
        // no room to search deeper
        score = evaluate();
    }
    else if (draft == 0 && !inCheck) {
        RETURN_IF_ABORT (quiescence());
    }
    else {
        RETURN_IF_ABORT (visitChildren());
        if (movesMade == 0) {
            //checkmated or stalemated
            score = inCheck ? Score::checkmated(ply) : Score{DrawScore};
        }
    }

    return parent->negamax(-score);
}

NodeControl NodeAb::negamax(Score lastScore) {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    if (lastScore > score) {
        score = lastScore;

        if (score >= beta) {
            //beta cut off
            return NodeControl::BetaCutoff;
        }

        if (score > alpha) {
            alpha = score;

            root.pvMoves.set(ply, currentMove);
            if (ply == 0) {
                root.infoNewPv(draft, score);
            }
        }
    }

    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);
    return NodeControl::Continue;
}

NodeControl NodeAb::visitChildren() {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);

    NodeAb node{this};
    const auto child = &node;

    CUTOFF (child->visitIfLegal(root.pvMoves[ply]));

    CUTOFF (goodCaptures(child));

    // the rest of the remaining unsorted moves
    for (Pi pi : MY.pieces()) {
        Square from = MY.squareOf(pi);

        for (Square to : moves[pi]) {
            CUTOFF (child->visit({from, to}));
        }
    }

    return NodeControl::Continue;
}

NodeControl NodeAb::quiescence() {
    assert (MinusInfinity <= alpha && alpha < beta && beta <= PlusInfinity);
    assert (!inCheck());

    //stand pat
    score = evaluate();
    if (beta <= score) {
        return NodeControl::BetaCutoff;
    }
    if (alpha < score) {
        alpha = score;
    }

    NodeAb node{this};
    const auto child = &node;

    CUTOFF (goodCaptures(child));

    return NodeControl::Continue;
}

NodeControl NodeAb::goodCaptures(NodeAb* child) {
    // MVV (most valuable victim)
    for (Pi victim : OP.pieces() % PiMask{TheKing}) {
        Square to = ~OP.squareOf(victim);
        PiMask attackers = moves[to];
        if (attackers.none()) { continue; }

        bool isVictimProtected = isOpAttacks(to);
        if (!isVictimProtected) {
            assert (OP.attackersTo(~to).none());
        }
        else {
            assert (OP.attackersTo(~to).any());
            // BLIND static exchange evaluation
            // filter out more valuable attackers than the victim
            attackers &= MY.goodKillers(OP.typeOf(victim));
        }

        //LVA (least valuable attacker)
        while (attackers.any()) {
            Pi attacker = attackers.least(); attackers -= attacker;

            Square from = MY.squareOf(attacker);

            if (MY.isPromotable(attacker) && !to.on(Rank8)) {
                // skip underpromotion pseudo moves
                continue;
            }

            CUTOFF (child->visit(Move{from, to}));
        }
    }

    // non capture promotions
    for (Pi pi : MY.promotables()) {
        for (Square to : moves[pi] & Bb{Rank8}) {
            // skip move to the defended square
            if (isOpAttacks(to)) { continue; }

            Square from = MY.squareOf(pi);
            // to the queen
            CUTOFF( child->visit({from, to}) );

            // underpromotion to the knight only if it makes check
            if (attacksFrom(Knight, ~OP.squareOf(TheKing)).has(to)) {
                Square toKnight{ File{to}, ::rankOf(Knight) };
                CUTOFF( child->visit({from, toKnight}) );
            }
        }
    }

    return NodeControl::Continue;
}

UciMove NodeAb::uciMove(Square from, Square to) const {
    return UciMove{from, to, isSpecial(from, to), colorToMove(), root.position.getChessVariant()};
}

Color NodeAb::colorToMove() const {
    return root.position.getColorToMove(ply);
}

Score NodeAb::evaluate()
{
    return Position::evaluate().clamp();
}
