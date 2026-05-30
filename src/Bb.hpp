#ifndef BB_HPP
#define BB_HPP

#include "BitArray.hpp"
#include "Index.hpp"

/**
 * a bit for each square of a chessboard rank
 */
class BitRank : public BitArray<BitRank, u8_t> {
public:
    static constexpr _t mask() { return 0xff; }
    constexpr BitRank () : BitArray{} {}
    constexpr explicit BitRank (_t v) : BitArray{v} {}
    constexpr explicit BitRank (File file) : BitArray{static_cast<_t>(1 << +file)} {}
};

/**
 * BitBoard type: a bit for each chessboard square
 */
class Bb : public BitSet<Bb, Square, u64_t> {
    Bb (int) = delete; // declared to catch type cast bugs
    constexpr Bb operator << (unsigned offset) const { return Bb{v_ << offset}; }
    constexpr Bb operator >> (unsigned offset) const { return Bb{v_ >> offset}; }

public:
    constexpr Bb () : BitSet{} {}
    constexpr explicit Bb (_t bb) : BitSet{bb} {}

    constexpr explicit Bb (Square::_t sq) : Bb{::singleton<_t>(sq)} {}
    constexpr explicit Bb (File::_t file) : Bb{U64(0x0101'0101'0101'0101) << file} {}
    constexpr explicit Bb (Rank::_t rank) : Bb{static_cast<_t>(BitRank::mask()) << 8*rank} {}

    constexpr explicit Bb (Square sq) : Bb{*sq} {}
    constexpr explicit Bb (File file) : Bb{*file} {}
    constexpr explicit Bb (Rank rank) : Bb{*rank} {}

    constexpr Bb (Rank rank, BitRank br) : Bb{static_cast<_t>(br.v()) << 8*+rank} {}
    constexpr BitRank bitRank(Rank rank) {
        return BitRank{static_cast<BitRank::_t>( v_>> 8*+rank & BitRank::mask() )};
    }

    constexpr Bb operator ~ () const { return Bb{::byteswap(v_)}; }
    constexpr void move(Square from, Square to) { assert (from != to); *this -= Bb{from}; *this += Bb{to}; }

    constexpr Bb pForward() const { return *this >> 8u; }
    constexpr Bb pBackward() const { return *this << 8u; }
    constexpr Bb pForwardDiag() const { return (*this % Bb{FileA} >> 9u) | (*this % Bb{FileH} >> 7u); }
    constexpr Bb pBackwardDiag() const { return (*this % Bb{FileA} << 7u) | (*this % Bb{FileH} << 9u); }

    // bidirectional signed rank shift
    constexpr Bb shiftRank(signed r) { return Bb{ r >= 0 ? (v_ << 8*r) : (v_ >> -8*r) }; }

};

constexpr Bb Square::bbRank() const { return Bb{rank()} - Bb{*this}; }
constexpr Bb Square::bbFile() const { return Bb{file()} - Bb{*this}; }
constexpr Bb Square::bbDiagonal() const {
    return Bb{U64(0x0102'0408'1020'4080)}.shiftRank(+rank() + +file() - 7) - Bb{*this};
}
constexpr Bb Square::bbAntidiag() const {
    return Bb{U64(0x8040'2010'0804'0201)}.shiftRank(+rank() - +file()) - Bb{*this};
}

constexpr Bb Square::bbDirection(Direction dir) const {
    switch (*dir) {
        case FileDir:     return bbFile();
        case RankDir:     return bbRank();
        case DiagonalDir: return bbDiagonal();
        case AntidiagDir: return bbAntidiag();
    }
}

constexpr Bb Square::bb(signed df, signed dr) const {
    // https://www.chessprogramming.org/0x88
    auto sq0x88 = v_ + (v_ & 070) + df + 16*dr;

    if (sq0x88 & 0x88) {
        return {}; //out of chess board
    }

    return Bb{ static_cast<Square::_t>((sq0x88 + (sq0x88 & 7)) >> 1) };
}

// line (file, rank, diagonal) in between two squares (excluding both ends) or 0 (32k)
class CACHE_ALIGN InBetween {
    array<Bb, Square, Square> inBetween;

    static constexpr Bb areaInBetween(Square from, Square to) {
        Bb areaFrom{ Bb{from}.v() - 1 };
        Bb areaTo{ Bb{to}.v() - 1 };
        return (areaFrom ^ areaTo) % Bb{to};
    }

public:
    consteval InBetween () {
        for (auto from : range<Square>()) {
            for (auto to : range<Square>()) {
                Bb result{};

                if (from.file() == to.file()) {
                    result = areaInBetween(from, to) & from.bbFile();
                }
                else if (from.rank() == to.rank()) {
                    result = areaInBetween(from, to) & from.bbRank();
                }
                else if (+from.file() + +from.rank() == +to.file() + +to.rank()) {
                    result = areaInBetween(from, to) & from.bbDiagonal();
                }
                else if (+from.file() - +from.rank() == +to.file() - +to.rank()) {
                    result = areaInBetween(from, to) & from.bbAntidiag();
                }

                inBetween[from][to] = result;
            }
        }
    }

    constexpr Bb operator() (Square from, Square to) const { return inBetween[from][to]; }

};

inline ostream& operator << (ostream& os, Bb bb) {
    os << "    a b c d e f g h\n";
    for (auto rank : range<Rank>()) {
        os << Rank{rank} << " |";
        for (auto file : range<File>()) {
            Square sq{file, rank};
            os << " " << (bb.has(sq) ? 'x'  : '.');
        }
        os << '\n';
    }
    os << "    a b c d e f g h\n";
    return os;
}

// line (file, rank, diagonal) in between two squares (excluding both ends) or 0
extern const InBetween inBetween;

//attack bitboards of the piece types on the empty board (3k)
class CACHE_ALIGN AttacksFrom {
    array<Bb, PieceType, Square> attack;
public:
    consteval AttacksFrom () {
        for (auto sq: range<Square>()) {
            attack[PieceType{Rook}][sq]   = sq.bbFile() + sq.bbRank();
            attack[PieceType{Bishop}][sq] = sq.bbDiagonal() + sq.bbAntidiag();
            attack[PieceType{Queen}][sq]  = attack[PieceType{Rook}][sq] + attack[PieceType{Bishop}][sq];

            attack[PieceType{Pawn}][sq] = sq.bb(-1, Rank3 - Rank2) + sq.bb(+1, Rank3 - Rank2);

            attack[PieceType{Knight}][sq] =
                sq.bb(+2, +1) + sq.bb(+2, -1) + sq.bb(+1, +2) + sq.bb(+1, -2) +
                sq.bb(-2, -1) + sq.bb(-2, +1) + sq.bb(-1, -2) + sq.bb(-1, +2);

            attack[PieceType{King}][sq] =
                sq.bb(+1, +1) + sq.bb(+1, 0) + sq.bb(0, +1) + sq.bb(+1, -1) +
                sq.bb(-1, -1) + sq.bb(-1, 0) + sq.bb(0, -1) + sq.bb(-1, +1);
        }
    }

    constexpr Bb operator() (PieceType ty, Square sq) const { return attack[ty][sq]; }
};
extern const AttacksFrom attacksFrom;

class CastlingRules {
    struct Rules {
        Bb unimpeded;
        Bb unattacked;
    };

    array<Rules, File, File> castlingRules;

    static constexpr Bb exBetween(Square king, Square rook) { return ::inBetween(king, rook) | Bb{rook}; }

public:
    consteval CastlingRules () {
        for (auto kingFile : range<File>()) {
            for (auto rookFile : range<File>()) {
                Square king{kingFile, Rank1};
                Square rook{rookFile, Rank1};

                if (king == rook) {
                    castlingRules[kingFile][rookFile].unimpeded  = Bb{};
                    castlingRules[kingFile][rookFile].unattacked = Bb{};
                    continue;
                }

                switch (*CastlingRules::castlingSide(king, rook)) {
                    case QueenSide:
                        castlingRules[kingFile][rookFile].unimpeded  = (exBetween(king, Square{C1}) | exBetween(rook, Square{D1})) % (Bb{king} + Bb{rook});
                        castlingRules[kingFile][rookFile].unattacked = exBetween(king, Square{C1}) | Bb{king};
                        break;

                    case KingSide:
                        castlingRules[kingFile][rookFile].unimpeded  = (exBetween(king, Square{G1}) | exBetween(rook, Square{F1})) % (Bb{king} + Bb{rook});
                        castlingRules[kingFile][rookFile].unattacked = exBetween(king, Square{G1}) | Bb{king};
                        break;
                }
            }
        }
    }

    bool isLegal(Square king, Square rook, Bb occupied, Bb attacked) const {
        assert (king.on(Rank1));
        assert (rook.on(Rank1));
        assert (king != rook);
        return castlingRules[king.file()][rook.file()].unimpeded.none(occupied)
            && castlingRules[king.file()][rook.file()].unattacked.none(attacked);
    }

    static constexpr CastlingSide castlingSide(Square king, Square rook) {
        return CastlingSide{rook < king ? QueenSide : KingSide};
    }

    static constexpr Square castlingKingTo(Square king, Square rook) {
        return castlingSide(king, rook).is(QueenSide) ? Square{C1} : Square{G1};
    }

    static constexpr Square castlingRookTo(Square king, Square rook) {
        return castlingSide(king, rook).is(QueenSide) ? Square{D1} : Square{F1};
    }

};
extern const CastlingRules castlingRules;

#endif
