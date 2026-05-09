#ifndef HISTORY_HPP
#define HISTORY_HPP

#include "Index.hpp"
#include "Score.hpp"

/**
 * Inserts or moves `value` to position `Pos` in the array, preserving uniqueness.
 * - If `value` is already present in [0, Pos): no action (already prioritized).
 * - Else if `value` is found at i >= Pos: moves it to position `Pos`.
 * - Otherwise: inserts at `Pos`, shifting [Pos, end-1] right (last element dropped).
 *
 * Useful for history heuristics where earlier positions have higher priority.
 *
 * @tparam Pos Target position (should be < Size)
 * @tparam T Element type
 * @tparam Size Array size
 * @param arr Array to update
 * @param value Value to insert or move
 */
template <size_t Pos = 0, typename T, size_t Size>
void insert_unique(std::array<T, Size>& arr, const T& value) {
    static_assert (Pos < Size, "Pos must be less than container size");

    auto begin = arr.begin();
    auto pos = begin + Pos;
    auto end = begin + Size;

    // already in high-priority zone?
    if (std::find(begin, pos, value) != pos) { return; }

    // found later?
    auto found = std::find(pos, end, value);
    if (found != end) {
        std::rotate(pos, found, found + 1);
    } else {
        // insert at Pos, shift right, drop last
        std::copy_backward(pos, end - 1, end);
        *pos = value;
    }
}

template<int _Size>
class CACHE_ALIGN HistoryMoves {
public:
    static constexpr int Size = _Size;
    struct Index; STRUCT_INDEX (Index, Size);

private:
    using _t = array<HistoryMove, Color, HistoryType, Square, Square, Index>;
    _t v_;

public:
    void clear() {
        std::memset(&v_, 0, sizeof(v_)); //TRICK: HistoryMove{None} == uint16_t{0}
    }

    constexpr HistoryMove get(Index i, Color color, HistoryMove move) const {
        assert (move.any());
        return v_[color][move.historyType()][move.from()][move.to()][i];
    }

    constexpr void set(Color color, HistoryMove move, HistoryMove bestMove) {
        assert (move.any()); assert (bestMove.any());
        insert_unique(v_[color][move.historyType()][move.from()][move.to()], bestMove);
    }
};

class CACHE_ALIGN PrincipalVariation {
    // final PV size + triangular arrays of subPVs (including final null moves)
    static constexpr auto triangularArraySize = Ply::size() + Ply::size()*(Ply::size()+1) / 2;
public:
    struct Index; STRUCT_INDEX (Index, triangularArraySize);
    using Move = UciMove;

private:
    array<Move, Index> moves_;

    Ply depth_{0}; // latest PV iteration depth
    Score score_{NoScore}; // latest PV score

public:
    PrincipalVariation () { clear(); }

    void clear() {
        depth_ = 0_ply;
        score_ = Score{NoScore};
    }

    // clear child PV space: moves_[i] = {}
    void clear(Index i) { moves_[i] = {}; }

    /// set parent PV move and copy child PV
    /// @return Index of space past PV
    Index set(Index parent, Move childMove, Index childPv) {
        auto from = childPv;
        auto to   = parent;

        moves_[to++] = childMove;
        if (from == to) {
            // no need to copy, but still needs to find move list end
            while ((moves_[to++]).any()) {}
        } else {
            assert (from > to);
            // copies null move terminated move list (including last null)
            while ((moves_[to++] = moves_[from++]).any()) {}
        }

        return to; // new childPv
    }

    /// set root PV depth, score and PV moves
    /// @return Index of space past PV
    Index set(Ply depth, Score score, Move bestRootMove, Index childPv) {
        depth_ = depth;
        score_ = score;
        return set(Index{0}, bestRootMove, childPv);
    }

    void set(Move bestRootMove) {
        clear();
        moves_[Index{0}] = bestRootMove;
        moves_[Index{1}] = {};
    }

    void set(Ply depth) { depth_ = depth; }

    const auto* moves() const { return &moves_[Index{0}]; }
    auto move(Ply ply) const { return moves_[Index{+ply}]; }
    auto depth() const { return depth_; }
    auto score() const { return score_; }
};

// https://www.talkchess.com/forum/viewtopic.php?p=554664#p554664
class ZHash {
    using _t = u64_t;
    _t v_;
    static constexpr _t hash(Z z) { return ::singleton<_t>(z & 077); }

public:
    constexpr ZHash () : v_{0} {}
    constexpr ZHash (ZHash zHash, Z z) : v_{zHash.v_ | hash(z)} {}
    constexpr bool none(Z z) const { return (v_ & hash(z)) == 0; }
};

template <int Size>
class RingIndex {
public:
    using _t = int;
private:
    _t v_;
public:
    static constexpr int size() { static_assert (Size >= 1); return Size; }
    static constexpr _t last() { return size() - 1; }

    constexpr RingIndex (_t v) : v_{v} { assertOk(); }
    constexpr int operator + () const { return v_; }
    constexpr _t operator * () const { return v_; }

    constexpr bool isOk() const { return 0 <= v_ && v_ < size(); }
    constexpr void assertOk() const { assert (isOk()); }

    constexpr RingIndex operator + (int d) const { assert (d >= 0); _t r = v_ + d; return RingIndex{r < size() ? r : r - size()}; }
    constexpr RingIndex operator - (int d) const { assert (d >= 0); _t r = v_ - d; return RingIndex{r >= 0 ? r : r + size()}; }

    constexpr RingIndex& operator ++ () { v_ = v_ < last() ? v_ + 1 : 0; return *this; }
    constexpr RingIndex& operator -- () { v_ = v_ > 0 ? v_ - 1 : last(); return *this; }
    constexpr RingIndex operator ++ (int) { auto r = *this; ++(*this); return r; }
    constexpr RingIndex operator -- (int) { auto r = *this; --(*this); return r; }

    friend constexpr bool operator == (RingIndex a, RingIndex b) { return a.v_ == b.v_; }
};

class CACHE_ALIGN RepSide {
    friend class TestRepSide; // unit test version

    struct _t {
        Z z;
        ZHash zHash;
    };

    struct DupIndex; STRUCT_INDEX (DupIndex, 25);
    array<_t, DupIndex> dup; // only duplicate positions for true 3-fold repetition detection
    ZHash dupZHash_{};
    int dupCount_ = 0; // number of duplicants (only for unit tests)

    using RingIndex = ::RingIndex<50>; // 50 because of 50-move draw rule
    static constexpr RingIndex Last{RingIndex::last()};
    array<_t, RingIndex> ring; // ring buffer of all game history positions of the same side to move
    ZHash ringZHash_{};
    int ringCount_ = 0; // number of all game history positions (max 50)
    RingIndex last_{Last}; // last added position index

public:
    constexpr void clear() {
        dupZHash_ = {};
        dupCount_ = 0;
        ringZHash_ = {};
        ringCount_ = 0;
        last_ = Last; //TRICK: ring buffer overflow will add first position to Index{0}
    }

    void push(Z z) {
        ++last_;
        ringCount_ = std::min(ringCount_+1, RingIndex::size());
        ring[last_].z = z;
        ring[last_].zHash = ringZHash_;
        ringZHash_ = ZHash{ringZHash_, z};
    }

    void normalize() {
        dupZHash_ = {};
        dupCount_ = 0;
        if (ringCount_ <= 2) { return; } // no space for any repetition

        int dupFound = 0;
        {
            ZHash dupZHash{}; // zHash of the last duplicant
            for (int i = 0; i < ringCount_ - 2; ++i) {
                Z z = ring[last_ - i].z;

                // test if already present in duplicant array
                if (!dupZHash.none(z)) {
                    bool already = false;
                    for (int d = dupFound-1; true; --d) {
                        if (dup[DupIndex{d}].z == z) { already = true; break; }
                        if (dup[DupIndex{d}].zHash.none(z)) { break; }
                        assert (d > 0);
                    }
                    if (already) { continue; }
                }

                // test for repetition (i and i+1 cannot be chess repetitions)
                for (int j = i + 2; j < ringCount_; ++j) {
                    if (ring[last_ - j].z == z) {
                        // append to duplicant array
                        dup[DupIndex{dupFound}].z = z;
                        dup[DupIndex{dupFound}].zHash = dupZHash;
                        dupZHash = ZHash{dupZHash, z};
                        ++dupFound;
                        break;
                    }
                    if (ring[last_ - j].zHash.none(z)) { break; }
                }
            }
        }
        if (dupFound == 0) { return; } // no repetitions found, already cleared

        // rehash in opposite direction
        ZHash dupZHash{};
        for (int d = dupFound-1; d >= 0; --d) {
            dup[DupIndex{d}].zHash = dupZHash;
            dupZHash = ZHash{dupZHash, dup[DupIndex{d}].z};
        }

        dupZHash_ = dupZHash;
        dupCount_ = dupFound;
    }

    bool has(Z z) const {
        if (dupZHash_.none(z)) { return false; }

        for (int d{0}; true; ++d) {
            if (dup[DupIndex{d}].z == z) { return true; }
            if (dup[DupIndex{d}].zHash.none(z)) { return false; }
            assert (d < dupCount_);
        }
    }
};

class Repetitions {
    array<RepSide, Color> v_;

public:
    constexpr void clear() {
        for (auto& repSide : v_) {
            repSide.clear();
        }
    }

    void push(Color color, Z z) {
        return v_[color].push(z);
    }

    void normalize(Color) {
        for (auto& repSide : v_) {
            repSide.normalize();
        }
    }

    bool has(Color color, Z z) const {
        return v_[color].has(z);
    }
};

#endif
