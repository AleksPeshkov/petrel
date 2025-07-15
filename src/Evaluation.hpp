#ifndef EVALUATION_HPP
#define EVALUATION_HPP

#include "typedefs.hpp"
#include "Score.hpp"
#include "PieceSquareTable.hpp"

class Evaluation {
public:
    typedef PieceSquareTable::_t _t;

private:
    _t v;

    constexpr void from(PieceType ty, Square f) { v -= pieceSquareTable(ty, f); }
    constexpr void to(PieceType ty, Square t) { v += pieceSquareTable(ty, t); }

public:
    constexpr Evaluation () : v{} {}

    // https://www.chessprogramming.org/PeSTO%27s_Evaluation_Function
    static Score evaluate(const Evaluation& my, const Evaluation& op) {
        constexpr const u16_t PieceMatMax = 32; // initial sum of non pawn pieces material points

        auto myMaterial = std::min(my.v.s.pieceMat, PieceMatMax);
        auto opMaterial = std::min(op.v.s.pieceMat, PieceMatMax);

        auto myScore = static_cast<signed>(my.v.s.openingPst * opMaterial + my.v.s.endgamePst * (PieceMatMax-opMaterial));
        auto opScore = static_cast<signed>(op.v.s.openingPst * myMaterial + op.v.s.endgamePst * (PieceMatMax-myMaterial));

        return static_cast<Score>((myScore - opScore) / static_cast<signed>(PieceMatMax));
    }

    void drop(PieceType ty, Square t) { to(ty, t); }
    void capture(PieceType ty, Square f) { from(ty, f); }
    void move(PieceType ty, Square f, Square t) { assert (f != t); from(ty, f); to(ty, t); }

    //TRICK: removing pawn does not alter material value
    void promote(Square f, Square t, PromoType ty) {
        assert (f.on(Rank7) && t.on(Rank8));
        from(Pawn, f);
        to(static_cast<PieceType::_t>(ty), t);
    }

    void castle(Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
        assert (kingFrom != rookFrom);
        assert (kingTo != rookTo);

        from(King, kingFrom); from(Rook, rookFrom);
        to(Rook, rookTo); to(King, kingTo);
    }

};

#endif
