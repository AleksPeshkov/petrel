#ifndef HISTORY_HPP
#define HISTORY_HPP

#include "Index.hpp"
#include "Zobrist.hpp"

template<int _Size>
class CACHE_ALIGN HistoryMoves {
public:
    static constexpr int Size = _Size;
    using Index = ::Index<Size>;
    using Slot = Index:: template arrayOf<HistoryMove>;

private:
    using _t = Color::arrayOf<HistoryMove::Index::arrayOf<Slot>>;
    _t v;

public:
    void clear() {
        std::memset(&v, 0, sizeof(v)); //TRICK: Move{} == int16_t{0}
    }

    constexpr const HistoryMove& get(int i, Color color, HistoryMove slot) const {
        return v[color][slot][Index{i}];
    }

    constexpr void set(Color color, HistoryMove slot, HistoryMove historyMove) {
        insert_unique(v[color][slot], historyMove);
    }
};

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
    static_assert(Pos < Size);

    auto begin = arr.begin();
    auto pos = begin + Pos;
    auto end = arr.end();

    // find if existing before Pos
    if (std::find(arr.begin(), pos, value) != pos) {
        return; // already present → do nothing
    }

    // find if existing after Pos
    auto found = std::find(pos, end, value);

    if (found != end) {
        std::rotate(pos, found, found + 1); // moves *found to *pos
    } else {
        // No match: shift all down, insert at *pos
        std::copy_backward(pos, end - 1, end);
        *pos = value;
    }
}

// triangular array
class CACHE_ALIGN PvMoves {
    static constexpr auto triangularArraySize = (Ply::Last+1) * (Ply::Last+2) / 2;
public:
    using Index = ::Index<triangularArraySize>;
    Index::arrayOf<UciMove> pv;

public:
    PvMoves () { clear(); }

    void clear() { std::memset(&pv, 0, sizeof(pv)); }

    void clearPly(Index i) { pv[i] = UciMove{}; }

    Index set(Index parent, UciMove move, Index child) {
        pv[parent++] = move;
        assert (parent <= child);
        while ((pv[parent++] = pv[child++]));
        pv[parent] = UciMove{};
        return parent; // new child index
    }

    operator const UciMove* () const { return &pv[0]; }
};

// https://www.talkchess.com/forum/viewtopic.php?p=554664#p554664
class RepetitionHash {
    using _t = u64_t;
    _t v{0};

    static constexpr _t hash(Z z) { return ::singleton<_t>(z & 077); }
public:
    constexpr RepetitionHash () : v{0} {}
    constexpr RepetitionHash (const RepetitionHash& m, Z z) : v{m.v | hash(z)} {}
    constexpr bool has(Z z) const { return (v & hash(z)) != 0; }
};

class RepetitionsSide {
    using RepIndex = Index<50>;

    struct RepEntry {
        Z z;
        RepetitionHash hash;
    };

    constexpr static RepIndex Last{RepIndex::Last};

    RepIndex::arrayOf<RepEntry> reps;
    int count = 0; // number of entries
    RepIndex last{RepIndex::Last}; // last added entry index

    static constexpr int prev(int i) { return i > 0 ? i-1 : Last; }

public:
    constexpr RepetitionsSide () { reps[last].hash = {}; }

    constexpr void clear() {
        last = Last;
        count = 0;
    }

    constexpr void push(Z z) {
        auto prevHash{(count == 0) ? RepetitionHash{} : RepetitionHash{reps[last].hash, reps[last].z}};
        last = RepIndex{last < Last ? last+1 : 0};
        reps[last].z = z;
        reps[last].hash = prevHash;
        count = count < RepIndex::Size ? count+1 : count;
    }

    constexpr void dropLast() {
        if (count == 0) { assert (false); return; }
        last  = last > 0 ? RepIndex{last-1} : Last;
        count = count-1;
    }

    void normalize() {
        Index<25>::arrayOf<Z> duplicates;
        int duplicateCount = 0;

        // find duplicates
        for (int c = 0, i = last; c < count; ++c, i = prev(i)) {
            auto z = reps[i].z;

            for (int d = 0; d < duplicateCount; ++d) {
                // already found
                if (duplicates[d] == z) { goto NEXT; };
            }

            // not found
            {
                // i and i-1 cannot be repetitions
                int j = prev(i); j = prev(j);
                for (int cc = c+2; cc < count; ++cc) {
                    if (z == reps[j].z) {
                        // found
                        duplicates[duplicateCount++] = z;
                        break;
                    }

                    if (!reps[j].hash.has(z)) {
                        // not found
                        break;
                    }

                    j = prev(j);
                }
            }

        NEXT:
            continue;
        }

        RepetitionHash hash{};

        // push back in reversed order, creating correct hash
        for (int d = duplicateCount-1; d >= 0; --d) {
            auto z = duplicates[d];
            reps[d].z = z;
            reps[d].hash = hash;
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

    constexpr RepetitionHash repetitionHash() const {
        return (count == 0) ?  RepetitionHash{} : RepetitionHash{reps[0].hash, reps[0].z};
    }

    // used for unit testing
    constexpr auto size() const { return count; }
};

class Repetitions {
    Color::arrayOf<RepetitionsSide> v;

public:
    void clear() {
        for (auto c : Color::range()) {
            v[c].clear();
        }
    }

    void push(Color color, Z z) {
        return v[color].push(z);
    }

    void normalize(Color color) {
        // the very last position is search root and should be removed from history
        v[color].dropLast();
        for (auto c : Color::range()) {
            v[c].normalize();
        }
    }

    bool has(Color color, Z z) const {
        return v[color].has(z);
    }

    RepetitionHash repetitionHash(Color color) const {
        return v[color].repetitionHash();
    }
};

#endif
