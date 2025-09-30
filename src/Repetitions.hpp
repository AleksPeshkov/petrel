#ifndef REPETITIONS_HPP
#define REPETITIONS_HPP

#include "typedefs.hpp"
#include "Zobrist.hpp"

// https://www.talkchess.com/forum/viewtopic.php?p=554664#p554664
class RepetitionHash {
    using _t = u64_t;
    _t v{0};

    static constexpr _t hash(ZArg z) { return ::singleton<_t>(z & 077); }
public:
    constexpr RepetitionHash () : v{0} {}
    constexpr RepetitionHash (const RepetitionHash& m, ZArg z) : v{m.v | hash(z)} {}
    constexpr bool has(ZArg z) const { return (v & hash(z)) != 0; }
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

public:
    constexpr RepetitionsSide () { reps[last].hash = {}; }

    constexpr void clear() {
        last = Last;
        count = 0;
    }

    constexpr void push(ZArg z) {
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

    constexpr void normalize() {
        if (count <= 1) { return; }

        if (count != RepIndex::Size) {
            // just reverse order
            for (int i = 0; i < count/2; ++i) {
                std::swap(reps[i], reps[last - i]);
            }
            return;
        }

        // rare buffer overflow case

        RepIndex::arrayOf<Z> temp;
        auto current = last < RepIndex::Last ? last+1 : 0;

        // copy z elements to the beginning
        for (int i = 0; i < count; ++i) {
            temp[i] = reps[current].z;
            current = current < Last ? current+1 : 0;
        }

        RepetitionHash hash{};

        // push again in reverse order
        for (int i = 0; i < count; ++i) {
            auto z = temp[i];
            hash = {hash, z};
            reps[count - i - 1].z = z;
            reps[count - i - 1].hash = hash;
        }

        count = 0;
        last = Last;
    }

    constexpr bool has(ZArg z) const {
        int repetitions = 0;
        RepIndex i{0};
        while (true) {
            if (z == reps[i].z) {
                ++repetitions;
                if (repetitions == 2) {
                    return true;
                }
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

    void push(Color color, ZArg z) {
        return v[color].push(z);
    }

    void normalize(Color color) {
        // the very last position is search root and should be removed from history
        v[color].dropLast();
        for (auto c : Color::range()) {
            v[c].normalize();
        }
    }

    bool has(Color color, ZArg z) const {
        return v[color].has(z);
    }

    RepetitionHash repetitionHash(Color color) const {
        return v[color].repetitionHash();
    }
};

#endif
