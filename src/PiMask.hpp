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

    Pi seekVacant() const {
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
    typedef u8x16_t _t;

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
class PiMask : public BitArray<PiMask, u8x16_t> {
public:
    typedef BitArray<PiMask, u8x16_t> Base;
    using typename Base::_t;
    using Base::v;

    constexpr PiMask () : Base{::vectorOfAll[0]} {}
    constexpr PiMask (Pi pi) : Base( ::piSingle[pi] ) {}

    explicit PiMask (_t a) : Base{a} { assertOk(); }
    explicit PiMask (i128_t a) { v.i128 = a; assertOk(); }
    explicit PiMask (vu8x16_t a) { v.vu8x16 = a; assertOk(); }
    constexpr operator const _t& () const { return v; }
    constexpr operator const i128_t& () const { return v; }

    // check if either 0 or 0xff bytes are set
    bool isOk() const { return ::equals(v.i128, _mm_cmpgt_epi8(i128_t{0,0}, v.i128)); }

    // assert if either 0 or 0xff bytes are set
    void assertOk() const { assert (isOk()); }

    using Base::any;
    // create a mask of non empty bytes
    static PiMask any(i128_t a) { return equals(equals(a, i128_t{0,0}), i128_t{0,0}); }
    static PiMask all() { return PiMask{ ::vectorOfAll[0xffu] }; }

    static PiMask equals(i128_t a, i128_t b) { return PiMask { _mm_cmpeq_epi8(a, b) }; }
    static PiMask equals(vu8x16_t a, vu8x16_t b) { return equals(static_cast<i128_t>(a), static_cast<i128_t>(b)); }
    PiMask operator ~ () { return equals(v.i128, i128_t{0,0}); }

    static PiMask cmpgt(vu8x16_t a, vu8x16_t b) { return PiMask { _mm_cmpgt_epi8(a, b) }; }
    static PiMask cmpgt(i128_t a, i128_t b) { return PiMask { _mm_cmpgt_epi8(a, b) }; }

    operator PieceSet() const {
        assertOk();
        return PieceSet( static_cast<PieceSet::_t>(_mm_movemask_epi8(v.i128)) );
    }

    bool has(Pi pi) const { return PieceSet(*this).has(pi); }
    bool none() const { return PieceSet(*this).none(); }
    bool isSingleton() const { return PieceSet(*this).isSingleton(); }

    Pi index() const { return PieceSet(*this).index(); }

    // most valuable piece in the first (lowest) set bit
    Pi most() const { return PieceSet(*this).first(); }

    // least valuable pieces in the last (highest) set bit
    Pi least() const { return PieceSet(*this).last(); }

    Pi seekVacant() const { return PieceSet(*this).seekVacant(); }

    index_t popcount() const { return PieceSet(*this).popcount(); }

    PieceSet begin() const { return *this; }
    PieceSet end() const { return {}; }

    friend io::ostream& operator << (io::ostream& out, PiMask a) {
        return out << PieceSet(a);
    }
};

class PiOrder {
    union {
        vu8x16_t vu8x16;
        au8x16_t index;
    };

    bool isOk() const {
        return ::equals(::shufflevector(vu8x16, vu8x16), vu8x16_t{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15});
    }

public:
    constexpr PiOrder () : vu8x16{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15} {}
    PiOrder (const vu8x16_t& o) : vu8x16{o} { assert (isOk()); }

    PiMask order(PiMask a) const {
        return PiMask{::shufflevector(static_cast<vu8x16_t>(a), vu8x16)};
    }
    Pi operator[] (Pi pi) const { return static_cast<Pi::_t>(index[pi]); }

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
