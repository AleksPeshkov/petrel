#ifndef BIT_ARRAY_HPP
#define BIT_ARRAY_HPP

#include <cassert>

#define SELF static_cast<Self&>(*this)
#define CONST_SELF static_cast<const Self&>(*this)

template <typename _vector_type>
struct BitArrayOps {
    typedef _vector_type _t;
    static constexpr bool equals(const _t& a, const _t& b) { return a == b; }
    static constexpr void and_assign(_t& a, const _t& b) { a &= b; }
    static constexpr void or_assign(_t& a, const _t& b) { a |= b; }
    static constexpr void xor_assign(_t& a, const _t& b) { a ^= b; }
};

//typesafe BitArray implementation using "curiously recurring template pattern"
template <class _Self, typename _vector_type>
class BitArray {
public:
    typedef _vector_type _t;
    typedef _vector_type vector_type;

protected:
    typedef _Self Self;
    typedef const Self& Arg;
    typedef BitArrayOps<vector_type> Ops;

    vector_type v;

public:
    constexpr BitArray () : v{} {}
    constexpr explicit BitArray (const vector_type& b) : v{b} {}
    constexpr Self& operator = (Arg b) { v = b.v; return SELF; }
    constexpr operator const vector_type& () const { return v; }

    constexpr friend bool operator == (Arg a, Arg b) { return Ops::equals(a.v, b.v); }
    constexpr Self& operator &= (Arg b) { Ops::and_assign(v, b.v); return SELF; }
    constexpr Self& operator |= (Arg b) { Ops::or_assign(v, b.v); return SELF; }
    constexpr Self& operator ^= (Arg b) { Ops::xor_assign(v, b.v); return SELF; }

    constexpr Self& operator %= (Arg b) { Ops::or_assign(v, b.v); Ops::xor_assign(v, b.v); return SELF; } // andnot_assign
    constexpr Self& operator += (Arg b) { assert(none(b)); return SELF ^= b; }
    constexpr Self& operator -= (Arg b) { assert(CONST_SELF >= b); return SELF ^= b; }

    constexpr friend bool operator != (Arg a, Arg b) { return !(a == b); }
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
