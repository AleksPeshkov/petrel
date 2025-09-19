#ifndef REPETITIONS_HPP
#define REPETITIONS_HPP

#include "typedefs.hpp"
#include "Zobrist.hpp"

typedef const Z& Zr;

class RepetitionMask {
    typedef u64_t _t;
    _t v{0};

    static constexpr _t mask(Zr z) { return ::singleton<u64_t>(z & 077); }
public:
    constexpr RepetitionMask () : v{0} {}
    constexpr RepetitionMask (const RepetitionMask& m, Zr z) : v{m.v | mask(z)} {}
    constexpr bool has(Zr z) const { return (v & mask(z)) != 0; }
};

class Repetitions {
    class RepRootSide {
        typedef Index<50> RepIndex;

        struct RepEntry {
            Z z;
            RepetitionMask mask;
        };

        RepIndex::arrayOf<RepEntry> reps;
        int count = 0; // number of entries
        RepIndex last = RepIndex::Last; // last added entry index

    public:
        constexpr RepRootSide () { reps[last].mask = {}; }

        constexpr void clear() {
            count = 0;
            last = RepIndex::Last;
            reps[last].mask = {};
        }

        constexpr void dropLast() {
            if (count == 0) { assert (false); return; }
            count = count-1;
            last  = last > 0 ? RepIndex{last-1} : RepIndex::Last;
        }

        void push(Zr z) {
            RepetitionMask mask{reps[last].mask, reps[last].z};
            last  = last < RepIndex::Last ? last+1 : 0;
            count = count < RepIndex::Size ? count+1 : count;
            reps[last].z = z;
            reps[last].mask = (count == 1) ? RepetitionMask{} :  mask;
        }

        void normalize() {
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
                current = current < RepIndex::Last ? current+1 : 0;
            }

            RepetitionMask mask{};

            // push again in reverse order
            for (int i = 0; i < count; ++i) {
                auto z = temp[i];
                mask = {mask, z};
                reps[count - i - 1].z = z;
                reps[count - i - 1].mask = mask;
            }

            count = 0;
            last = RepIndex::Last;
        }

        bool has(Zr z) const {
            RepIndex i = 0;
            while (true) {
                if (z == reps[i].z) {
                    return true;
                }
                if (!reps[i].mask.has(z)) {
                    return false;
                }
                ++i;
            }
            return false;
        }

        RepetitionMask repMask() const {
            if (count == 0) { return RepetitionMask{}; }
            return RepetitionMask{reps[0].mask, reps[0].z};
        }
    };

    Color::arrayOf<RepRootSide> v;

public:
    void push(Color c, Zr z) {
        return v[c].push(z);
    }

    void clear() {
        FOR_EACH(Color, color) {
            v[color].clear();
        }
    }

    void normalize(Color c) {
        // the very last position is root and should be removed from history
        v[c].dropLast();
        FOR_EACH(Color, color) {
            v[color].normalize();
        }
    }

    bool has(Color c, Zr z) const {
        return v[c].has(z);
    }

    RepetitionMask repMask(Color c) const {
        return v[c].repMask();
    }
};

#endif
