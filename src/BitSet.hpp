#ifndef BIT_SET_HPP
#define BIT_SET_HPP

#include "bitops.hpp"
#include "BitArray.hpp"

#define SELF static_cast<self_type&>(*this)
#define CONST_SELF static_cast<const self_type&>(*this)

template <class self_type, class index_type, typename value_type = unsigned>
class BitSet : public BitArray<self_type, value_type> {
    using Base = BitArray<self_type, value_type>;
    friend class BitArray<self_type, value_type>;

public:
    using typename Base::_t;
    using Index = index_type;

protected:
    using Base::v_;
    constexpr BitSet () : Base{} {}
    constexpr explicit BitSet (_t v) : Base{v} {}
    constexpr explicit BitSet (Index i) : BitSet{::singleton<_t>(i.v())} {}

public:
    // clear the first (lowest) set bit
    constexpr _t clearFirst() const { return ::clearFirst(v_); }

    // check if the index bit is set
    constexpr bool has(Index i) const {
        return CONST_SELF.any(self_type{Index{i}});
    }

    // one and only one bit set
    constexpr bool isSingleton() const {
        assert (CONST_SELF.any());
        return clearFirst() == 0;
    }

    // get the first (lowest) bit set
    constexpr Index first() const {
        return Index{static_cast<Index::_t>(::lsb(v_))};
    }

    // get the last (highest) bit set
    constexpr Index last() const {
        return  Index{static_cast<Index::_t>(::msb(v_))};
    }

    // get the singleton bit set
    constexpr Index index() const {
        assert (isSingleton());
        return operator* ();
    }

    constexpr int popcount() const {
        return ::popcount(v_);
    }

    //support for range-based for loop
    constexpr Index operator*() const { return CONST_SELF.first(); }
    constexpr self_type& operator++() { *this = self_type{clearFirst()}; return SELF; }
    constexpr auto begin() const { return CONST_SELF; }
    constexpr auto end() const { return self_type{}; }
};

#undef SELF
#undef CONST_SELF

#endif
