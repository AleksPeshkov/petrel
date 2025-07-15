#ifndef SQUARES_IN_BETWEEN_HPP
#define SQUARES_IN_BETWEEN_HPP

#include "Bb.hpp"
#include "typedefs.hpp"

// line (file, rank, diagonal) in between two squares (excluding both ends) or 0 (32k)
class SquaresInBetween {
    Square::arrayOf< Square::arrayOf<Bb> > inBetween;

public:
    constexpr SquaresInBetween () {
        FOR_EACH(Square, from) {
            FOR_EACH(Square, to) {
                Bb belowFrom{ ::singleton<Bb::_t>(from) - 1 };
                Bb belowTo{ ::singleton<Bb::_t>(to) - 1 };
                Bb areaInBetween = (belowFrom ^ belowTo) % to;

                Bb result = Bb{};
                FOR_EACH(Direction, dir) {
                    Bb line = from.line(dir); // line bitboard for this direction
                    if (line.has(to)) {       // Check if 'to' is on this line
                        result = areaInBetween & line;
                        break; // Only one valid direction per (from, to)
                    }
                }

                inBetween[from][to] = result;
            }
        }
    }

    constexpr const Bb& operator() (Square from, Square to) const { return inBetween[from][to]; }

};

// line (file, rank, diagonal) in between two squares (excluding both ends) or 0
extern const SquaresInBetween inBetween;

#endif
