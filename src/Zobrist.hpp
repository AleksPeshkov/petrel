#ifndef ZOBRIST_HPP
#define ZOBRIST_HPP

#include "bitops.hpp"
#include "io.hpp"
#include "typedefs.hpp"

using z_t = u64_t;

class Z;
using ZArg = const Z&;

class Z {
    using Arg = ZArg;

protected:
    z_t v;

    constexpr Z& operator ^= (Arg b) { v ^= b.v; return *this; }
    constexpr friend Z operator ^ (Arg a, Arg b) { return Z{a} ^= b; }

public:
    constexpr Z(z_t n = 0) : v{n} {}
    constexpr operator const z_t& () const { return v; }

    constexpr friend bool operator == (Arg a, Arg b) { return a.v == b.v; }
    constexpr friend bool operator != (Arg a, Arg b) { return a.v != b.v; }

    friend ostream& operator << (ostream& out, Z z) {
        auto flags = out.flags();

        out << " [" << std::hex << std::setw(16) << std::setfill('0') << "]";
        out << static_cast<z_t>(z);

        out.flags(flags);
        return out;
    }
};

class Zobrist : public Z {
    using Arg = const Zobrist&;

    //hand picked set of de Bruijn numbers
    enum : z_t {
        ZQueen  = U64(0x0218a392cd5d3dbf),
        ZRook   = U64(0x024530decb9f8ead),
        ZBishop = U64(0x02b91efc4b53a1b3),
        ZKnight = U64(0x02dc61d5ecfc9a51),
        ZPawn   = U64(0x031faf09dcda2ca9),
        ZKing   = U64(0x0352138afdd1e65b),
        ZCastling = ZRook ^ ZPawn,
        ZEnPassant = ::rotateleft(ZPawn, 32), // A4 => A8
        // ZExtra  = U64(0x03ac4dfb48546797), // reserved
    };

    enum { Castling = 6, EnPassant = 7 };
    using Index = ::Index<8>;

    inline static constexpr Index::arrayOf<z_t> zKey = {{
        ZQueen, ZRook, ZBishop, ZKnight, ZPawn, ZKing, ZCastling, ZEnPassant
    }};

    constexpr static z_t r(const z_t& z, Square::_t sq) { return ::rotateleft(z, sq); }
    constexpr static z_t z(Index ty, Square::_t sq) { return r(zKey[ty], sq); }
    constexpr static z_t flip(z_t z) { return ::byteswap(z); }

    constexpr void my(Index ty, Square sq) { v ^= z(ty, sq); }
    constexpr void op(Index ty, Square sq) { v ^= flip(z(ty, sq)); }

public:
    using Z::Z;
    constexpr Zobrist (Arg my, Arg op) : Z{my.v ^ flip(op)} {
        static_assert (z(EnPassant, A4) == z(Pawn, A8));
        static_assert (z(EnPassant, B4) == z(Pawn, B8));
    }

    constexpr Zobrist& flip() { v = ::byteswap(v); return *this; }

    void operator () (PieceType::_t ty, Square sq) { my(Index{ty}, sq); }
    void castling(Square sq)  { assert (sq.on(Rank1)); my(Castling, sq); }
    void enPassant(Square sq) { assert (sq.on(Rank4)); my(EnPassant, sq); }

    void op(PieceType::_t ty, Square sq) { op(Index{ty}, sq);}
    void opCastling(Square sq)  { assert (sq.on(Rank1)); op(Castling, sq); }
    void opEnPassant(Square sq) { assert (sq.on(Rank4)); op(EnPassant, sq); }

    void move(PieceType::_t ty, Square from, Square to) {
        assert (from != to);
        my(ty, from);
        my(ty, to);
    }

    void promote(Square from, PromoType::_t ty, Square to) {
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
