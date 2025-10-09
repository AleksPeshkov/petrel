#ifndef REPETITIONS_HPP
#define REPETITIONS_HPP

#include "typedefs.hpp"
#include "Zobrist.hpp"

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
