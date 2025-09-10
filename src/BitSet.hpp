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
    using index_type = _index_type;

    constexpr BitSet () : Base{} {}
    constexpr explicit BitSet (_t b) : Base{b} {}

    constexpr BitSet (index_type i) : BitSet{::singleton<_t>(i)} {}

    // clear the first (lowest) set bit
    constexpr _t clearFirst() const { return ::clearFirst(v); }

    // check if the index bit is set
    constexpr bool has(index_type i) const {
        return (CONST_SELF & Self{i}).any();
    }

    // one and only one bit set
    bool isSingleton() const {
        assert (CONST_SELF.any());
        return clearFirst() == 0;
    }

    // get the first (lowest) bit set
    constexpr index_type first() const {
        return static_cast<index_type>(::lsb(v));
    }

    // get the last (highest) bit set
    constexpr index_type last() const {
        return static_cast<index_type>(::msb(v));
    }

    // get the singleton bit set
    index_type index() const {
        assert (CONST_SELF.any() && isSingleton());
        return operator* ();
    }

    //support for range-based for loop
    constexpr index_type operator*() const { return CONST_SELF.first(); }
    Self& operator++() { *this = Self{clearFirst()}; return SELF; }
    constexpr Self begin() const { return CONST_SELF; }
    constexpr Self end() const { return Self{}; }
};

#undef SELF
#undef CONST_SELF

#endif
