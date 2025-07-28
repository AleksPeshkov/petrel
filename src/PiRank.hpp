#ifndef PI_RANK_HPP
#define PI_RANK_HPP

#include "typedefs.hpp"
#include "BitArray128.hpp"
#include "PiMask.hpp"
#include "VectorOfAll.hpp"

class PiRank : public BitArray<PiRank, vu8x16_t> {
    typedef BitArray<PiRank, vu8x16_t> Base;

public:
    using Base::Base;
    constexpr explicit PiRank () : Base{::all(0)} {}
    constexpr explicit PiRank (BitRank br) : Base{::vectorOfAll[br]} {}
    constexpr explicit PiRank (File f) : PiRank{BitRank{f}} {}
    constexpr PiRank (PiMask m) : Base{m} {}

    BitRank gather() const {
        u8_t r  = v[0] | v[1] | v[2] | v[3] | v[4] | v[5] | v[6] | v[7]
            | v[8] | v[9] | v[10] | v[11] | v[12] | v[13] | v[14] | v[15];
        return BitRank{r};
    }

    constexpr BitRank operator [] (Pi pi) const {
        return BitRank{ ::u8(v, pi) };
    }

    PiMask operator [] (File file) const {
        _t file_vector = PiRank{file};
        return PiMask{ (v & file_vector) == file_vector };
    }

    constexpr void clear() { v = ::all(0); }
};

#endif
