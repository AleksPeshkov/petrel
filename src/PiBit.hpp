#ifndef PI_BIT_HPP
#define PI_BIT_HPP

#include "BitArray128.hpp"
#include "VectorOfAll.hpp"
#include "PiMask.hpp"

template <class Self, class Index>
class PiBit : public BitArray<Self, i128_t> {
    typedef BitArray<Self, i128_t> Base;
public:
    typedef typename Base::_t _t;
    typedef u8_t element_type;
protected:
    typedef typename Index::_t index_type;

    constexpr static element_type single(index_type _Bit) { return ::singleton<element_type>(_Bit); }
    constexpr static element_type exceptSingle(index_type _Bit) { return static_cast<element_type>(0xff ^ single(_Bit)); }

    static _t singleVector(index_type _Bit) { return ::vectorOfAll[single(_Bit)]; }
    static _t exceptSingleVector(index_type _Bit) { return ::vectorOfAll[exceptSingle(_Bit)]; }

    element_type& operator [] (Pi pi) { return reinterpret_cast<element_type*>(this)[pi]; }

public:
    using Base::Base;

    constexpr bool isEmpty(Pi pi) const { return get(pi) == 0; }
    void clear() { *this = Self(); }
    void clear(Pi pi) { (*this)[pi] = 0; }

    constexpr const element_type& get(Pi pi) const { return reinterpret_cast<const element_type*>(this)[pi]; }

    constexpr bool is(Pi pi, element_type bitmask) const { return (get(pi) & bitmask) != 0; }
    constexpr bool is(Pi pi, index_type _Bit) const { return is(pi, single(_Bit)); }

    PiMask anyOf(index_type _Bit) const {
        _t mask = singleVector(_Bit);
        return PiMask::equals(mask, this->v & mask);
    }

    PiMask anyOf(element_type bitmask) const {
        _t mask = ::vectorOfAll[bitmask];
        return PiMask::negate(PiMask::equals(this->v & mask, _t{0,0}));
    }

    PiMask notEmpty() const {
        return PiMask::negate(PiMask::equals(this->v, _t{0,0}));
    }

    void clear(index_type _Bit) {
        this->v &= exceptSingleVector(_Bit);
    }

    void clear(Pi pi, index_type _Bit) {
        element_type mask{exceptSingle(_Bit)};
        (*this)[pi] &= mask;
    }

    void set(Pi pi, index_type _Bit) {
        element_type mask{single(_Bit)};
        (*this)[pi] |= mask;
    }

};

#endif
