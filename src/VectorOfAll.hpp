#ifndef VECTOR_OF_ALL_HPP
#define VECTOR_OF_ALL_HPP

#include "bitops128.hpp"
#include "typedefs.hpp"

constexpr u8x16_t all(u8_t i) { return u8x16_t{ i,i,i,i, i,i,i,i, i,i,i,i, i,i,i,i }; }

class VectorOfAll {
    typedef u8x16_t _t;
    typedef Index<0x100> ByteIndex;

    ByteIndex::arrayOf<_t> v;

public:
    constexpr VectorOfAll () : v {
        all(0x00), all(0x01), all(0x02), all(0x03), all(0x04), all(0x05), all(0x06), all(0x07), all(0x08), all(0x09), all(0x0a), all(0x0b), all(0x0c), all(0x0d), all(0x0e), all(0x0f),
        all(0x10), all(0x11), all(0x12), all(0x13), all(0x14), all(0x15), all(0x16), all(0x17), all(0x18), all(0x19), all(0x1a), all(0x1b), all(0x1c), all(0x1d), all(0x1e), all(0x1f),
        all(0x20), all(0x21), all(0x22), all(0x23), all(0x24), all(0x25), all(0x26), all(0x27), all(0x28), all(0x29), all(0x2a), all(0x2b), all(0x2c), all(0x2d), all(0x2e), all(0x2f),
        all(0x30), all(0x31), all(0x32), all(0x33), all(0x34), all(0x35), all(0x36), all(0x37), all(0x38), all(0x39), all(0x3a), all(0x3b), all(0x3c), all(0x3d), all(0x3e), all(0x3f),

        all(0x40), all(0x41), all(0x42), all(0x43), all(0x44), all(0x45), all(0x46), all(0x47), all(0x48), all(0x49), all(0x4a), all(0x4b), all(0x4c), all(0x4d), all(0x4e), all(0x4f),
        all(0x50), all(0x51), all(0x52), all(0x53), all(0x54), all(0x55), all(0x56), all(0x57), all(0x58), all(0x59), all(0x5a), all(0x5b), all(0x5c), all(0x5d), all(0x5e), all(0x5f),
        all(0x60), all(0x61), all(0x62), all(0x63), all(0x64), all(0x65), all(0x66), all(0x67), all(0x68), all(0x69), all(0x6a), all(0x6b), all(0x6c), all(0x6d), all(0x6e), all(0x6f),
        all(0x70), all(0x71), all(0x72), all(0x73), all(0x74), all(0x75), all(0x76), all(0x77), all(0x78), all(0x79), all(0x7a), all(0x7b), all(0x7c), all(0x7d), all(0x7e), all(0x7f),

        all(0x80), all(0x81), all(0x82), all(0x83), all(0x84), all(0x85), all(0x86), all(0x87), all(0x88), all(0x89), all(0x8a), all(0x8b), all(0x8c), all(0x8d), all(0x8e), all(0x8f),
        all(0x90), all(0x91), all(0x92), all(0x93), all(0x94), all(0x95), all(0x96), all(0x97), all(0x98), all(0x99), all(0x9a), all(0x9b), all(0x9c), all(0x9d), all(0x9e), all(0x9f),
        all(0xa0), all(0xa1), all(0xa2), all(0xa3), all(0xa4), all(0xa5), all(0xa6), all(0xa7), all(0xa8), all(0xa9), all(0xaa), all(0xab), all(0xac), all(0xad), all(0xae), all(0xaf),
        all(0xb0), all(0xb1), all(0xb2), all(0xb3), all(0xb4), all(0xb5), all(0xb6), all(0xb7), all(0xb8), all(0xb9), all(0xba), all(0xbb), all(0xbc), all(0xbd), all(0xbe), all(0xbf),

        all(0xc0), all(0xc1), all(0xc2), all(0xc3), all(0xc4), all(0xc5), all(0xc6), all(0xc7), all(0xc8), all(0xc9), all(0xca), all(0xcb), all(0xcc), all(0xcd), all(0xce), all(0xcf),
        all(0xd0), all(0xd1), all(0xd2), all(0xd3), all(0xd4), all(0xd5), all(0xd6), all(0xd7), all(0xd8), all(0xd9), all(0xda), all(0xdb), all(0xdc), all(0xdd), all(0xde), all(0xdf),
        all(0xe0), all(0xe1), all(0xe2), all(0xe3), all(0xe4), all(0xe5), all(0xe6), all(0xe7), all(0xe8), all(0xe9), all(0xea), all(0xeb), all(0xec), all(0xed), all(0xee), all(0xef),
        all(0xf0), all(0xf1), all(0xf2), all(0xf3), all(0xf4), all(0xf5), all(0xf6), all(0xf7), all(0xf8), all(0xf9), all(0xfa), all(0xfb), all(0xfc), all(0xfd), all(0xfe), all(0xff),
    }
    {}

    constexpr const _t& operator[] (u8_t i) const { return v[i]; }
    constexpr const _t& operator[] (Pi pi) const { return v[pi]; }

};

extern const VectorOfAll vectorOfAll;

#endif
