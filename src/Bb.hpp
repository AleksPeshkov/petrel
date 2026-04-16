#ifndef BB_HPP
#define BB_HPP

#include "BitArray.hpp"
#include "Index.hpp"

template <class self_type, class index_type, typename value_type = unsigned>
class BitSet : public BitArray<self_type, value_type> {
    using Base = BitArray<self_type, value_type>;
    friend class BitArray<self_type, value_type>;
    using T = self_type;

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
    constexpr bool has(Index i) const { return static_cast<const T&>(*this).any(T{Index{i}}); }

    // one and only one bit set
    constexpr bool isSingleton() const { assert (static_cast<const T&>(*this).any()); return clearFirst() == 0; }

    // get the first (lowest) bit set
    constexpr Index first() const { return Index{static_cast<Index::_t>(::lsb(v_))}; }

    // get the last (highest) bit set
    constexpr Index last() const { return  Index{static_cast<Index::_t>(::msb(v_))}; }

    // get the singleton bit set
    constexpr Index index() const { assert (isSingleton()); return operator* (); }

    constexpr int popcount() const { return ::popcount(v_); }

    //support for range-based for loop
    constexpr Index operator*() const { return static_cast<const T&>(*this).first(); }
    constexpr T& operator++() { *this = T{clearFirst()}; return static_cast<T&>(*this); }
    constexpr const T& begin() const { return static_cast<const T&>(*this); }
    constexpr T end() const { return T{}; }
};

/**
 * BitBoard type: a bit for each chessboard square
 */
class Bb : public BitSet<Bb, Square, u64_t> {
    Bb (int) = delete; // declared to catch type cast bugs
public:
    constexpr Bb () : BitSet{} {}
    constexpr explicit Bb (_t bb) : BitSet{bb} {}

    constexpr explicit Bb (Square sq) : BitSet{sq} {}
    constexpr explicit Bb (Square::_t sq) : Bb{Square{sq}} {}
    constexpr explicit Bb (File::_t file) : Bb{U64(0x0101'0101'0101'0101) << file} {}
    constexpr explicit Bb (Rank::_t rank) : Bb{U64(0xff) << 8*rank} {}
    constexpr explicit Bb (File file) : Bb{file.v()} {}
    constexpr explicit Bb (Rank rank) : Bb{rank.v()} {}

    constexpr Bb operator ~ () const { return Bb{::byteswap(v_)}; }

    void move(Square from, Square to) { assert (from != to); *this -= Bb{from}; *this += Bb{to}; }

    constexpr Bb pawnAttacks() const { return (*this % Bb{FileA} >> 9u) | (*this % Bb{FileH} >> 7u); }
    constexpr Bb pawnAttacksBack() const { return (*this % Bb{FileA} << 7u) | (*this % Bb{FileH} << 9u); }

    // bidirectional signed rank shift
    constexpr Bb shiftRank(signed r) { return Bb{ r >= 0 ? (v_ << 8*r) : (v_ >> -8*r) }; }

    friend constexpr Bb operator << (Bb bb, unsigned offset) { return Bb{bb.v_ << offset}; }
    friend constexpr Bb operator >> (Bb bb, unsigned offset) { return Bb{bb.v_ >> offset}; }

    friend ostream& operator << (ostream& os, Bb bb) {
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
};

constexpr Bb Square::bbRank() const { return Bb{Rank{*this}} - Bb{*this}; }
constexpr Bb Square::bbFile() const { return Bb{File{*this}} - Bb{*this}; }
constexpr Bb Square::bbDiagonal() const {
    return Bb{U64(0x0102'0408'1020'4080)}.shiftRank(static_cast<signed>(Rank{*this}.v()) + File{*this}.v() - 7) - Bb{*this};
}
constexpr Bb Square::bbAntidiag() const {
    return Bb{U64(0x8040'2010'0804'0201)}.shiftRank(static_cast<signed>(Rank{*this}.v()) - File{*this}.v()) - Bb{*this};
}

constexpr Bb Square::bbDirection(Direction dir) const {
    switch (dir.v()) {
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

/**
 * a bit for each square of a chessboard rank
 */
class BitRank : public BitArray<BitRank, u8_t> {
public:
    constexpr BitRank () : BitArray{} {}
    constexpr explicit BitRank (_t v) : BitArray{v} {}
    constexpr explicit BitRank (File file) : BitArray{static_cast<_t>(1 << file.v())} {}
    constexpr BitRank (Bb bb, Rank rank) : BitArray{small_cast<_t>(bb.v() >> 8*rank.v())} {}
};

// line (file, rank, diagonal) in between two squares (excluding both ends) or 0 (32k)
class CACHE_ALIGN InBetween {
    Square::arrayOf< Square::arrayOf<Bb> > inBetween;

    static constexpr Bb areaInBetween(Square from, Square to) {
        Bb::_t belowFrom{ ::singleton<Bb::_t>(from.v()) - 1 };
        Bb::_t belowTo{ ::singleton<Bb::_t>(to.v()) - 1 };
        return Bb{((belowFrom ^ belowTo) | ::singleton<Bb::_t>(to.v())) ^ ::singleton<Bb::_t>(to.v())};
    }

public:
    consteval InBetween () {
        for (auto from : range<Square>()) {
            for (auto to : range<Square>()) {
                Bb result{};

                if (File{from} == File{to}) {
                    result = areaInBetween(from, to) & from.bbFile();
                }
                else if (Rank{from} == Rank{to}) {
                    result = areaInBetween(from, to) & from.bbRank();
                }
                else if (static_cast<int>(File{from}.v()) + static_cast<int>(Rank{from}.v())
                    == static_cast<int>(File{to}.v()) + static_cast<int>(Rank{to}.v())) {
                    result = areaInBetween(from, to) & from.bbDiagonal();
                }
                else if (static_cast<int>(File{from}.v()) - static_cast<int>(Rank{from}.v())
                    == static_cast<int>(File{to}.v()) - static_cast<int>(Rank{to}.v())) {
                    result = areaInBetween(from, to) & from.bbAntidiag();
                }

                inBetween[from][to] = result;
            }
        }
    }

    constexpr Bb operator() (Square from, Square to) const { return inBetween[from][to]; }

};

// line (file, rank, diagonal) in between two squares (excluding both ends) or 0
extern const InBetween inBetween;

//attack bitboards of the piece types on the empty board (3k)
class CACHE_ALIGN AttacksFrom {
    PieceType::arrayOf< Square::arrayOf<Bb> > attack;
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

extern constinit AttacksFrom attacksFrom;

class CastlingRules {
    struct Rules {
        Bb unimpeded;
        Bb unattacked;
    };

    File::arrayOf< File::arrayOf<Rules> > castlingRules;

    static constexpr Bb exBetween(Square king, Square rook) { return ::inBetween(king, rook) | Bb{rook}; }

public:
    consteval CastlingRules () {
        for (auto kingFile : range<File>()) {
            for (auto rookFile : range<File>()) {
                Square king{kingFile.v(), Rank1};
                Square rook{rookFile.v(), Rank1};

                if (king == rook) {
                    castlingRules[kingFile][rookFile].unimpeded  = Bb{};
                    castlingRules[kingFile][rookFile].unattacked = Bb{};
                    continue;
                }

                switch (CastlingRules::castlingSide(king, rook).v()) {
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
        return castlingRules[File{king}][File{rook}].unimpeded.none(occupied)
            && castlingRules[File{king}][File{rook}].unattacked.none(attacked);
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

extern constinit CastlingRules castlingRules;

#endif
