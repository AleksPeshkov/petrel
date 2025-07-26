#ifndef REPETITION_HPP
#define REPETITION_HPP

#include "typedefs.hpp"
#include "Zobrist.hpp"

typedef const Z& Zr;

class RepetitionMask {
    typedef u64_t _t;
    _t v{0};

    static constexpr _t mask(Zr z) { return ::singleton<u64_t>(z & 077); }
public:
    constexpr bool has(Zr z) const { return (v & mask(z)) != 0; }
    constexpr RepetitionMask& add(Zr z) { v |= mask(z); return *this;}
    constexpr RepetitionMask& clear() { v = {0}; return *this; }
    constexpr RepetitionMask& update(const Rule50& rule50, Zr z) {
        return rule50.isEmpty() ? clear() : add(z);
    }
};

class RepetitionHistory {
    class RepRootSide {
        typedef Index<50> RepIndex;

        struct RepEntry {
            Z z;
            RepetitionMask mask;
        };

        RepIndex::arrayOf<RepEntry> reps;
        index_t count = 0; // number of entries
        RepIndex last = RepIndex::Last; // last added entry index

    public:
        constexpr RepRootSide () { reps[last].mask.clear(); }

        constexpr void clear() {
            count = 0;
            last = RepIndex::Last;
            reps[last].mask.clear();
        }

        void push(Zr z) {
            auto mask = reps[last].mask;
            last  = last < RepIndex::Last ? last+1 : 0;
            count = count < RepIndex::Size ? count+1 : count;
            reps[last].z = z;
            reps[last].mask = mask.add(z);
        }

        void normalize() {
            if (count <= 1) { return; }

            if (count != RepIndex::Size) {
                // just reverse order
                for (auto i = 0u; i < count/2; ++i) {
                    std::swap(reps[i], reps[last - i]);
                }
                return;
            }

            // rare buffer overflow case

            RepIndex::arrayOf<Z> temp;
            auto current = last < RepIndex::Last ? last+1 : 0;

            // copy z elements to the beginning
            for (auto i = 0u; i < count; ++i) {
                temp[i] = reps[current].z;
                current = current < RepIndex::Last ? current+1 : 0;
            }

            RepetitionMask mask;

            // push again in reverse order
            for (auto i = 0u; i < count; ++i) {
                auto z = temp[i];
                reps[count - i - 1].z = z;
                reps[count - i - 1].mask = mask.add(z);
            }

            count = 0;
            last = RepIndex::Last;
        }

        bool has(Zr z) const {
            for (RepIndex i = 0; reps[i].mask.has(z); ++i) {
                if (z == reps[i].z) {
                    return true;
                }
            }
            return false;
        }
    };

    Color::arrayOf<RepRootSide> v;

public:
    void clear() {
        FOR_EACH(Color, c) {
            v[c].clear();
        }
    }

    void normalize() {
        FOR_EACH(Color, c) {
            v[c].normalize();
        }
    }

    bool has(Color c, Zr z) const {
        return v[c].has(z);
    }

    void push(Color c, Zr z) {
        return v[c].push(z);
    }

};

#endif
