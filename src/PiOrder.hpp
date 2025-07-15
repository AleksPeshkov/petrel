#ifndef PI_ORDER_HPP
#define PI_ORDER_HPP

#include "typedefs.hpp"
#include "PiMask.hpp"

class PiOrder {
    typedef u8x16_t _t;

    _t v;

    bool isOk() const {
        return ::equals(_mm_shuffle_epi8(v, v), PiOrder{});
    }

    operator i128_t () const { return v; }

public:
    PiOrder () : v{{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15}} {}
    PiOrder (_t o) : v(o) { assert (isOk()); }

    PiMask order(PiMask a) const {
        return PiMask{_mm_shuffle_epi8(static_cast<PiMask::_t>(a), v)};
    }
    Pi operator[] (Pi pi) const { return static_cast<Pi::_t>(v.u8[pi]); }

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
