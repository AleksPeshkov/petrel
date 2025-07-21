#ifndef TT_BUCKET_HPP
#define TT_BUCKET_HPP

#include "bitops128.hpp"
#include "typedefs.hpp"

class CACHE_ALIGN TtBucket {
public:
    union {
        Index<4>::arrayOf<i128_t> i128;
        Index<8>::arrayOf<u64_t>   u64;
        Index<64>::arrayOf<u8_t>    u8;
    };
};

#endif
