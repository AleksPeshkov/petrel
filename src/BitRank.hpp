#ifndef BIT_RANK_HPP
#define BIT_RANK_HPP

#include "typedefs.hpp"
#include "BitSet.hpp"

/**
 * a bit for each chessboard square of a rank
 */
class BitRank : public BitSet<BitRank, File, u8_t> {
public:
    using BitSet::BitSet;
};

#endif
