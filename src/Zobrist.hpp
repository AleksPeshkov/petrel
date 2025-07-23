#ifndef ZOBRIST_HPP
#define ZOBRIST_HPP

#include "bitops.hpp"
#include "io.hpp"
#include "typedefs.hpp"

class Z {
public:
    typedef const Z& Arg;
    typedef u64_t _t;

protected:
    _t v;

public:
    constexpr Z(_t n = 0) : v{n} {}
    constexpr operator const _t& () const { return v; }

    constexpr Z& flip() { v = ::byteswap(v); return *this; }
    constexpr Z  operator * () const { return Z{*this}.flip(); }

    constexpr Z& operator ^= (Arg z) { v ^= z.v; return *this; }
    constexpr friend Z operator ^ (Arg a, Arg b) { return Z{a} ^= b; }

    constexpr friend bool operator == (Arg a, Arg b) { return a.v == b.v; }
    constexpr friend bool operator != (Arg a, Arg b) { return a.v != b.v; }

    friend out::ostream& operator << (out::ostream& out, Z z) {
        auto flags = out.flags();

        out << " [" << std::hex << std::setw(16) << std::setfill('0') << "]";
        out << static_cast<Z::_t>(z);

        out.flags(flags);
        return out;
    }
};

class Zobrist : public Z {
    typedef PieceZobristType Index;

protected:

    //hand picked set of de Bruijn numbers
    enum : _t {
        _Queen  = ULL(0x0218a392cd5d3dbf),
        _Rook   = ULL(0x024530decb9f8ead),
        _Bishop = ULL(0x02b91efc4b53a1b3),
        _Knight = ULL(0x02dc61d5ecfc9a51),
        _Pawn   = ULL(0x031faf09dcda2ca9),
        _King   = ULL(0x0352138afdd1e65b),
        _Extra  = ULL(0x03ac4dfb48546797),
        _Castling = _Extra ^ _Rook,
        _EnPassant = _Extra ^ _Pawn,
    };

    constexpr static const Index::arrayOf<_t> key = {{
        _Queen, _Rook, _Bishop, _Knight, _Pawn, _King, _Castling, _EnPassant
    }};

    constexpr static Z get(Index ty, Square sq) { return static_cast<Z>(::rotateleft(key[ty], sq)); }

public:
    using Z::Z;
    Zobrist (Arg my, Arg op) : Z{my ^ *op} {
        //static_assert (get(Queen, C3) == ~get(Queen, C6));
    }

    void change(Index ty, Square sq) { *this ^=  get(ty, sq); }
    void my(PieceType::_t ty, Square sq) { *this ^=  get(ty, sq); }
    void op(PieceType::_t ty, Square sq) { *this ^= *get(ty, sq); }
    void myCastling(Square sq)  { assert (sq.on(Rank1)); *this ^=  get(Castling, sq); }
    void opCastling(Square sq)  { assert (sq.on(Rank1)); *this ^= *get(Castling, sq); }
    void myEnPassant(Square sq) { assert (sq.on(Rank4)); *this ^=  get(EnPassant, sq); }
    void opEnPassant(Square sq) { assert (sq.on(Rank4)); *this ^= *get(EnPassant, sq); }

    void drop(PieceType ty, Square to) { change(static_cast<Index>(ty), to); }
    void clear(PieceType ty, Square from) { change(static_cast<Index>(ty), from); }

    void setCastling(Square sq)  { assert (sq.on(Rank1)); change(Castling, sq); }
    void clearCastling(Square sq) { setCastling(sq); }

    void setEnPassant(Square sq) { assert (sq.on(Rank4)); change(EnPassant, sq); }
    void clearEnPassant(Square sq) { setEnPassant(sq); }

    void move(PieceType ty, Square from, Square to) {
        assert (from != to);
        clear(ty, from);
        drop(ty, to);
    }

    void promote(Square from, PromoType::_t ty, Square to) {
        assert (from.on(Rank7) && to.on(Rank8));
        clear(Pawn, from);
        drop(ty, to);
    }

};

#endif
