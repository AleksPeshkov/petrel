#ifndef BIT_ARRAY_HPP
#define BIT_ARRAY_HPP

#include "bitops.hpp"

template <typename _vector_type>
struct BitArrayOps {
    using _t = _vector_type;
    static constexpr bool equals(const _t& a, const _t& b) { return a == b; }
};

//typesafe BitArray implementation using "curiously recurring template pattern"
template <class self_type, typename value_type = unsigned>
class BitArray {
public:
    using _t = value_type; // _t v_

private:
    using Self = self_type;
#define SELF static_cast<Self&>(*this)
#define CONST_SELF static_cast<const Self&>(*this)
    using Arg = Self;

    using Ops = BitArrayOps<_t>;
protected:
    _t v_; // _t v_
    constexpr BitArray () : v_{} {}
    constexpr explicit BitArray (_t v) : v_{v} {}

public:
    constexpr _t v() const { return v_; } // _t v() const { return v_; }

    constexpr Self& operator &= (Arg b) { v_ &= b.v_; return SELF; }
    constexpr Self& operator |= (Arg b) { v_ |= b.v_; return SELF; }
    constexpr Self& operator ^= (Arg b) { v_ ^= b.v_; return SELF; }
    constexpr Self& operator %= (Arg b) { *this |= b; *this ^= b; return SELF; } // andnot_assign
    constexpr Self& operator += (Arg b) { assert (none(b)); return SELF ^= b; }
    constexpr Self& operator -= (Arg b) { assert (CONST_SELF >= b); return SELF ^= b; }

    constexpr friend Self operator &  (Arg a, Arg b) { return Self{a} &= b; }
    constexpr friend Self operator |  (Arg a, Arg b) { return Self{a} |= b; }
    constexpr friend Self operator ^  (Arg a, Arg b) { return Self{a} ^= b; }
    constexpr friend Self operator +  (Arg a, Arg b) { return Self{a} += b; }
    constexpr friend Self operator -  (Arg a, Arg b) { return Self{a} -= b; }
    constexpr friend Self operator %  (Arg a, Arg b) { return Self{a} %= b; }

    constexpr friend bool operator == (Arg a, Arg b) { return Ops::equals(a.v_, b.v_); }
    constexpr friend bool operator <= (Arg a, Arg b) { return (a & b) == a; }
    constexpr friend bool operator >= (Arg a, Arg b) { return b <= a; }
    constexpr friend bool operator != (Arg a, Arg b) { return !(a == b); }
    constexpr friend bool operator <  (Arg a, Arg b) { return !(a >= b); }
    constexpr friend bool operator >  (Arg a, Arg b) { return b < a; }

    constexpr bool none() const { return CONST_SELF == Self{}; }
    constexpr bool none(Arg b) const { return (CONST_SELF & b).none(); }
    constexpr bool any() const { return !none(); }
    constexpr bool any(Arg b) const { return !none(b); }

#undef SELF
#undef CONST_SELF
};

#endif
