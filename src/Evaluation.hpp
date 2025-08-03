#ifndef EVALUATION_HPP
#define EVALUATION_HPP

#include "typedefs.hpp"
#include "Score.hpp"

//https://www.chessprogramming.org/PeSTO%27s_Evaluation_Function
class PieceSquareTable {
public:
    union element_type {
        struct PACKED {
            u16_t openingPst:14;
            u16_t endgamePst:14;

            u8_t queens:4;  // number of queens
            u8_t rooks:4;   // number of rooks
            u8_t bishops:4; // number of bishops
            u8_t knights:4; // number of knights
            u8_t pawns:4;   // number of pawns

            u8_t piecesMat:8; // sum of non pawn pieces material points (pawn = 1)
            u8_t totalMat:8;  // sum of all pieces material points (pawn = 1)
        } s;
        u64_t v;

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

    // https://www.chessprogramming.org/PeSTO%27s_Evaluation_Function
    static Score evaluate(const Evaluation& my, const Evaluation& op) {
        constexpr const unsigned PieceMatMax = 32; // initial chess position sum of non pawn pieces material points

        auto myMaterial = std::min<unsigned>(my.v.s.piecesMat, PieceMatMax);
        auto opMaterial = std::min<unsigned>(op.v.s.piecesMat, PieceMatMax);

        auto myScore = my.v.s.openingPst * myMaterial + my.v.s.endgamePst * (PieceMatMax-myMaterial);
        auto opScore = op.v.s.openingPst * opMaterial + op.v.s.endgamePst * (PieceMatMax-opMaterial);

        return static_cast<Score>((myScore - opScore) / static_cast<Score>(PieceMatMax));
    }

    void drop(PieceType ty, Square t) { to(ty, t); }
    void capture(PieceType ty, Square f) { from(ty, f); }
    void move(PieceType ty, Square f, Square t) { assert (f != t); from(ty, f); to(ty, t); }

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
            case Queen:
                return v.s.queens;
            case Rook:
                return v.s.rooks;
            case Bishop:
                return v.s.bishops;
            case Knight:
                return v.s.knights;
            case Pawn:
                return v.s.pawns;
            default:
                assert (false);
                return 0;
        }
    }

    // 10, 5, 3, 3, 0
    constexpr index_t piecesMat() const {
        return v.s.piecesMat;
    }

    // 12, 6, 4, 4, 1
    constexpr index_t material() const {
        return v.s.totalMat;
    }

    // any queen, rook or pawn
    constexpr bool hasMatingPieces() const {
        return (v.s.queens | v.s.rooks | v.s.pawns) != 0;
    }

};

#endif
