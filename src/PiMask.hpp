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
class PieceSet : public BitSet<PieceSet, Pi::_t> {
    typedef BitSet<PieceSet, Pi::_t> Base;
public:
    using Base::Base;

    Pi vacantMostValuable() const {
        _t x = v;
        x = ~x & (x+1); //TRICK: isolate the lowest unset bit
        return PieceSet{x}.index();
    }

    index_t popcount() const {
        return ::popcount(v);
    }

    friend io::ostream& operator << (io::ostream& out, PieceSet s) {
        FOR_EACH(Pi, pi) {
            if (s.has(pi)) { out << pi; } else { out << '.'; }
        }
        return out;
    }

};

class PiSingle {
    typedef vu8x16_t _t;

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
    typedef BitArray<PiMask, vu8x16_t> Base;
    using typename Base::_t;
    using Base::v;

    constexpr static _t zero() { return ::all(0); }

    constexpr PiMask () : Base{zero()} {}
    constexpr PiMask (Pi pi) : Base( ::piSingle[pi] ) {}

    explicit PiMask (_t a) : Base{a} { assertOk(); }
    constexpr PiMask (_t a, _t b) : Base{a == b} {}
    constexpr operator const _t& () const { return v; }

    // check if either 0 or 0xff bytes are set
    bool isOk() const { return ::equals(v, __builtin_convertvector(v != zero(), vu8x16_t)); }

    // assert if either 0 or 0xff bytes are set
    void assertOk() const { assert (isOk()); }

    using Base::any;
    // create a mask of non empty bytes
    static PiMask any(_t a) { return PiMask{a != zero()}; }
    static PiMask all() { return PiMask{::all(0xff)}; }

    PiMask operator ~ () { return PiMask{*this, zero()}; }

    explicit operator PieceSet() const {
        assertOk();
        return PieceSet{::mask(v)};
    }

    bool has(Pi pi) const { return PieceSet{*this}.has(pi); }
    bool none() const { return PieceSet{*this}.none(); }
    bool isSingleton() const { return PieceSet{*this}.isSingleton(); }

    Pi index() const { return PieceSet{*this}.index(); }

    // most valuable piece in the first (lowest) set bit
    Pi mostValuable() const { return PieceSet{*this}.first(); }

    // least valuable pieces in the last (highest) set bit
    Pi leastValuable() const { return PieceSet{*this}.last(); }

    index_t popcount() const { return PieceSet{*this}.popcount(); }

    PieceSet begin() const { return PieceSet{*this}; }
    PieceSet end() const { return PieceSet{ PiMask{zero()} }; }

    friend io::ostream& operator << (io::ostream& out, PiMask a) {
        return out << PieceSet(a);
    }
};

class PiOrder {
    vu8x16_t v;

    bool isOk() const {
        return ::equals(::shufflevector(v, v), vu8x16_t{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15});
    }

public:
    constexpr PiOrder () : v{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15} {}
    PiOrder (const vu8x16_t& o) : v{o} { assert (isOk()); }

    PiMask order(PiMask a) const {
        return PiMask{::shufflevector(static_cast<vu8x16_t>(a), v)};
    }

    Pi operator[] (Pi pi) const {
        return static_cast<Pi::_t>(::u8(v, pi));
    }
};

class PiOrderedIterator {
    PiOrder order;
    PieceSet mask;

public:
    PiOrderedIterator (PiOrder o, PiMask m) : order(o), mask(o.order(m)) {}

    Pi operator * () const { return order[*mask]; }
    PiOrderedIterator& operator ++ () { ++mask; return *this; }
    PiOrderedIterator begin() const { return *this; }
    PiOrderedIterator end() const { return PiOrderedIterator(order, {}); }
};

#endif
