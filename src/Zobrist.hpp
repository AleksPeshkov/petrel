#ifndef ZOBRIST_HPP
#define ZOBRIST_HPP

#include "Index.hpp"

class Z {
public:
    using _t = u64_t;

protected:
    _t v_;

    constexpr Z& operator ^= (Z b) { v_ ^= b.v_; return *this; }
    constexpr friend Z operator ^ (Z a, Z b) { return Z{a} ^= b; }

public:
    explicit constexpr Z(_t n = 0) : v_{n} {}
    constexpr operator const _t& () const { return v_; }

    constexpr friend bool operator == (Z a, Z b) { return a.v_ == b.v_; }

    friend ostream& operator << (ostream& out, Z z) {
        auto flags = out.flags();

        out << " [" << std::hex << std::setw(16) << std::setfill('0') << "]";
        out << z.v_;

        out.flags(flags);
        return out;
    }
};

class Zobrist : public Z {
    //hand picked set of de Bruijn numbers
    enum : _t {
        ZQueen  = U64(0x0218'a392'cd5d'3dbf),
        ZRook   = U64(0x0245'30de'cb9f'8ead),
        ZBishop = U64(0x02b9'1efc'4b53'a1b3),
        ZKnight = U64(0x02dc'61d5'ecfc'9a51),
        ZPawn   = U64(0x031f'af09'dcda'2ca9),
        ZKing   = U64(0x0352'138a'fdd1'e65b),
        ZCastling = ZRook ^ ZPawn,
        ZEnPassant = ::rotateleft(ZPawn, 32), // A4 => A8
        // ZExtra  = U64(0x03ac'4dfb'4854'6797), // reserved
    };

    enum zobrist_index_t { Castling = 6, EnPassant = 7 };
    struct Index : ::Index<Index, 8> {
        using Base = ::Index<Index, 8>;
        constexpr Index (PieceType::_t ty) : Base{ty} {}
        constexpr Index (zobrist_index_t ty) : Base{ty} {}
        constexpr Index (PieceType ty) : Base{ty.v()} {}
        constexpr Index (NonKingType ty) : Base{ty.v()} {}
        constexpr Index (PromoType ty) : Base{ty.v()} {}
    };

    constexpr static Index::arrayOf<_t> zKey{
        ZQueen, ZRook, ZBishop, ZKnight, ZPawn, ZKing, ZCastling, ZEnPassant
    };

    constexpr static _t r(_t z, Square sq) { return ::rotateleft(z, sq.v()); }
    constexpr static _t z(Index ty, Square sq) { return r(zKey[ty], sq); }
    constexpr static _t flip(_t z) { return ::byteswap(z); }

    constexpr void my(Index ty, Square sq) { v_ ^= z(ty, sq); }
    constexpr void op(Index ty, Square sq) { v_ ^= flip(z(ty, sq)); }

public:
    using Z::Z;
    constexpr Zobrist (Zobrist my, Zobrist op) : Z{my.v_ ^ flip(op)} {
        static_assert (z(EnPassant, Square{A4}) == z(Pawn, Square{A8}));
        static_assert (z(EnPassant, Square{B4}) == z(Pawn, Square{B8}));
    }

    constexpr Zobrist& flip() { v_ = ::byteswap(v_); return *this; }

    void operator () (PieceType ty, Square sq) { my(ty, sq); }
    void castling(Square sq)  { assert (sq.on(Rank1)); my(Castling, sq); }
    void enPassant(Square sq) { assert (sq.on(Rank4)); my(EnPassant, sq); }

    void opCapture(NonKingType ty, Square sq) { op(ty, sq); }
    void opCastling(Square sq)  { assert (sq.on(Rank1)); op(Castling, sq); }
    void opEnPassant(Square sq) { assert (sq.on(Rank4)); op(EnPassant, sq); }

    void move(PieceType ty, Square from, Square to) {
        assert (from != to);
        my(ty, from);
        my(ty, to);
    }

    void promote(Square from, PromoType ty, Square to) {
        assert (from.on(Rank7) && to.on(Rank8));
        my(Pawn, from);
        my(ty, to);
    }

    void castle(Square kingFrom, Square kingTo, Square rookFrom, Square rookTo) {
        assert (kingFrom.on(Rank1) && kingTo.on(Rank1));
        assert (rookFrom.on(Rank1) && rookTo.on(Rank1));
        assert (kingFrom != rookFrom);
        assert (kingTo != rookTo);
        my(King, kingFrom);
        my(King, kingTo);
        my(Rook, rookFrom);
        my(Rook, rookTo);
    }

};

#endif
