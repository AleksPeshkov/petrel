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

    const Index::arrayOf<_t> key = {{
        _Queen, _Rook, _Bishop, _Knight, _Pawn, _King, _Castling, _EnPassant
    }};

    static constexpr _t rol(_t n, Square::_t sq) { return n << sq | n >> (64-sq); }

public:
    constexpr _t operator() (Index i, Square sq) const { return rol(key[i], sq); }

};

extern const ZobristKey zobristKey;

#endif
