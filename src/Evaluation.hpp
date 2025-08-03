#ifndef EVALUATION_HPP
#define EVALUATION_HPP

#include "bitops128.hpp"
#include "typedefs.hpp"
#include "Score.hpp"

class PieceSquareTable {
public:
    union element_type {
        struct PACKED {
            u16_t openingPst;  // full set of pieces and pawns, king safety bonus
            u16_t noPiecesPst; // no pieces, pawn advancement bonus
            u16_t noPawnsPst;  // no pawns, centralization bonus
            u8_t pawns; // number of pawns

            u8_t knights; // number of knights
            u8_t bishops; // number of bishops
            u8_t rooks; // number of rooks
            u8_t queens; // number of queens

            u8_t pieceMat; // sum of non pawn pieces material points (pawn = 1)
            u8_t totalMat; // sum of all pieces material points (pawn = 1)
        } s;
        vu64x2_t v;

        constexpr const element_type& operator += (const element_type& o) { v += o.v; return *this; }
        constexpr const element_type& operator -= (const element_type& o) { v -= o.v; return *this; }
    };

protected:

    PieceType::arrayOf< Square::arrayOf<element_type> > pst;

public:
    PieceSquareTable ();

    constexpr const element_type& operator() (PieceType ty, Square sq) const { return pst[ty][sq]; }
};

extern const PieceSquareTable pieceSquareTable;

class Evaluation {
public:
    typedef PieceSquareTable::element_type _t;

private:
    _t v;

    constexpr void from(PieceType::_t ty, Square f) { v -= pieceSquareTable(ty, f); }
    constexpr void to(PieceType::_t ty, Square t) { v += pieceSquareTable(ty, t); }

public:
    constexpr Evaluation () : v{} {}

    static Score evaluate(const Evaluation& my, const Evaluation& op) {
        constexpr const unsigned PieceMatMax = 32; // initial chess position sum of non pawn pieces material points

        auto myMaterial = std::min<unsigned>(my.v.s.pieceMat, PieceMatMax);
        auto opMaterial = std::min<unsigned>(op.v.s.pieceMat, PieceMatMax);

        auto myScore = my.v.s.openingPst * opMaterial + my.v.s.noPiecesPst * (PieceMatMax-opMaterial);
        auto opScore = op.v.s.openingPst * myMaterial + op.v.s.noPiecesPst * (PieceMatMax-myMaterial);

        return static_cast<Score>((myScore - opScore) / static_cast<Score>(PieceMatMax));
    }

    void drop(PieceType ty, Square t) { to(ty, t); }
    void capture(PieceType ty, Square f) { from(ty, f); }
    void move(PieceType ty, Square f, Square t) { assert (f != t); from(ty, f); to(ty, t); }

    //TRICK: removing pawn does not alter material value
    void promote(Square f, Square t, PromoType ty) {
        assert (f.on(Rank7) && t.on(Rank8));
        from(Pawn, f);
        to(ty, t);
    }

    void castle(Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
        assert (kingFrom != rookFrom);
        assert (kingTo != rookTo);

        from(King, kingFrom); from(Rook, rookFrom);
        to(Rook, rookTo); to(King, kingTo);
    }

    constexpr index_t count(PieceType ty) const {
        switch (ty) {
            case Pawn:
                return v.s.pawns;
            case Knight:
                return v.s.knights;
            case Bishop:
                return v.s.bishops;
            case Rook:
                return v.s.rooks;
            case Queen:
                return v.s.queens;
            default:
                assert (false);
                return 0;
        }
    }

    constexpr index_t material() const {
        return v.s.totalMat;
    }

};

#endif
