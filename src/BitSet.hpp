#ifndef BIT_SET_HPP
#define BIT_SET_HPP

#include "bitops.hpp"
#include "BitArray.hpp"

#define SELF static_cast<Self&>(*this)
#define CONST_SELF static_cast<const Self&>(*this)

template <class Self, class Index, class Value = index_t>
class BitSet : public BitArray<Self, Value> {
public:
    typedef BitArray<Self, Value> Base;
    using typename Base::_t;
    using Base::v;
    using index_type = typename Index::_t;

    constexpr BitSet () : Base{} {}
    constexpr explicit BitSet (_t b) : Base{b} {}

    constexpr BitSet (Index i) : BitSet{::singleton<_t>(i)} {}
    constexpr BitSet (index_type i) : BitSet{::singleton<_t>(i)} {}

    // clear the first (lowest) set bit
    constexpr _t clearFirst() const { return ::clearFirst(v); }

    // check if the index bit is set
    constexpr bool has(Index i) const {
        return (CONST_SELF & Self{i}).any();
    }

    // one and only one bit set
    bool isSingleton() const {
        assert(CONST_SELF.any());
        return clearFirst() == 0;
    }

    // get the first (lowest) bit set
    constexpr Index first() const {
        return static_cast<index_type>(::bsf(v));
    }

    // get the last (highest) bit set
    constexpr Index last() const {
        return static_cast<index_type>(::bsr(v));
    }

    // get the singleton bit set
    Index index() const {
        assert(CONST_SELF.any() && isSingleton());
        return operator* ();
    }

    //support for range-based for loop
    constexpr Index operator*() const { return CONST_SELF.first(); }
    Self& operator++() { *this = Self{clearFirst()}; return SELF; }
    constexpr Self begin() const { return CONST_SELF; }
    constexpr Self end() const { return Self{}; }
};

#undef SELF
#undef CONST_SELF

#endif
