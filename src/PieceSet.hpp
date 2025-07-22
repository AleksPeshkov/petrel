#ifndef PIECE_SET_HPP
#define PIECE_SET_HPP

#include "io.hpp"
#include "typedefs.hpp"
#include "BitSet.hpp"

/**
 * class used for enumeration of piece vectors
 */
class PieceSet : public BitSet<PieceSet, Pi> {
public:
    using BitSet::BitSet;

    Pi seekVacant() const {
        _t x = v;
        x = ~x & (x+1); //TRICK: isolate the lowest unset bit
        return PieceSet{x}.index();
    }

    index_t popcount() const {
        return ::popcount(v);
    }

    friend io::ostream& operator << (io::ostream& out, PieceSet v) {
        FOR_EACH(Pi, pi) {
            if (v.has(pi)) { out << pi; } else { out << '.'; }
        }
        return out;
    }

};

#endif
