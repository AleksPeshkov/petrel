#include "NodeAb.hpp"
#include "out.hpp"
#include "SearchControl.hpp"
#include "UciSearchInfo.hpp"

NodeControl NodeAb::visit(Square from, Square to) {
    auto& p = static_cast<NodeAb&>(parent);

    RETURN_IF_ABORT (control.countNode());
    makeMove(p, from, to);

    if (movesCount() == 0 || draft == 0) {
        score = staticEval();
    }
    else {
        alpha = -p.beta;
        beta = -p.alpha;
        RETURN_IF_ABORT (visitChildren());
    }

    if (p.score < -score) {
        p.score = -score;

        if (p.alpha < p.score) {
            p.alpha = p.score;

            if (p.alpha >= p.beta) {
                //beta cut off
                return NodeControl::BetaCutoff;
            }

            control.setPv(ply-1, {from, to});

            //out::space(std::cout, ply-1, '*') << " " << ply-1 << " ";
            //control.info.write(std::cout, {p, from, to}, ply-1) << " pv";
            //control.info.write(std::cout, control.getPv(ply-1), ply-1) << std::endl;
        }
    }

    return NodeControl::Continue;
}

NodeControl NodeAb::visitChildren() {
    auto my = (*this)[My];

    score = Score::Minimum;
    auto child = NodeAb{*this};

    const Move& move = control.getPv(0)[ply];
    if (move) {
        Square from = move.from();
        Square to = move.to();
        if (isLegalMove(from, to)) {
            CUTOFF (child.visit(from, to));
        }
    }

    for (Pi pi : my.pieces()) {
        Square from = my.squareOf(pi);

        for (Square to : moves[pi]) {
            CUTOFF (child.visit(from, to));
        }
    }

    return NodeControl::Continue;
}
