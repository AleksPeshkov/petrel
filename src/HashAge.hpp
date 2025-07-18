#ifndef HASH_AGE_HPP
#define HASH_AGE_HPP

class HashAge {
public:
    typedef unsigned _t;
    enum {AgeBits = 3, AgeMask = (1u << AgeBits)-1};

private:
    _t v;

public:
    constexpr HashAge () : v(1) {}
    constexpr operator const _t& () { return v; }

    void nextAge() {
        //there are "AgeMask" ages, not "1 << AgeBits", because of:
        //1) we want to break 4*n ply transposition pattern
        //2) make sure that initally clear entry is never hidden
        auto a = (v + 1) & AgeMask;
        v = a ? a : 1;
    }

};

#endif
