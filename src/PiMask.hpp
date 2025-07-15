#ifndef PI_MASK_HPP
#define PI_MASK_HPP

#include "io.hpp"
#include "typedefs.hpp"

#include "PieceSet.hpp"
#include "VectorOfAll.hpp"
#include "PiSingle.hpp"

///piece vector of boolean values: false (0) or true (0xff)
class PiMask : public BitArray<PiMask, u8x16_t> {
public:
    typedef BitArray<PiMask, u8x16_t> Base;
    using typename Base::_t;
    using Base::v;

    constexpr PiMask () : Base{{{0,0}}} {}
    constexpr PiMask (Pi pi) : Base( ::piSingle[pi] ) {}

    explicit PiMask (_t a) : Base{a} { assertOk(); }
    explicit PiMask (i128_t a) { v.i128 = a; assertOk(); }
    constexpr operator const _t& () const { return v; }
    constexpr operator const i128_t& () const { return v; }

    // check if either 0 or 0xff bytes are set
    bool isOk() const { return ::equals(v, _mm_cmpgt_epi8(i128_t{0,0}, v)); }

    // assert if either 0 or 0xff bytes are set
    void assertOk() const { assert (isOk()); }

    static PiMask all() { return PiMask{ ::vectorOfAll[0xffu] }; }

    static PiMask equals(i128_t a, i128_t b) { return PiMask { _mm_cmpeq_epi8(a, b) }; }
    static PiMask equals(PiMask a, PiMask b) { return equals(static_cast<i128_t>(a), static_cast<i128_t>(b)); }
    PiMask operator ~ () { return equals(*this, PiMask{}); }

    static PiMask cmpgt(_t a, _t b) { return PiMask { _mm_cmpgt_epi8(a, b) }; }

    operator PieceSet() const {
        assertOk();
        return PieceSet( static_cast<PieceSet::_t>(_mm_movemask_epi8(v)) );
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

    index_t count() const { return PieceSet(*this).count(); }

    PieceSet begin() const { return *this; }
    PieceSet end() const { return {}; }

    friend io::ostream& operator << (io::ostream& out, PiMask a) {
        return out << PieceSet(a);
    }
};

#endif
