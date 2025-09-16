#ifndef BIT_ARRAY_HPP
#define BIT_ARRAY_HPP

#include "bitops.hpp"

#define SELF static_cast<Self&>(*this)
#define CONST_SELF static_cast<const Self&>(*this)

template <typename _vector_type>
struct BitArrayOps {
    using _t = _vector_type;
    static constexpr bool equals(const _t& a, const _t& b) { return a == b; }
};

//typesafe BitArray implementation using "curiously recurring template pattern"
template <class self_type, typename vector_type>
class BitArray {
public:
    using _t = vector_type;

protected:
    using Self = self_type;
    using Arg = const Self&;
    using Ops = BitArrayOps<_t>;

    _t v;

public:
    constexpr BitArray () : v{} {}
    constexpr explicit BitArray (const _t& b) : v{b} {}
    constexpr Self& operator = (Arg b) { v = b.v; return SELF; }
    constexpr operator const _t& () const { return v; }

    constexpr friend bool operator == (Arg a, Arg b) { return Ops::equals(a.v, b.v); }
    constexpr Self& operator &= (Arg b) { v &= b.v; return SELF; }
    constexpr Self& operator |= (Arg b) { v |= b.v; return SELF; }
    constexpr Self& operator ^= (Arg b) { v ^= b.v; return SELF; }

    constexpr Self& operator %= (Arg b) { *this |= b; *this ^= b; return SELF; } // andnot_assign
    constexpr Self& operator += (Arg b) { assert (none(b)); return SELF ^= b; }
    constexpr Self& operator -= (Arg b) { assert (CONST_SELF >= b); return SELF ^= b; }

    constexpr friend bool operator <  (Arg a, Arg b) { return !(a >= b); }
    constexpr friend bool operator >  (Arg a, Arg b) { return !(a <= b); }
    constexpr friend bool operator <= (Arg a, Arg b) { return (a & b) == a; }
    constexpr friend bool operator >= (Arg a, Arg b) { return b <= a; }
    constexpr friend Self operator &  (Arg a, Arg b) { return Self{a} &= b; }
    constexpr friend Self operator |  (Arg a, Arg b) { return Self{a} |= b; }
    constexpr friend Self operator ^  (Arg a, Arg b) { return Self{a} ^= b; }
    constexpr friend Self operator +  (Arg a, Arg b) { return Self{a} += b; }
    constexpr friend Self operator -  (Arg a, Arg b) { return Self{a} -= b; }
    constexpr friend Self operator %  (Arg a, Arg b) { return Self{a} %= b; }
    constexpr bool none() const { return CONST_SELF == Self{}; }
    constexpr bool any() const { return !none(); }
    constexpr bool none(Arg b) const { return (CONST_SELF & b).none(); }
};

#undef SELF
#undef CONST_SELF

#endif
