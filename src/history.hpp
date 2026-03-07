#ifndef HISTORY_HPP
#define HISTORY_HPP

#include "Index.hpp"

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
    static_assert(Pos < Size, "Pos must be less than container size");

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
    using Slot = Index:: template arrayOf<HistoryMove>;

private:
    using _t = Color::arrayOf<HistoryMove::HistoryIndex::arrayOf<Slot>>;
    _t v_;

public:
    void clear() {
        std::memset(&v_, HistoryMove::None, sizeof(v_));
    }

    constexpr const HistoryMove& get(Index i, Color color, HistoryMove slot) const {
        return v_[color][slot][i];
    }

    constexpr void set(Color color, HistoryMove slot, HistoryMove historyMove) {
        insert_unique(v_[color][slot], historyMove);
    }
};

// triangular array
class CACHE_ALIGN PvMoves {
    static constexpr auto triangularArraySize = (Ply::Last+1) * (Ply::Last+2) / 2;
public:
    struct Index; STRUCT_INDEX (Index, triangularArraySize);
    Index::arrayOf<UciMove> pv;

public:
    PvMoves () { clear(); }

    void clear() { std::memset(&pv, 0, sizeof(pv)); }

    void clearPly(Index i) { pv[i] = UciMove{}; }

    Index set(Index parent, UciMove move, Index child) {
        pv[parent] = move;
        assert (parent < child);

        auto from = child.v();
        auto to = parent.v() + 1;
        while ((pv[Index{to++}] = pv[Index{from++}]).any()) {}

        pv[Index{to}] = UciMove{};
        return Index{to}; // new child index
    }

    operator const UciMove* () const { return &pv[Index{0}]; }
};

// https://www.talkchess.com/forum/viewtopic.php?p=554664#p554664
class RepHash {
    using _t = u64_t;
    _t v{0};

    static constexpr _t hash(Z z) { return ::singleton<_t>(z.v() & 077); }
public:
    constexpr RepHash () : v{0} {}
    constexpr RepHash (const RepHash& m, Z z) : v{m.v | hash(z)} {}
    constexpr bool has(Z z) const { return (v & hash(z)) != 0; }
};

class RepSide {
    struct RepIndex; STRUCT_INDEX (RepIndex, 50);

    struct RepEntry {
        Z z;
        RepHash hash;
    };

    constexpr static RepIndex Last{RepIndex::Last};

    RepIndex::arrayOf<RepEntry> reps;
    int count = 0; // number of entries
    RepIndex last{RepIndex::Last}; // last added entry index

    static constexpr int prev(int i) { return i > 0 ? i-1 : Last.v(); }

public:
    constexpr RepSide () { reps[last].hash = {}; }

    constexpr void clear() {
        last = Last;
        count = 0;
    }

    constexpr void push(Z z) {
        auto prevHash{(count == 0) ? RepHash{} : RepHash{reps[last].hash, reps[last].z}};
        last = RepIndex{last < Last ? last.v()+1 : 0};
        reps[last].z = z;
        reps[last].hash = prevHash;
        count = count < RepIndex::Size ? count+1 : count;
    }

    constexpr void dropLast() {
        if (count == 0) { assert (false); return; }
        last  = last.v() > 0 ? RepIndex{last.v()-1} : Last;
        count = count-1;
    }

    void normalize() {
        std::array<Z, 25> duplicates;
        int duplicateCount = 0;

        // find duplicates
        for (int c = 0, i = last.v(); c < count; ++c, i = prev(i)) {
            auto z = reps[RepIndex{i}].z;

            for (int d = 0; d < duplicateCount; ++d) {
                // already found
                if (duplicates[d] == z) { goto NEXT; };
            }

            // not found
            {
                // i and i-1 cannot be repetitions
                int j = prev(i); j = prev(j);
                for (int cc = c+2; cc < count; ++cc) {
                    if (z == reps[RepIndex{j}].z) {
                        // found
                        duplicates[duplicateCount++] = z;
                        break;
                    }

                    if (!reps[RepIndex{j}].hash.has(z)) {
                        // not found
                        break;
                    }

                    j = prev(j);
                }
            }

        NEXT:
            continue;
        }

        RepHash hash{};

        // push back in reversed order, creating correct hash
        for (int d = duplicateCount-1; d >= 0; --d) {
            auto z = duplicates[d];
            reps[RepIndex{d}].z = z;
            reps[RepIndex{d}].hash = hash;
            hash = {hash, z};
        }

        count = duplicateCount;
        last = Last; // new push() should be from the very beginning
    }

    constexpr bool has(Z z) const {
        if (count == 0) { return false; }

        RepIndex i{0};
        while (true) {
            if (z == reps[i].z) {
                return true;
            }
            if (!reps[i].hash.has(z)) {
                return false;
            }
            ++i;
        }
    }

    constexpr RepHash repHash() const {
        return (count == 0) ?  RepHash{} : RepHash{reps[RepIndex{0}].hash, reps[RepIndex{0}].z};
    }

    // used for unit testing
    constexpr auto size() const { return count; }
};

class Repetitions {
    Color::arrayOf<RepSide> v_;

public:
    void clear() {
        for (auto& repSide : v_) {
            repSide.clear();
        }
    }

    void push(Color color, Z z) {
        return v_[color].push(z);
    }

    void normalize(Color color) {
        // the very last position is search root and should be removed from history
        v_[color].dropLast();
        for (auto& repSide : v_) {
            repSide.normalize();
        }
    }

    bool has(Color color, Z z) const {
        return v_[color].has(z);
    }

    RepHash repHash(Color color) const {
        return v_[color].repHash();
    }
};

#endif
