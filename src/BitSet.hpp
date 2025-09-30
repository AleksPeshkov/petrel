#ifndef BIT_SET_HPP
#define BIT_SET_HPP

#include "bitops.hpp"
#include "BitArray.hpp"

#define SELF static_cast<Self&>(*this)
#define CONST_SELF static_cast<const Self&>(*this)

template <class Self, typename _index_type, class Value = unsigned>
class BitSet : public BitArray<Self, Value> {
public:
    using Base = BitArray<Self, Value>;
    using typename Base::_t;
    using Base::v;
    using Index = _index_type;

    constexpr explicit BitSet (_t b = 0) : Base{b} {}
    constexpr explicit BitSet (Index i) : BitSet{::singleton<_t>(i)} {}

    // clear the first (lowest) set bit
    constexpr _t clearFirst() const { return ::clearFirst(v); }

    // check if the index bit is set
    constexpr bool has(Index::_t i) const {
        return (CONST_SELF & Self{Index{i}}).any();
    }

    // one and only one bit set
    constexpr bool isSingleton() const {
        assert (CONST_SELF.any());
        return clearFirst() == 0;
    }

    // get the first (lowest) bit set
    constexpr Index first() const {
        return Index{static_cast<Index::_t>(::lsb(v))};
    }

    // get the last (highest) bit set
    constexpr Index last() const {
        return  Index{static_cast<Index::_t>(::msb(v))};
    }

    // get the singleton bit set
    constexpr Index index() const {
        assert (CONST_SELF.any() && isSingleton());
        return operator* ();
    }

    constexpr int popcount() const {
        return ::popcount(v);
    }

    //support for range-based for loop
    constexpr Index operator*() const { return CONST_SELF.first(); }
    constexpr Self& operator++() { *this = Self{clearFirst()}; return SELF; }
    constexpr auto begin() const { return CONST_SELF; }
    constexpr auto end() const { return Self{}; }
};

#undef SELF
#undef CONST_SELF

#endif
