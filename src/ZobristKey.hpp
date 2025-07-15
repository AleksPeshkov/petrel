#ifndef ZOBRIST_KEY_HPP
#define ZOBRIST_KEY_HPP

#include "typedefs.hpp"

class ZobristKey {
public:
    typedef u64_t _t;
    typedef PieceZobristType Index;

private:
    //hand picked set of de Bruijn numbers
    enum : _t {
        _Queen  = 0x0218a392cd5d3dbfull,
        _Rook   = 0x024530decb9f8eadull,
        _Bishop = 0x02b91efc4b53a1b3ull,
        _Knight = 0x02dc61d5ecfc9a51ull,
        _Pawn   = 0x031faf09dcda2ca9ull,
        _King   = 0x0352138afdd1e65bull,
        _Extra  = 0x03ac4dfb48546797ull,
        _Castling = _Extra ^ _Rook,
        _EnPassant = _Extra ^ _Pawn,
    };

    const Index::arrayOf<_t> key = {{
        _Queen, _Rook, _Bishop, _Knight, _Pawn, _King, _Castling, _EnPassant
    }};

    static constexpr _t rol(_t n, Square::_t sq) { return n << sq | n >> (64-sq); }

public:
    constexpr _t operator() (Index i, Square sq) const { return rol(key[i], sq); }

};

extern const ZobristKey zobristKey;

#endif
