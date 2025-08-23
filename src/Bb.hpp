#ifndef BB_HPP
#define BB_HPP

#include "bitops.hpp"
#include "io.hpp"
#include "typedefs.hpp"
#include "BitSet.hpp"

/**
 * a bit for each chessboard square of a rank
 */
class BitRank : public BitSet<BitRank, File::_t, u8_t> {
public:
    using BitSet::BitSet;
};

/**
 * BitBoard type: a bit for each chessboard square
 */
class Bb : public BitSet<Bb, Square::_t, u64_t> {
    //declared to catch type cast bugs
    Bb (int) = delete;

public:
    constexpr Bb () : BitSet() {}
    constexpr explicit Bb (_t bb) : BitSet{bb} {}

    constexpr explicit Bb (Square sq) : BitSet(sq) {}
    constexpr explicit Bb (Square::_t sq) : BitSet(sq) {}

    constexpr explicit Bb (File::_t f) : Bb{ULL(0x0101010101010101) << f} {}
    constexpr explicit Bb (Rank::_t r) : Bb{ULL(0xff) << 8*r} {}

    constexpr Bb operator ~ () const { return Bb{::byteswap(v)}; }

    void move(Square from, Square to) { assert (from != to); *this -= Bb{from}; *this += Bb{to}; }

    constexpr BitRank operator[] (Rank r) const { return BitRank{small_cast<BitRank::_t>(v >> 8*r)}; }

    // bidirectional signed rank shift
    constexpr Bb shiftRank(signed r) { return Bb{ r >= 0 ? (v << 8*r) : (v >> -8*r) }; }

    constexpr friend Bb operator << (Bb bb, unsigned offset) { return Bb{static_cast<_t>(bb) << offset}; }
    constexpr friend Bb operator >> (Bb bb, unsigned offset) { return Bb{static_cast<_t>(bb) >> offset}; }

    friend ostream& operator << (ostream& out, Bb bb) {
        FOR_EACH(Rank, rank) {
            FOR_EACH(File, file) {
                if (bb.has(Square{file, rank})) {
                    out << file;
                }
                else {
                    out << '.';
                }
            }
            out << '\n';
        }
        return out;
    }
};

constexpr Bb Square::rank() const { return Bb{Rank{*this}} - Bb{*this}; }
constexpr Bb Square::file() const { return Bb{File{*this}} - Bb{*this}; }
constexpr Bb Square::diagonal() const {
    return Bb{ULL(0x0102'0408'1020'4080)}.shiftRank(static_cast<signed>(Rank{*this}) + File{*this} - 7) - Bb{*this};
}
constexpr Bb Square::antidiag() const {
    return Bb{ULL(0x8040'2010'0804'0201)}.shiftRank(static_cast<signed>(Rank{*this}) - File{*this}) - Bb{*this};
}

constexpr Bb Square::line(Direction dir) const {
    switch (dir) {
        case FileDir:     return file();
        case RankDir:     return rank();
        case DiagonalDir: return diagonal();
        case AntidiagDir: return antidiag();
        default: return {};
    }
}

// https://www.chessprogramming.org/0x88
constexpr Bb Square::operator() (signed df, signed dr) const {
    auto sq0x88 = v + (v & 070) + df + 16*dr;

    if (sq0x88 & 0x88) {
        return {}; //out of chess board
    }

    return Bb{ static_cast<square_t>((sq0x88 + (sq0x88 & 7)) >> 1) };
}

// line (file, rank, diagonal) in between two squares (excluding both ends) or 0 (32k)
class InBetween {
    Square::arrayOf< Square::arrayOf<Bb> > inBetween;

public:
    constexpr InBetween () {
        FOR_EACH(Square, from) {
            FOR_EACH(Square, to) {
                Bb belowFrom{ ::singleton<Bb::_t>(from) - 1 };
                Bb belowTo{ ::singleton<Bb::_t>(to) - 1 };
                Bb areaInBetween = (belowFrom ^ belowTo) % Bb{to};

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
extern const InBetween inBetween;

//attack bitboards of the piece types on the empty board (3k)
class AttacksFrom {
    PieceType::arrayOf< Square::arrayOf<Bb> > attack;
public:
    constexpr AttacksFrom () {
        FOR_EACH (Square, sq) {
            attack[Rook][sq]   = sq.file() + sq.rank();
            attack[Bishop][sq] = sq.diagonal() + sq.antidiag();
            attack[Queen][sq]  = attack[Rook][sq] + attack[Bishop][sq];

            attack[Pawn][sq] = sq(-1, Rank3 - Rank2) + sq(+1, Rank3 - Rank2);

            attack[Knight][sq] =
                sq(+2, +1) + sq(+2, -1) + sq(+1, +2) + sq(+1, -2) +
                sq(-2, -1) + sq(-2, +1) + sq(-1, -2) + sq(-1, +2);

            attack[King][sq] =
                sq(+1, +1) + sq(+1, 0) + sq(0, +1) + sq(+1, -1) +
                sq(-1, -1) + sq(-1, 0) + sq(0, -1) + sq(-1, +1);
        }
    }

    constexpr const Bb& operator() (PieceType ty, Square sq) const { return attack[ty][sq]; }
};

extern const AttacksFrom attacksFrom;

class CastlingRules {
    struct Rules {
        Bb unimpeded;
        Bb unattacked;
    };

    File::arrayOf< File::arrayOf<Rules> > castlingRules;

    static constexpr Bb exBetween(Square king, Square rook) { return ::inBetween(king, rook) + Bb{rook}; }

public:
    constexpr CastlingRules () {
        FOR_EACH(File, kingFile) {
            FOR_EACH(File, rookFile) {
                Square king{kingFile, Rank1};
                Square rook{rookFile, Rank1};

                if (king == rook) {
                    castlingRules[kingFile][rookFile].unimpeded  = Bb{};
                    castlingRules[kingFile][rookFile].unattacked = Bb{};
                    continue;
                }

                switch (CastlingRules::castlingSide(king, rook)) {
                    case QueenSide:
                        castlingRules[kingFile][rookFile].unimpeded  = (exBetween(king, C1) | exBetween(rook, D1)) % (Bb{king} + Bb{rook});
                        castlingRules[kingFile][rookFile].unattacked = exBetween(king, C1) | Bb{king};
                        break;

                    case KingSide:
                        castlingRules[kingFile][rookFile].unimpeded  = (exBetween(king, G1) | exBetween(rook, F1)) % (Bb{king} + Bb{rook});
                        castlingRules[kingFile][rookFile].unattacked = exBetween(king, G1) | Bb{king};
                        break;
                }
            }
        }
    }

    bool isLegal(Square king, Square rook, Bb occupied, Bb attacked) const {
        assert (king.on(Rank1));
        assert (rook.on(Rank1));
        assert (king != rook);
        return (occupied & castlingRules[File{king}][File{rook}].unimpeded).none() && (attacked & castlingRules[File{king}][File{rook}].unattacked).none();
    }

    static constexpr CastlingSide castlingSide(Square king, Square rook) {
        return (rook < king) ? QueenSide : KingSide;
    }

    static constexpr Square castlingKingTo(Square king, Square rook) {
        return castlingSide(king, rook).is(QueenSide) ? C1 : G1;
    }

    static constexpr Square castlingRookTo(Square king, Square rook) {
        return castlingSide(king, rook).is(QueenSide) ? D1 : F1;
    }

};

extern const CastlingRules castlingRules;

#endif
