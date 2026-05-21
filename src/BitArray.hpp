#ifndef BIT_ARRAY_HPP
#define BIT_ARRAY_HPP

#include "bitops.hpp"

template <typename self_type>
struct BitArrayOps {
    using Arg = self_type;
    static constexpr bool equals(Arg a, Arg b) { return a == b; }
};

// typesafe BitArray implementation using "curiously recurring template pattern"
template <class self_type, typename value_type = unsigned>
class BitArray {
public:
    using _t = value_type; // _t v_

private:
    using Self = self_type;
    using Arg = Self;
    using Ops = BitArrayOps<_t>;

    constexpr Self& self() { return static_cast<Self&>(*this); }
    constexpr const Self& cself() const { return static_cast<const Self&>(*this); }

protected:
    _t v_; // _t v_
    constexpr BitArray () : v_{} {}
    constexpr explicit BitArray (_t v) : v_{v} {}

public:
    constexpr _t v() const { return v_; } // _t v() const { return v_; }

    constexpr Self& operator &= (Arg b) { v_ &= b.v_; return self(); }
    constexpr Self& operator |= (Arg b) { v_ |= b.v_; return self(); }
    constexpr Self& operator ^= (Arg b) { v_ ^= b.v_; return self(); }
    constexpr Self& operator %= (Arg b) { v_ |= b.v_; v_ ^= b.v_; return self(); } // andnot_assign
    constexpr Self& operator += (Arg b) { assert (none(b)); return self() ^= b; }
    constexpr Self& operator -= (Arg b) { assert (cself() >= b); return self() ^= b; }

    friend constexpr Self operator & (Arg a, Arg b) { return Self{a} &= b; }
    friend constexpr Self operator | (Arg a, Arg b) { return Self{a} |= b; }
    friend constexpr Self operator ^ (Arg a, Arg b) { return Self{a} ^= b; }
    friend constexpr Self operator + (Arg a, Arg b) { return Self{a} += b; }
    friend constexpr Self operator - (Arg a, Arg b) { return Self{a} -= b; }
    friend constexpr Self operator % (Arg a, Arg b) { return Self{a} %= b; }

    friend constexpr bool operator == (Arg a, Arg b) { return Ops::equals(a.v_, b.v_); }
    friend constexpr bool operator <  (Arg a, Arg b) { return !((a & b) == b); }

    constexpr bool none() const { return cself() == Self{}; }
    constexpr bool none(Arg b) const { return (cself() & b).none(); }
    constexpr bool any() const { return !none(); }
    constexpr bool any(Arg b) const { return !none(b); }
};

template <class self_type, class index_type, typename value_type = unsigned>
class BitSet : public BitArray<self_type, value_type> {
    using Base = BitArray<self_type, value_type>;
    friend class BitArray<self_type, value_type>;
    using Self = self_type;

public:
    using typename Base::_t;
    using Index = index_type;

protected:
    using Base::v_;
    using Base::Base;

public:
    // clear the first (lowest) set bit
    constexpr _t clearFirst() const { return ::clearFirst(v_); }

    // check if the index bit is set
    constexpr bool has(Index i) const { return static_cast<const Self&>(*this).any(Self{Index{i}}); }

    // one and only one bit set
    constexpr bool isSingleton() const { assert (static_cast<const Self&>(*this).any()); return clearFirst() == 0; }

    // get the first (lowest) bit set
    constexpr Index first() const { return Index{static_cast<Index::_t>(::lsb(v_))}; }

    // get the last (highest) bit set
    constexpr Index last() const { return  Index{static_cast<Index::_t>(::msb(v_))}; }

    // get the singleton bit set
    constexpr Index index() const { assert (isSingleton()); return operator* (); }

    constexpr int popcount() const { return ::popcount(v_); }

    //support for range-based for-loop
    constexpr Index operator*() const { return static_cast<const Self&>(*this).first(); }
    constexpr Self& operator++() { *this = Self{clearFirst()}; return static_cast<Self&>(*this); }
    constexpr const Self& begin() const { return static_cast<const Self&>(*this); }
    static constexpr Self end() { return Self{}; }
};

#endif
