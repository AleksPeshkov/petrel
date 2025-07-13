#ifndef PI_ORDER_HPP
#define PI_ORDER_HPP

#include "typedefs.hpp"
#include "PiMask.hpp"
#include "PiVector.hpp"

class PiOrder {
    typedef i128_t _t;

    struct Sorted {
        union {
            _t v;
            char a[16];
        };
        constexpr Sorted () : a{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15} {}
        constexpr operator _t () const { return v; }
    };
    static const Sorted sorted;

    PiVector v;

    bool isOk() const {
        _t a = _mm_shuffle_epi8(static_cast<_t>(v), static_cast<_t>(v));
        return _mm_movemask_epi8(_mm_cmpeq_epi8(a, sorted)) == 0xffffu;
    }

public:
    PiOrder () : v(sorted) {}
    PiOrder (_t o) : v(o) { assert (isOk()); }

    PiMask order(PiMask a) const {
        return PiMask{_mm_shuffle_epi8(static_cast<PiMask::_t>(a), v)};
    }
    Pi operator[] (Pi pi) const { return static_cast<Pi::_t>(v.get(pi)); }

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
