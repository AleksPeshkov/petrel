#ifndef BIT_SET_HPP
#define BIT_SET_HPP

#include "bitops.hpp"
#include "BitArray.hpp"

template <class Self, class Index, class Value = index_t>
class BitSet : public BitArray<Self, Value> {
    typedef BitArray<Self, Value> Base;

protected:
    using Base::self;

public:
    using typename Base::_t;

    constexpr BitSet () : Base{} {}
    constexpr explicit BitSet (_t b) : Base{b} {}

    constexpr BitSet (Index i) : BitSet{::singleton<_t>(i)} {}
    constexpr BitSet (typename Index::_t i) : BitSet{::singleton<_t>(i)} {}

    // clear the first (lowest) set bit
    constexpr _t clearFirst() const { return ::clearFirst(this->v); }

    // check if the index bit is set
    constexpr bool has(Index i) const { return (self() & Self{i}).any(); }

    // one and only one bit sets
    bool isSingleton() const { assert (self().any()); return clearFirst() == 0; }

    // get the first (lowest) bit set
    constexpr Index first() const { return static_cast<typename Index::_t>(::bsf(this->v)); }

    // get the last (highest) bit set
    constexpr Index last()  const { return static_cast<typename Index::_t>(::bsr(this->v)); }

    // get the singleton bit set
    Index index() const { assert (self().any() && self().isSingleton()); return *self(); }

    // get the lowest bit set
    constexpr Index operator * () const { return self().first(); }

    // unset the lowest bit set
    Self& operator ++ () { *this = Self(clearFirst()); return self(); }

    constexpr Self begin() const { return self(); }
    constexpr Self end() const { return Self{}; }

};

#endif
