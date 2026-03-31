#ifndef BIT_ARRAY_HPP
#define BIT_ARRAY_HPP

#include "bitops.hpp"

template <typename self_type>
struct BitArrayOps {
    using Arg = self_type;
    static constexpr bool equals(Arg a, Arg b) { return a == b; }
};

//typesafe BitArray implementation using "curiously recurring template pattern"
template <class self_type, typename value_type = unsigned>
class BitArray {
public:
    using _t = value_type; // _t v_

private:
    using T = self_type;
    using Arg = T;
    using Ops = BitArrayOps<_t>;

protected:
    _t v_; // _t v_
    constexpr BitArray () : v_{} {}
    constexpr explicit BitArray (_t v) : v_{v} {}

public:
    constexpr _t v() const { return v_; } // _t v() const { return v_; }

    constexpr void clear() { *this = T{}; }
    constexpr T& operator &= (Arg b) { v_ &= b.v(); return static_cast<T&>(*this); }
    constexpr T& operator |= (Arg b) { v_ |= b.v(); return static_cast<T&>(*this); }
    constexpr T& operator ^= (Arg b) { v_ ^= b.v(); return static_cast<T&>(*this); }
    constexpr T& operator %= (Arg b) { v_ |= b.v(); v_ ^= b.v(); return static_cast<T&>(*this); } // andnot_assign
    constexpr T& operator += (Arg b) { assert (none(b)); return static_cast<T&>(*this) ^= b; }
    constexpr T& operator -= (Arg b) { assert (static_cast<const T&>(*this) >= b); return static_cast<T&>(*this) ^= b; }

    friend constexpr T operator &  (Arg a, Arg b) { return T{a} &= b; }
    friend constexpr T operator |  (Arg a, Arg b) { return T{a} |= b; }
    friend constexpr T operator ^  (Arg a, Arg b) { return T{a} ^= b; }
    friend constexpr T operator +  (Arg a, Arg b) { return T{a} += b; }
    friend constexpr T operator -  (Arg a, Arg b) { return T{a} -= b; }
    friend constexpr T operator %  (Arg a, Arg b) { return T{a} %= b; }

    friend constexpr bool operator == (Arg a, Arg b) { return Ops::equals(a.v(), b.v()); }
    friend constexpr bool operator <  (Arg a, Arg b) { return !((a & b) == b); }

    constexpr bool none() const { return static_cast<const T&>(*this) == T{}; }
    constexpr bool none(Arg b) const { return (static_cast<const T&>(*this) & b).none(); }
    constexpr bool any() const { return !none(); }
    constexpr bool any(Arg b) const { return !none(b); }
};

#endif
