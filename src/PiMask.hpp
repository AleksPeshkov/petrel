#ifndef PI_MASK_HPP
#define PI_MASK_HPP

#include "io.hpp"
#include "typedefs.hpp"

#include "PieceSet.hpp"
#include "VectorOfAll.hpp"
#include "PiSingle.hpp"

///piece vector of boolean values: false (0) or true (0xff)
class PiMask {
public:
    typedef i128_t _t;
private:
    _t v;

    typedef PiMask self_type;
    typedef self_type& self_ref;
    typedef const self_type& self_cref;

#ifdef NDEBUG
    void assertOk() const {}
#else
    void assertOk() const {
        auto n = _mm_cmpgt_epi8(_t{0,0}, v);
        assert (_mm_movemask_epi8(_mm_cmpeq_epi8(v, n)) == 0xffffu);
    }
#endif

    friend class PiOrder;
    explicit PiMask (_t a) : v{a} { assertOk(); }

public:
    constexpr operator _t () const { return v; }

    constexpr PiMask () : v{0,0} {}
    constexpr PiMask (Pi pi) : v( ::piSingle[pi] ) {}

    static PiMask all() { return PiMask{ ::vectorOfAll[0xff] }; }
    static PiMask equals(_t a, _t b) { return PiMask { _mm_cmpeq_epi8(a, b) }; }
    static PiMask cmpgt(_t a, _t b) { return PiMask { _mm_cmpgt_epi8(a, b) }; }
    static PiMask negate(_t a) { return PiMask::equals(a, _t{0,0}); }

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

    bool any() const { return !none(); }

    constexpr self_ref operator &= (self_cref a) { v &= a.v; return *this; }
    constexpr self_ref operator |= (self_cref a) { v |= a.v; return *this; }
    constexpr self_ref operator ^= (self_cref a) { v ^= a.v; return *this; }
    constexpr self_ref operator %= (self_cref a) { v |= a.v; v ^= a.v; return *this; }  //"and not"

    self_ref operator += (self_cref a) { assert ((*this & a).none()); v ^= a.v; return *this; }
    self_ref operator -= (self_cref a) { assert (*this >= a); v ^= a.v; return *this; }

    friend bool operator == (self_type a, self_type b) { return (a ^ b).none(); }
    friend bool operator != (self_type a, self_type b) { return !(a == b); }
    friend bool operator <= (self_type a, self_type b) { return (a & b) == a; }
    friend bool operator >= (self_type a, self_type b) { return b <= a; }

    constexpr friend self_type operator & (self_type a, self_type b) { return self_type{a} &= b; }
    constexpr friend self_type operator | (self_type a, self_type b) { return self_type{a} |= b; }
    constexpr friend self_type operator ^ (self_type a, self_type b) { return self_type{a} ^= b; }
    constexpr friend self_type operator % (self_type a, self_type b) { return self_type{a} %= b; }

    friend self_type operator + (self_type a, self_type b) { return self_type{a} += b; }
    friend self_type operator - (self_type a, self_type b) { return self_type{a} -= b; }

};

#endif
