#ifndef PI_MASK_HPP
#define PI_MASK_HPP

#include "io.hpp"
#include "typedefs.hpp"
#include "BitSet.hpp"
#include "BitArray128.hpp"
#include "VectorOfAll.hpp"

/**
 * class used for enumeration of piece vectors
 */
class PieceSet : public BitSet<PieceSet, Pi> {
    using Base = BitSet<PieceSet, Pi>;
public:
    using Base::Base;

    constexpr Pi vacantMostValuable() const {
        _t x = v;
        x = ~x & (x+1); //TRICK: isolate the lowest unset bit
        return PieceSet{x}.index();
    }

    constexpr int popcount() const {
        return ::popcount(v);
    }

    friend ostream& operator << (ostream& out, PieceSet s) {
        for (auto pi : Pi::range()) {
            if (s.has(pi)) { out << pi; } else { out << '.'; }
        }
        return out;
    }

};

class PiSingle {
    using _t = vu8x16_t;

    Pi::arrayOf<_t> v;

public:
    constexpr PiSingle () : v {{
        {0xff,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
        {0,0xff,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
        {0,0,0xff,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
        {0,0,0,0xff, 0,0,0,0, 0,0,0,0, 0,0,0,0},

        {0,0,0,0, 0xff,0,0,0, 0,0,0,0, 0,0,0,0},
        {0,0,0,0, 0,0xff,0,0, 0,0,0,0, 0,0,0,0},
        {0,0,0,0, 0,0,0xff,0, 0,0,0,0, 0,0,0,0},
        {0,0,0,0, 0,0,0,0xff, 0,0,0,0, 0,0,0,0},

        {0,0,0,0, 0,0,0,0, 0xff,0,0,0, 0,0,0,0},
        {0,0,0,0, 0,0,0,0, 0,0xff,0,0, 0,0,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0xff,0, 0,0,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0xff, 0,0,0,0},

        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0xff,0,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0xff,0,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0xff,0},
        {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0xff},
    }}
    {}

    constexpr const _t& operator[] (Pi pi) const { return v[pi]; }
};

extern const PiSingle piSingle;

///piece vector of boolean values: false (0) or true (0xff)
class PiMask : public BitArray<PiMask, vu8x16_t> {
public:
    using Base = BitArray<PiMask, vu8x16_t>;
    using typename Base::_t;
    using Base::v;

    constexpr static _t zero() { return ::all(0); }

    constexpr PiMask () : Base{zero()} {}
    constexpr PiMask (Pi pi) : Base( ::piSingle[pi] ) {}

    constexpr explicit PiMask (_t a) : Base{a} { assertOk(); }
    constexpr PiMask (_t a, _t b) : Base{a == b} {}
    constexpr operator const _t& () const { return v; }

    // check if either 0 or 0xff bytes are set
    bool isOk() const { return ::equals(v, v != zero()); }

    // assert if either 0 or 0xff bytes are set
    constexpr void assertOk() const { assert (isOk()); }

    using Base::any;
    // create a mask of non empty bytes
    static PiMask any(_t a) { return PiMask{a != zero()}; }
    static PiMask all() { return PiMask{::all(0xff)}; }

    explicit operator PieceSet() const {
        assertOk();
        return PieceSet{static_cast<PieceSet::_t>(::mask(v))};
    }

    bool has(Pi pi) const { return PieceSet{*this}.has(pi); }
    bool none() const { return PieceSet{*this}.none(); }
    bool isSingleton() const { return PieceSet{*this}.isSingleton(); }

    Pi index() const { return PieceSet{*this}.index(); }

    // most valuable piece in the first (lowest) set bit
    Pi mostValuable() const { return PieceSet{*this}.first(); }

    // least valuable pieces in the last (highest) set bit
    Pi leastValuable() const { return PieceSet{*this}.last(); }

    int popcount() const { return PieceSet{*this}.popcount(); }

    PieceSet begin() const { return PieceSet{*this}; }
    PieceSet end() const { return PieceSet{ PiMask{zero()} }; }

    friend ostream& operator << (ostream& out, PiMask a) {
        return out << PieceSet(a);
    }
};

class PiOrder {
    vu8x16_t v;

    constexpr static Pi::arrayOf<vu8x16_t> forwardMask = {{
        {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
        {1,0,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
        {2,0,1,3,4,5,6,7,8,9,10,11,12,13,14,15},
        {3,0,1,2,4,5,6,7,8,9,10,11,12,13,14,15},
        {4,0,1,2,3,5,6,7,8,9,10,11,12,13,14,15},
        {5,0,1,2,3,4,6,7,8,9,10,11,12,13,14,15},
        {6,0,1,2,3,4,5,7,8,9,10,11,12,13,14,15},
        {7,0,1,2,3,4,5,6,8,9,10,11,12,13,14,15},
        {8,0,1,2,3,4,5,6,7,9,10,11,12,13,14,15},
        {9,0,1,2,3,4,5,6,7,8,10,11,12,13,14,15},
        {10,0,1,2,3,4,5,6,7,8,9,11,12,13,14,15},
        {11,0,1,2,3,4,5,6,7,8,9,10,12,13,14,15},
        {12,0,1,2,3,4,5,6,7,8,9,10,11,13,14,15},
        {13,0,1,2,3,4,5,6,7,8,9,10,11,12,14,15},
        {14,0,1,2,3,4,5,6,7,8,9,10,11,12,13,15},
        {15,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14},
    }};

    constexpr static vu8x16_t ordered = forwardMask[0];

    constexpr bool isOk() const {
        // check all values [0, 15] are represented
        u32_t mask = 0;
        for (int i = 0; i < 16; ++i) {
            mask |= ::singleton<int>(::u8(v, i));
        }
        return ::popcount(mask) == 16;
    }

    constexpr void assertOk() const { assert (isOk()); }

public:
    constexpr PiOrder () : v{ordered} { assertOk(); }

    constexpr PiMask operator () (PiMask pieces) const {
        return PiMask{::shufflevector(static_cast<vu8x16_t>(pieces), v)};
    }

    constexpr Pi operator[] (Pi pi) const {
        return Pi{static_cast<Pi::_t>(::u8(v, pi))};
    }

    PiOrder& forward(Pi pi) {
        // find index of pi in the shuffled vector
        PiMask piMask{v, ::vectorOfAll[pi]};
        Pi pos = piMask.index();

        v = ::shufflevector(v, forwardMask[pos]);
        assertOk();
        return *this;
    }
};

class PiOrdered {
    PiOrder order; // vector of piece indices
    PieceSet mask; // shuffled PiMask bitset

    constexpr explicit operator const PieceSet& () const { return mask; }

public:
    PiOrdered (PiMask pieces, PiOrder o)
        : order{o}, mask{order(pieces)}
    {}

    constexpr friend bool operator == (const PiOrdered& a, const PieceSet& b) { return a.mask == b; }

    constexpr Pi operator * () const { return order[*mask]; }
    constexpr PiOrdered& operator ++ () { ++mask; return *this; }
    PiOrdered begin() const { return *this; }
    constexpr PieceSet end() const { return {}; }
};

#endif
