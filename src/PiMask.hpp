#ifndef PI_MASK_HPP
#define PI_MASK_HPP

#include "Index.hpp"
#include "BitSet.hpp"
#include "BitArray128.hpp"

class CACHE_ALIGN VectorOfAll {
    using _t = vu8x16_t;
    using Index0x100 = Index<0x100>;
    Index0x100::arrayOf<_t> v;

public:
    constexpr VectorOfAll () {
        for (auto i : range<Index0x100>()) {
            v[i] = ::all(i);
        }
    }

    constexpr const _t& operator[] (u8_t i) const { return v[Index0x100{i}]; }
    constexpr const _t& operator[] (Pi pi) const { return v[Index0x100{pi}]; }
};

extern const VectorOfAll vectorOfAll;

/**
 * class used for enumeration of piece vectors
 */

#ifndef NEON_VECTOR

class PieceSet : public BitSet<PieceSet, Pi> {
    using Base = BitSet<PieceSet, Pi>;
public:
    using Base::Base;

    constexpr Pi vacantMostValuable() const {
        for (Pi pi : range<Pi>()) {
            if (!has(pi)) {
                return pi;
            }
        }
        assert (false);
        return Pi{TheKing};
    }
};

#else

class PieceSet : public BitSet<PieceSet, Pi, u64_t> {
    using Base = BitSet<PieceSet, Pi, u64_t>;
public:
    explicit constexpr PieceSet (_t n = 0) : Base{n & U64(0x8888'8888'8888'8888)} {}
    explicit constexpr PieceSet (Index i) : Base{::singleton<u64_t>(i*4 + 3)} {}

    // get the first (lowest) bit set
    constexpr Index first() const {
        return Index{static_cast<Index::_t>(::lsb(v) >> 2)};
    }

    // get the last (highest) bit set
    constexpr Index last() const {
        return  Index{static_cast<Index::_t>(::msb(v) >> 2)};
    }

    constexpr Pi vacantMostValuable() const {
        for (auto pi : range<Pi>()) {
            if (!has(pi)) {
                return pi;
            }
        }
        assert (false);
        return Pi{TheKing};
    }
};

#endif // NEON_VECTOR

consteval Pi::arrayOf<vu8x16_t> make_pi_single_array() {
    Pi::arrayOf<vu8x16_t> arr;
    return arr;
}

class PiSingle {
    using _t = vu8x16_t;
    Pi::arrayOf<_t> v;

public:
    constexpr PiSingle() {
        for (auto pi : range<Pi>()) {
            std::array<uint8_t, 16> vec = {}; // zero
            vec[static_cast<int>(pi)] = 0xff;
            v[pi] = std::bit_cast<vu8x16_t>(vec);
        }
    }

    constexpr const _t& operator[] (Pi pi) const { return v[pi]; }
};

extern const PiSingle piSingle;

///piece vector of boolean values: false (0) or true (0xff)
class PiMask : public BitArray<PiMask, vu8x16_t> {
public:
    using Base = BitArray<PiMask, vu8x16_t>;
    using typename Base::_t;
    using Base::v;
    using Base::any;

    constexpr static _t zero() { return ::all(0); }

    constexpr PiMask () : Base{zero()} {}
    constexpr PiMask (Pi pi) : Base( ::piSingle[pi] ) {}
    explicit constexpr PiMask (_t a) : Base{a} { assertOk(); }

    static constexpr PiMask equals(_t a, _t b) { return PiMask{a == b}; }
    static constexpr PiMask notEquals(_t a, _t b) { return PiMask{a != b}; }
    static constexpr PiMask any(_t a) { return notEquals(a, zero()); }
    static constexpr PiMask all() { return PiMask{::all(0xff)}; }

    constexpr operator const _t& () const { return v; }

    // check if either 0 or 0xff bytes are set
    bool isOk() const { return ::equals(v, v != zero()); }

    // assert if either 0 or 0xff bytes are set
    constexpr void assertOk() const { assert (isOk()); }

    explicit operator PieceSet() const {
        assertOk();
        #ifndef NEON_VECTOR
            return PieceSet{static_cast<PieceSet::_t>(::mask(v))};
        #else
            return PieceSet{::mask4(v)};
        #endif
    }

    constexpr bool has(Pi pi) const { return PieceSet{*this}.has(pi); }
    constexpr bool none() const { return PieceSet{*this}.none(); }
    constexpr bool isSingleton() const { return PieceSet{*this}.isSingleton(); }

    // get the singleton piece index
    constexpr Pi index() const { return PieceSet{*this}.index(); }

    // most valuable piece in the first (lowest) set bit
    constexpr Pi mostValuable() const { return PieceSet{*this}.first(); }

    // least valuable pieces in the last (highest) set bit
    constexpr Pi leastValuable() const { return PieceSet{*this}.last(); }

    constexpr int popcount() const { return PieceSet{*this}.popcount(); }

    constexpr PieceSet begin() const { return PieceSet{*this}; }
    constexpr PieceSet end() const { return PieceSet{}; }

    friend ostream& operator << (ostream& out, PiMask mask) {
        return out << PieceSet{mask};
    }
};

class PiRank : public BitArray<PiRank, vu8x16_t> {
    using Base = BitArray<PiRank, vu8x16_t>;

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

class PiSquare {
    using _t = Square::_t;
    constexpr static _t NoSq = static_cast<_t>(0xff); // captured piece

    // defined to make debugging clear
    union {
        Pi::arrayOf<_t> square;
        vu8x16_t vu8x16;
    };

    constexpr void set(Pi pi, _t sq) { square[pi] = sq; }
    constexpr vu8x16_t vector(_t e) const { return ::vectorOfAll[e]; }

public:
    constexpr PiSquare () {
        for (auto pi : range<Pi>()) {
            square[pi] = NoSq;
        }
    }

    constexpr bool isOk(Pi pi) const { return !isEmpty(pi) && pieceAt(square[pi]) == pi; }

    #ifdef NDEBUG
        constexpr void assertOk(Pi) const {}
    #else
        constexpr void assertOk(Pi pi) const { assert (isOk(pi)); }
    #endif

    void drop(Pi pi, Square sq) { assert (isEmpty(pi)); assert (!has(sq)); set(pi, sq); }
    void clear(Pi pi) { assertOk(pi); square[pi] = NoSq; }
    void move(Pi pi, Square sq) { assertOk(pi); assert (!has(sq)); set(pi, sq); }

    void castle(Square kingTo, Pi theRook, Square rookTo) {
        assert (TheKing != theRook);
        assert (squareOf(Pi{TheKing}).on(Rank1));
        assert (squareOf(theRook).on(Rank1));
        assert (kingTo.is(G1) || kingTo.is(C1));
        assert (rookTo.is(F1) || rookTo.is(D1));

        assertOk(Pi{TheKing});
        assertOk(theRook);
        set(Pi{TheKing}, kingTo);
        set(theRook, rookTo);
        assertOk(Pi{TheKing});
        assertOk(theRook);
    }

    constexpr bool isEmpty(Pi pi) const { return square[pi] == NoSq; }
    constexpr Square squareOf(Pi pi) const { assertOk(pi); return Square{square[pi]}; }

    constexpr bool has(_t sq) const { return piecesAt(sq).any(); }
    constexpr Pi pieceAt(_t sq) const { assert (has(sq)); return piecesAt(sq).index(); }

    constexpr PiMask pieces() const { return PiMask::notEquals(vu8x16, ::all(NoSq)); }
    constexpr PiMask piecesAt(_t sq) const { return PiMask::equals(vu8x16, vector(sq)); }

    constexpr PiMask piecesOn(Rank::_t rank) const {
        return PiMask::equals(
            vu8x16 & vector(static_cast<_t>(NoSq ^ static_cast<_t>(File::Mask))),
            vector(Square{static_cast<File::_t>(0), rank})
        );
    }
};

class PiType {
    enum pieces_t : u8_t {
        None   = 0,
        Queens  = ::singleton<u8_t>(Queen),
        Rooks   = ::singleton<u8_t>(Rook),
        Bishops = ::singleton<u8_t>(Bishop),
        Knights = ::singleton<u8_t>(Knight),
        Pawns   = ::singleton<u8_t>(Pawn),
        Kings   = ::singleton<u8_t>(King),
        Sliders  = Queens | Rooks | Bishops,
        Leapers  = Pawns | Knights | Kings,
        All      = Sliders | Leapers,
        Officers = Queens | Rooks | Bishops | Knights,
        NonPawns = Queens | Rooks | Bishops | Knights | Kings,
        NonKings = Queens | Rooks | Bishops | Knights | Pawns,
        PNBR     = Pawns | Knights | Bishops | Rooks,
        PNB      = Pawns | Knights | Bishops,
    };

    using element_type = pieces_t;

    static constexpr PieceType::arrayOf<element_type> LessOrEqualValue = {
        NonKings, // Queen
        PNBR,  // Rook
        PNB,   // Bishop
        PNB,   // Knight
        Pawns, // Pawn
        All,   // King
    };

    static constexpr PieceType::arrayOf<element_type> LessValue = {
        PNBR,  // Queen
        PNB,   // Rook
        Pawns, // Bishop
        Pawns, // Knight
        None,  // Pawn
        NonKings, // King
    };

    // defined to make debugging clear
    union {
        Pi::arrayOf<pieces_t> type;
        vu8x16_t vu8x16;
    };

    constexpr element_type element(PieceType::_t ty) const { return static_cast<element_type>(::singleton<u8_t>(ty)); }
    constexpr vu8x16_t vector(element_type e) const { return ::vectorOfAll[static_cast<u8_t>(e)]; }
    constexpr vu8x16_t vector(PieceType::_t ty) const { return vector(element(ty)); }

    constexpr bool has(Pi pi, element_type e) const { assertOk(pi); return (static_cast<u8_t>(type[pi]) & static_cast<u8_t>(e)) != 0; }
    constexpr bool is(Pi pi, PieceType::_t ty) const { assertOk(pi);  return has(pi, element(ty)); }
    constexpr PiMask any(element_type e) const { return PiMask::any(vu8x16 & vector(e)); }

public:
    constexpr PiType () {
        for (auto pi : range<Pi>()) {
            type[pi] = None;
        }
    }

    constexpr bool isOk(Pi pi) const { return !isEmpty(pi) && ::isSingleton(static_cast<u8_t>(type[pi])); }

    #ifdef NDEBUG
        constexpr void assertOk(Pi) const {}
    #else
        constexpr void assertOk(Pi pi) const { assert (isOk(pi)); }
    #endif

    void drop(Pi pi, PieceType::_t ty) { assert (isEmpty(pi)); assert (pi != TheKing || ty == King); type[pi] = element(ty); }
    void clear(Pi pi) { assertOk(pi); assert (pi != TheKing); assert (!is(pi, King)); type[pi] = None; }
    void promote(Pi pi, PromoType::_t ty) { assert (isPawn(pi)); type[pi] = element(ty); }

    constexpr bool isEmpty(Pi pi) const { return type[pi] == None; }
    constexpr bool isPawn(Pi pi) const { return is(pi, Pawn); }
    constexpr bool isRook(Pi pi) const { return is(pi, Rook); }
    constexpr bool isSlider(Pi pi) const { assertOk(pi); return has(pi, Sliders); }
    constexpr PieceType typeOf(Pi pi) const { assertOk(pi); return PieceType{static_cast<PieceType::_t>( ::lsb(static_cast<unsigned>(type[pi])) )}; }

    constexpr PiMask pieces() const { return PiMask::any(vu8x16); }
    constexpr PiMask piecesOfType(PieceType::_t ty) const { assert (!PieceType{ty}.is(King)); return any(element(ty)); }

    // Queens, Rooks, Bishops
    constexpr PiMask sliders() const { return any(Sliders); }

    // King, Pawns, Knights
    constexpr PiMask leapers() const { return any(Leapers); }

    // Queens, Rooks, Bishops, Knights
    constexpr PiMask officers() const { return any(Officers); }

    // King, Queens, Rooks, Bishops, Knights
    constexpr PiMask nonPawns() const { return any(NonPawns); }

    // Queens, Rooks, Bishops, Knights, Pawns
    constexpr PiMask nonKing() const { return any(NonKings); }

    // less valuable pieces than given piece type
    constexpr PiMask lessValue(PieceType ty) const {return any(LessValue[ty]); }

    // less or equal value pieces than given piece type
    constexpr PiMask lessOrEqualValue(PieceType ty) const { return any(LessOrEqualValue[ty]); }
};

class PiTrait {
    enum trait_t : u8_t {
        Empty       = 0,
        Checkers    = ::singleton<u8_t>(0), // any piece actually attacking enemy king
        Pinners     = ::singleton<u8_t>(1), // potential pinner: sliding piece that can attack the enemy king square on empty board
        Castlings   = ::singleton<u8_t>(2), // rook with castling rights
        EnPassants  = ::singleton<u8_t>(3), // pawn can be legally captured en passant OR pawn can perform a legal en passant capture
        Promotables = ::singleton<u8_t>(4), // any pawn on the 7th rank
        CheckersPinners = Checkers | Pinners,  // Checkers + Pinners
    };

    using element_type = trait_t;

    // defined to make debugging clear
    union {
        Pi::arrayOf<element_type> trait;
        vu8x16_t vu8x16;
    };

    constexpr PiMask any(element_type e) const { return PiMask::any(vu8x16 & ::vectorOfAll[static_cast<u8_t>(e)]); }
    constexpr void clear(element_type e) { vu8x16 &= ::vectorOfAll[0xff ^ static_cast<u8_t>(e)]; }

    constexpr bool has(Pi pi, element_type e) const {
        return (static_cast<u8_t>(trait[pi]) & static_cast<u8_t>(e)) != 0;
    }

    constexpr void add(Pi pi, element_type e) {
        trait[pi] = static_cast<element_type>(static_cast<u8_t>(trait[pi]) | static_cast<u8_t>(e));
    }

    constexpr void clear(Pi pi, element_type e) {
        trait[pi] = static_cast<element_type>(static_cast<u8_t>(trait[pi]) & (0xffu ^ static_cast<u8_t>(e)));
    }

public:
    constexpr PiTrait () {
        for (auto pi : range<Pi>()) {
            trait[pi] = Empty;
        }
    }

    constexpr void clear(Pi pi) { trait[pi] = Empty; }
    constexpr bool isEmpty(Pi pi) const { return trait[pi] == Empty; }

    constexpr PiMask castlingRooks() const { return any(Castlings); }
    constexpr bool isCastling(Pi pi) const { return has(pi, Castlings); }
    constexpr void setCastling(Pi pi) { assert (!isCastling(pi)); add(pi, Castlings); }
    constexpr void clearCastlings() { clear(Castlings); }

    constexpr PiMask enPassantPawns() const { return any(EnPassants); }
    constexpr Pi getEnPassant() const { Pi pi = enPassantPawns().index(); return pi; }
    constexpr bool isEnPassant(Pi pi) const { return has(pi, EnPassants); }
    constexpr void setEnPassant(Pi pi) { add(pi, EnPassants); }
    constexpr void clearEnPassant(Pi pi) { assert (isEnPassant(pi)); clear(pi, EnPassants); }
    constexpr void clearEnPassants() { clear(EnPassants); }

    constexpr PiMask pinners() const { return any(Pinners); }
    constexpr bool isPinner(Pi pi) const { return has(pi, Pinners); }
    constexpr void clearPinners() { clear(Pinners); }
    constexpr void setPinner(Pi pi) { assert (!isPinner(pi)); add(pi, Pinners); }
    constexpr void clearPinner(Pi pi) { clear(pi, Pinners); }

    constexpr PiMask checkers() const { return any(Checkers); }
    constexpr void clearCheckers() { clear(Checkers); }
    constexpr void setChecker(Pi pi) { assert (!has(pi, Checkers)); add(pi, Checkers); }

    constexpr PiMask promotables() const { return any(Promotables); }
    constexpr bool isPromotable(Pi pi) const { return has(pi, Promotables); }
    constexpr void setPromotable(Pi pi) { add(pi, Promotables); }
};

class ShuffleToFront {
    std::array<vu8x16_t, 16> shuffle;
public:
    constexpr ShuffleToFront() : shuffle{{
        {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
        {1,0,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
        {2,0,1,3,4,5,6,7,8,9,10,11,12,13,14,15},
        {3,0,1,2,4,5,6,7,8,9,10,11,12,13,14,15},
        {4,0,1,2,3,5,6,7,8,9,10,11,12,13,14,15},
        {5,0,1,2,3,4,6,7,8,9,10,11,12,13,14,15},
        {6,0,1,2,3,4,5,7,8,9,10,11,12,13,14,15},
        {7,0,1,2,3,4,5,6,8,9,10,11,12,13,14,15},
        {8,0,1,2,3,4,5,6,7,9,10,11,12,13,14,15},
        {9,0,1,2,3,4,5,6,7,8,10,11,12,13,14,15},
        {10,0,1,2,3,4,5,6,7,8,9,11,12,13,14,15},
        {11,0,1,2,3,4,5,6,7,8,9,10,12,13,14,15},
        {12,0,1,2,3,4,5,6,7,8,9,10,11,13,14,15},
        {13,0,1,2,3,4,5,6,7,8,9,10,11,12,14,15},
        {14,0,1,2,3,4,5,6,7,8,9,10,11,12,13,15},
        {15,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14},
    }} {}

    constexpr const vu8x16_t& operator [] (Pi pi) const { return shuffle[pi]; }
};

class PiOrder {
    using PiArray = Pi::arrayOf<Pi>;

    union {
        PiArray order;
        vu8x16_t vu8x16;
    };

    static constexpr ShuffleToFront shuffleToFront{};
    static constexpr vu8x16_t ordered = shuffleToFront[Pi{static_cast<Pi::_t>(0)}];

    constexpr bool isOk() const {
        // check all values [0, 15] are represented
        u32_t mask = 0;
        for (int i = 0; i < 16; ++i) {
            mask |= ::singleton<int>(::u8(vu8x16, i));
        }
        return ::popcount(mask) == 16;
    }

    constexpr void assertOk() const { assert (isOk()); }

public:
    constexpr PiOrder () : vu8x16{ordered} { assertOk(); }

    constexpr PiMask operator () (PiMask pieces) const {
        return PiMask{::shufflevector(static_cast<vu8x16_t>(pieces), vu8x16)};
    }

    constexpr const Pi& operator[] (Pi pi) const { return order[pi]; }

    PiOrder& forward(Pi pi) {
        // find index of pi in the shuffled vector
        PiMask mask = PiMask::equals(vu8x16, ::vectorOfAll[pi]);
        // shuffle selected pi to the first position
        vu8x16 = ::shufflevector(vu8x16, std::bit_cast<vu8x16_t>(shuffleToFront[mask.index()]));
        assertOk();
        return *this;
    }
};

class PiOrdered {
    PiOrder order; // vector of piece indices
    PieceSet mask; // shuffled PiMask bitset

    constexpr explicit operator const PieceSet& () const { return mask; }

public:
    constexpr PiOrdered (PiMask pieces, PiOrder o) : order{o}, mask{order(pieces)} {}

    constexpr friend bool operator == (const PiOrdered& a, const PieceSet& b) { return a.mask == b; }

    constexpr Pi operator * () const { return order[*mask]; }
    constexpr PiOrdered& operator ++ () { ++mask; return *this; }
    constexpr auto begin() const { return *this; }
    constexpr auto end() const { return PieceSet{}; }
};

#endif
