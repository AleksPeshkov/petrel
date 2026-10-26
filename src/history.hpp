#ifndef HISTORY_HPP
#define HISTORY_HPP

#include "Index.hpp"
#include "Score.hpp"

/**
 * Inserts unique non empty `value` to position `Pos` in the array.
 *
 * Behavior:
 * - Insert at `Pos`, shifting elements in [Pos, gap) right by one,
 *   where `gap` is the first empty slot (`{}`) in [Pos, Size-1). If no gap exists,
 *   shifts [Pos, Size-1) right, evicting the last element.
 * - If `value` is already present at [0, `Pos`]: do nothing (already prioritized).
 * - If `value` is already present at index > `Pos`: remove it before inserting again at correct `Pos`
 *
 * @tparam Pos Target position (must be < Size)
 * @tparam value_type Element type (must support `operator==` and contextually convertible to `bool`)
 * @tparam Size   Array size
 * @param arr  Array to update
 * @param value  Value to insert
 */
template <size_t Pos = 0, typename value_type, size_t Size>
constexpr void insert_unique_pos(std::array<value_type, Size>& arr, value_type value) {
    static_assert(Pos < Size, "Insert position must be within bounds");
    assert(value);

    const auto begin = arr.begin();
    const auto end = arr.end();
    const auto pos = begin + Pos;

    auto found = std::find_if(begin, end, [&](value_type v) { return v == value; });
    if (found != end) {
        if (found < pos + 1) { return; } // already present at correct position
        *found = value_type{}; // remove duplication at wrong position
    }

    if (pos != end - 1) {
        // find nearest right empty slot in [pos, end-1) — this also marks start of "gap"
        auto gap = std::find_if(pos, end - 1, [](value_type v) { return !static_cast<bool>(v); });
        if (pos != gap) {
            // preserve more valuable left entries by filling empty gaps or by evicting last entry
            std::copy_backward(pos, gap, gap + 1);
        }
    }

    *pos = value;
}

/**
 * Inserts non empty `value` to an early position in the array, preserving uniqueness and compactness.
 * - Inserts at `Pos` or the first empty slot in [0, Pos).
 * - If `value` is already present in [0, Pos): no action (already prioritized).
 * - if `value` is found elsewhere: removes it (marks as empty) so it can be reinserted early.
 * - If no empty slot exists in [0, Pos]: inserts at `Pos`, shifting [Pos, end-1] right (last element dropped).
 *
 * This ensures:
 * - No duplicates,
 * - No empty gaps from the beginning,
 *
 * @tparam Pos  Maximum target index for insertion (must be < Size)
 * @tparam value_type Element type (must support `operator==` and be contextually convertible to `bool`)
 * @tparam Size Array size
 * @param arr   Array to update
 * @param value Value to insert
 */
template <size_t Pos = 0, typename value_type, size_t Size>
constexpr void insert_unique_compact(std::array<value_type, Size>& arr, value_type value) {
    static_assert(Pos < Size, "Pos must be less than container size");
    assert(value); // value must be valid

    const auto begin = arr.begin();
    const auto end = arr.end();
    const auto pos = begin + Pos;

    auto found = std::find_if(begin, end, [&](value_type v) { return v == value; });
    if (found != end) {
        if (found <= pos) { return; } // already present at correct position
        *found = value_type{}; // remove duplication at wrong position
    }

    // find first empty slot from the begining till last by one element
    auto gap = std::find_if(begin, end - 1, [](value_type v) { return !static_cast<bool>(v); });
    if (gap <= pos) {
        *gap = value;
        return;
    }

    // preserve more valuable left entries till the first gap or by evicting last entry
    std::copy_backward(pos, gap, gap + 1);
    *pos = value;
}


enum deep_node_enum { ShallowNode, DeepNode };
struct DeepNodeIndex : Index<DeepNodeIndex, 2, deep_node_enum> {
    constexpr DeepNodeIndex (_t i) : Index{i} {}
    explicit operator bool () const { return v_ == DeepNode; }
};

enum continuation_move_enum { Counter, Followup };
struct ContIndex : Index<ContIndex, 2, continuation_move_enum> {
    constexpr ContIndex (_t i) : Index{i} {}
};

// Continuation Move table, counter and followup moves together (for cache locality)
template<int _Size>
class CACHE_ALIGN ContMoves {
public:
    static constexpr int Size = _Size;
    struct BucketIndex : Index<BucketIndex, Size> { explicit constexpr BucketIndex (int v) : Index<BucketIndex, Size>{v} {} };

private:
    using _t = array<Move, Color, MoveType, Square, Square, ContIndex, DeepNodeIndex, BucketIndex>;
    _t v_;

public:
    constexpr Move get(DeepNodeIndex deepNode, ContIndex ci, BucketIndex n, Color color, Move move) const {
        assert (move.isAny());
        return v_[color][move.moveType()][move.from()][move.to()][ci][deepNode][n];
    }

    template <size_t Pos = 0>
    constexpr void set(DeepNodeIndex deepNode, ContIndex ci, Color color, Move move, Move bestMove) {
        assert (move.isAny()); assert (bestMove.isAny());
        ::insert_unique_compact<Pos>(v_[color][move.moveType()][move.from()][move.to()][ci][ShallowNode], bestMove);
        if (deepNode) {
            ::insert_unique_compact<Pos>(v_[color][move.moveType()][move.from()][move.to()][ci][DeepNode], bestMove);
        }
    }
};

class CACHE_ALIGN PrincipalVariation {
    // final PV size + triangular arrays of subPVs (including final null moves)
    static constexpr auto triangularArraySize = Ply::size() + Ply::size()*(Ply::size()+1) / 2;
public:
    struct PvIndex : ::Index<PvIndex, triangularArraySize> { constexpr PvIndex (_t v) : Index{v} {} };

private:
    array<Move, PvIndex> pv_;

    Ply depth_{0}; // latest PV iteration depth
    Score score_{}; // latest PV score

public:
    constexpr PrincipalVariation () {
        pv_[{0}] = {};
        pv_[{1}] = {};
        depth_ = 0_ply;
        score_ = {};
    }

    // clear child PV space: pv_[i] = {}
    void clear(PvIndex i) { pv_[i] = {}; }

    /// set parent PV move and copy child PV
    /// @return Index of space past PV
    PvIndex set(PvIndex parent, Move childMove, PvIndex childPv) {
        auto from = childPv;
        auto to   = parent;

        pv_[to++] = childMove;
        if (from == to) {
            // no need to copy, but still needs to find move list end
            while ((pv_[to++]).isAny()) {}
        } else {
            assert (from > to);
            // copies null move terminated move list (including last null)
            while ((pv_[to++] = pv_[from++]).isAny()) {}
        }

        return to; // new childPv
    }

    /// set root PV depth, score and PV moves
    /// @return Index of space past PV
    PvIndex set(Ply depth, Score score, Move bestRootMove, PvIndex childPv) {
        depth_ = depth;
        score_ = score;
        return set({0}, bestRootMove, childPv);
    }

    void set(Move bestRootMove, Score score = {}) {
        pv_[{0}] = bestRootMove;
        pv_[{1}] = {};
        depth_ = 0_ply;
        score_ = score;
    }

    void set(Ply depth) { depth_ = depth; }

    const auto* moves() const { return &pv_[{0}]; }
    auto getMove(Ply ply) const { return pv_[{+ply}]; }
    auto depth() const { return depth_; }
    auto score() const { return score_; }
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

    struct HisIndex : Index<HisIndex, 50> { constexpr HisIndex(_t v) : Index{v} {} };
    array<_t, HisIndex> his; // game history with duplicates removed to avoid redundant checks for 2-fold repetition
    ZHash hisZHash_{};
    int hisCount_ = 0; // number of history without duplicates ((only for unit tests)

    struct DupIndex : Index<DupIndex, 25> { constexpr DupIndex(_t v) : Index{v} {} };
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
    constexpr RepSide () {
        hisZHash_ = {};
        hisCount_ = 0;
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
        hisZHash_ = {};
        hisCount_ = 0;
        dupZHash_ = {};
        dupCount_ = 0;
        if (ringCount_ == 0) { return; }

        int hisFound = 0;
        int dupFound = 0;
        {
            ZHash hisZHash{};
            ZHash dupZHash{};
            for (int i = 0; i < ringCount_; ++i) {
                Z z = ring[last_ - i].z;

                // test if already present in duplicant array
                if (!dupZHash.isNone(z)) {
                    bool already = false;
                    for (int d = dupFound-1; true; --d) {
                        if (dup[{d}].z == z) { already = true; break; }
                        if (dup[{d}].zHash.isNone(z)) { break; }
                        assert (d > 0);
                    }
                    if (already) { continue; }
                }

                // test for repetition (i and i+1 cannot be chess repetitions)
                for (int j = i + 2; j < ringCount_; ++j) {
                    if (ring[last_ - j].z == z) {
                        // append to duplicant array
                        dup[{dupFound}].z = z;
                        dup[{dupFound}].zHash = dupZHash;
                        dupZHash = ZHash{dupZHash, z};
                        ++dupFound;
                        break;
                    }
                    if (ring[last_ - j].zHash.isNone(z)) { break; }
                }

                if (!hisZHash.isNone(z)) {
                    bool already = false;
                    for (int h = hisFound-1; true; --h) {
                        if (his[{h}].z == z) { already = true; break; }
                        if (his[{h}].zHash.isNone(z)) { break; }
                        assert (h > 0);
                    }
                    if (already) { continue; }
                }

                // append to history array
                his[{hisFound}].z = z;
                his[{hisFound}].zHash = hisZHash;
                hisZHash = ZHash{hisZHash, z};
                ++hisFound;
            }
        }

        // rehash in opposite direction
        ZHash hisZHash{};
        for (int h = hisFound-1; h >= 0; --h) {
            his[{h}].zHash = hisZHash;
            hisZHash = ZHash{hisZHash, his[{h}].z};
        }
        hisZHash_ = hisZHash;
        hisCount_ = hisFound;

        // rehash in opposite direction
        ZHash dupZHash{};
        for (int d = dupFound-1; d >= 0; --d) {
            dup[{d}].zHash = dupZHash;
            dupZHash = ZHash{dupZHash, dup[{d}].z};
        }

        dupZHash_ = dupZHash;
        dupCount_ = dupFound;
    }

    // 2-fold repetition check
    bool has2(Z z) const {
        if (hisZHash_.isNone(z)) { return false; }

        // search for repetition in non-duplicate positions
        for (int h = 0; true; ++h) {
            if (his[{h}].z == z) { return true; }
            if (his[{h}].zHash.isNone(z)) { return false; }
            assert (h < hisCount_);
        }
    }

    // 3-fold repetition check
    bool has3(Z z) const {
        if (dupZHash_.isNone(z)) { return false; }

        // search for repetition in duplicate positions
        for (int d = 0; true; ++d) {
            if (dup[{d}].z == z) { return true; }
            if (dup[{d}].zHash.isNone(z)) { return false; }
            assert (d < dupCount_);
        }
    }
};

class Repetitions {
    array<RepSide, Color> v_;

public:
    void push(Color color, Z z) {
        return v_[color].push(z);
    }

    void normalize(Color) {
        for (auto& repSide : v_) {
            repSide.normalize();
        }
    }

    // 2-fold repetition check
    bool has2(Color color, Z z) const {
        return v_[color].has2(z);
    }

    // 3-fold repetition check
    bool has3(Color color, Z z) const {
        return v_[color].has3(z);
    }
};

#endif
