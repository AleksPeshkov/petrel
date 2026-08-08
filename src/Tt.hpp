#ifndef TT_HPP
#define TT_HPP

#include <atomic>
#include "System.hpp"
#include "Index.hpp"
#include "Score.hpp"

// Valid age is [1, 2, 3]
class TtAge {
public:
    using _t = unsigned;

    static constexpr int bit_width() { return 2; }
    static constexpr _t mask() { return singleton(bit_width()) - 1u; }

    constexpr TtAge () : v_{1} {}
    constexpr void nextAge() { v_ = next().v_; }

    constexpr bool none() const { return v_ == 0; }
    constexpr bool any() const { return !none(); }

    constexpr bool is(TtAge age) const { return v_ == age.v_; }
    constexpr bool isOld(TtAge age) const { return !is(age) && !is(age.next()); }

    template <typename P, typename S>
    constexpr P pack(S shift) { return ::pack<P>(v_, shift); }

    template <typename T, typename S>
    static constexpr TtAge unpack(T packed, S shift) { return TtAge{::unpack(packed, shift, mask())}; }

private:
    _t v_;

    constexpr explicit TtAge (_t v) : v_{v} { assert (v <= mask()); }
    constexpr TtAge next() const { return v_ == mask() ? TtAge{} : TtAge{v_ + 1}; }
};

class Tt {
    void* memory = nullptr;
    size_t size_ = 0;
    TtAge age;

    void free() {
        if (size_) {
            System::freeAligned(memory);
            memory = nullptr;
            size_ = 0;
        }
    }

    void zeroFill() {
        std::memset(memory, 0, size_);
    }

    void allocate(size_t _bytes) {
        const auto minBytes = minSize();
        auto bytes = ::bit_floor(std::max(_bytes, minBytes));

        if (bytes != size_) {
            free();

            for (; bytes >= minBytes; bytes >>= 1) {
                auto ptr = System::allocateAligned(bytes, minBytes);

                if (ptr) {
                    memory = ptr;
                    size_ = bytes;
                    break;
                }
            }
        }

        assert (bytes == size_);
        zeroFill();
    }

    template <size_t Align>
    constexpr uintptr_t mask() const {
        static_assert (isSingleton(Align));
        return (size_-1) ^ (Align-1);
    }

    Tt (const Tt&) = delete;
    Tt& operator= (const Tt&) = delete;
public:
    mutable node_count_t hits = 0;
    mutable node_count_t reads = 0;
    mutable node_count_t writes = 0;

    Tt(size_t n = minSize()) { setSize(n); }
    ~Tt() { free(); }

    constexpr size_t size() const { return size_; }

    // 2MB to trigger linux huge page support if possible
    static constexpr size_t minSize() { return 1024 /*2 * 1024 * 1024*/; }

    // all currently available memory
    static size_t maxSize() { return ::bit_floor(System::getAvailableMemory()); }

    void setSize(size_t bytes) { allocate(bytes); }
    void newGame() { zeroFill(); age = {}; }
    void newSearch() { reads = 0; writes = 0; hits = 0; }

    template <typename P, typename S>
    constexpr P packAge(S shift) { return age.pack<P>(shift); }

    constexpr void nextAge() { age.nextAge(); }
    constexpr bool isAge(TtAge a) const { return age.is(a); }
    constexpr bool isOld(TtAge a) const { return age.isOld(a); }

    template <size_t Align>
    constexpr void* addr(Z z) const {
        return static_cast<void*>( static_cast<char*>(memory) + (z & mask<Align>()) );
    }

    template <typename T>
    constexpr T* addr(Z z) const {
        return static_cast<T*>( addr<sizeof(T)>(z) );
    }

    void prefetch(void* ptr) const {
        __builtin_prefetch(ptr);
    }

    template <size_t Align>
    void* prefetch(Z z) const {
        auto ptr = addr<Align>(z);
        prefetch(ptr);
        return ptr;
    }

    template <typename T>
    T* prefetch(Z z) const {
        return static_cast<T*>( prefetch<sizeof(T)>(z) );
    }

};
extern Tt The_transpositionTable;

// 8 byte
class TtEntry {
    enum {
        ShiftEval  = 0,
        ShiftScore = ShiftEval  + Score::bit_width(),
        ShiftBound = ShiftScore + Score::bit_width(),
        ShiftAge   = ShiftBound + Bound::bit_width(),
        ShiftDraft = ShiftAge   + TtAge::bit_width(),
        ShiftMove  = ShiftDraft + Ply::bit_width(),
        ShiftZ     = ShiftMove  + TtMove::bit_width(), // total size of all data fields
    };

    using _t = u64_t;

#ifndef NDEBUG
    union {
        _t v_;
        struct PACKED {
            Score::_t eval_  :Score::bit_width();
            Score::_t score_ :Score::bit_width();
            Bound::_t bound_ :Bound::bit_width();
            TtAge::_t age_   :TtAge::bit_width();
            Ply::_t   draft_ :Ply::bit_width();
            unsigned  zmove_ :TtMove::bit_width();
            Z::_t z_ : (64 - ShiftZ); // remaining bits
        } u;
    };
    static_assert (sizeof(u) == sizeof(v_));
#else
    _t v_;
#endif

    static constexpr _t ZMask{ U64(0xffff'ffff'ffff'ffff) << ShiftZ };
    static constexpr _t MoveZMask{ U64(0xffff'ffff'ffff'ffff) << ShiftMove };

public:
    constexpr TtEntry () : v_{0} {}

    constexpr TtEntry (Z z,
        Score _eval,
        Score _score,
        Bound _bound,
        Ply _draft,
        TtMove _ttMove
    ) : v_{
        (((static_cast<_t>(+_ttMove) << ShiftMove) ^ +z) & MoveZMask)
        | _eval.pack<_t>(ShiftEval)
        | _score.pack<_t>(ShiftScore)
        | _bound.pack<_t>(ShiftBound)
        | _draft.pack<_t>(ShiftDraft)
        | The_transpositionTable.packAge<_t>(ShiftAge)
    } {
        static_assert (sizeof(TtEntry) == sizeof(u64_t));

        assert (score() == _score);
        assert (bound().is(_bound));
        assert (draft() == _draft);
        assert (The_transpositionTable.isAge(age()));
        assert (ttMove(z) == _ttMove);
    }

    constexpr bool none() const { return v_ == 0; }
    constexpr bool any() const { return !none(); }
    constexpr bool operator == (Z z) const { return (v_ & ZMask) == (z & ZMask); }

    constexpr Score eval() const { return Score::unpack(v_, ShiftEval); }
    constexpr Score score() const { return Score::unpack(v_, ShiftScore); }
    constexpr Bound bound() const { return Bound::unpack(v_, ShiftBound); }
    constexpr TtAge age() const { return TtAge::unpack(v_, ShiftAge); }
    constexpr Ply draft() const { return Ply::unpack(v_, ShiftDraft); }
    constexpr TtMove ttMove(Z z) const { return TtMove::unpack(v_ ^ +z, ShiftMove); }

    constexpr TtEntry& setAge() {
        v_ ^= age().pack<_t>(ShiftAge); // clear previous
        v_ |= The_transpositionTable.packAge<_t>(ShiftAge); // set new value
        return *this;
    }

    static TtEntry read(TtEntry* tt) {
        ++The_transpositionTable.reads;
        return std::bit_cast<TtEntry>(std::bit_cast<std::atomic<u64_t>*>(tt)->load(std::memory_order_relaxed));
    }

    TtEntry& write(TtEntry* tt) const {
        std::bit_cast<std::atomic<u64_t>*>(tt)->store(this->v_, std::memory_order_relaxed);
        ++The_transpositionTable.writes;
        return const_cast<TtEntry&>(*this);
    }
};

#endif
