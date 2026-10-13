#ifndef TT_HPP
#define TT_HPP

#include <atomic>
#include "System.hpp"
#include "Index.hpp"
#include "Score.hpp"

class Tt {
public:
    static constexpr size_t minSize() { return 64; }
    static size_t maxSize() { return ::bit_floor(System::getAvailableMemory()); }

    Tt(size_t bytes) { setSize(bytes); }
    ~Tt() { free(); }

    constexpr size_t size() const { return size_; }
    void setSize(size_t bytes) { allocate(bytes); zeroFill(); }
    void newGame() { zeroFill(); }
    void newSearch(bool isDirty) { if (isDirty) { zeroed_ = false; } }

    template <typename T>
    constexpr T* addr(Z z) const {
        return static_cast<T*>( addr<sizeof(T)>(z) );
    }

    template <size_t Align>
    void* prefetch(Z z) const {
        auto ptr = addr<Align>(z);
        __builtin_prefetch(ptr);
        return ptr;
    }

    template <typename T>
    T* prefetch(Z z) const {
        return static_cast<T*>( prefetch<sizeof(T)>(z) );
    }

private:
    void* allocated_{nullptr};
    size_t size_{0};
    bool zeroed_{false};

    template <size_t Align>
    constexpr void* addr(Z z) const {
        static_assert (::isSingleton(Align));
        auto mask = (size_-1) ^ (Align-1);
        return static_cast<void*>( static_cast<char*>(allocated_) + (z & mask) );
    }

    Tt (const Tt&) = delete;
    Tt& operator= (const Tt&) = delete;

    void free() {
        if (size_) {
            System::freeAligned(allocated_);
            allocated_ = nullptr;
            size_ = 0;
        }
    }

    void zeroFill() {
        if (allocated_ && !zeroed_) {
            std::memset(allocated_, 0, size_);
            zeroed_ = true;
        }
    }

    void allocate(size_t bytes) {
        const auto minSize = this->minSize();
        auto size = ::bit_floor(std::max(bytes, minSize));

        if (size == size_) { return; }

        free();
        for (; size >= minSize; size >>= 1) {
            // 2MB align to trigger linux huge page support if possible
            auto allocated = System::allocateAligned(size, std::min<size_t>(size, 2*1024*1024));

            if (allocated) {
                allocated_ = allocated;
                size_ = size;
                zeroed_ = false;
                break;
            }
        }

        assert (size == size_);
    }
};
extern Tt the_tt;

// Valid age is [1, 2, 3]
class TtAge {
public:
    using _t = unsigned;

    static constexpr int bit_width() { return 2; }
    static constexpr _t mask() { return singleton(bit_width()) - 1u; }

    static constexpr _t nextLut() {
        _t lut{};
        for (_t a = 0; a <= mask(); ++a) {
            _t next = a == mask() ? 1 : a + 1;
            lut |= next << a * bit_width();
        }
        return lut;
    }

    static constexpr auto freshLut() {
        using lut_t = std::conditional_t<bit_width() == 3, u64_t, _t>;
        lut_t lut{};

        for (_t entry = 1; entry <= mask(); ++entry) {
            _t next_entry = +TtAge{entry}.next();
            for (_t base = 1; base <= mask(); ++base) {
                // isFresh() is true if entry == base or previous (i.e. entry.next() == base) and both != 0
                if (entry == base || next_entry == base) {
                    _t index = (entry << bit_width()) | base;
                    lut |= (static_cast<decltype(lut)>(1) << index);
                }
            }
        }
        return lut;
    }

public:
    constexpr TtAge () : v_{1} {}
    constexpr TtAge next() const { return TtAge{ (nextLut() >> v_ * bit_width()) & mask() }; }

    constexpr _t operator + () const { return v_; }
    constexpr bool isNone() const { return v_ == 0; }
    constexpr bool isAny() const { return !isNone(); }
    constexpr bool isSame(TtAge age) const { return v_ == age.v_; }

    // isFresh() is true if entry == this or previous (i.e. entry.next() == this) and both != 0
    constexpr bool isFresh(TtAge entry) const {
        _t index{ (entry.v_ << bit_width()) | v_ };
        return (freshLut() >> index) & 1;
    }

    template <typename P, typename S>
    constexpr P pack(S shift) { return ::pack<P>(v_, shift); }

    template <typename T, typename S>
    static constexpr TtAge unpack(T packed, S shift) { return TtAge{::unpack(packed, shift, mask())}; }

private:
    _t v_;
    constexpr explicit TtAge (_t v) : v_{v} { assert (v <= mask()); }
};

class TtMeta {
public:
    std::atomic<node_count_t> reads = 0;
    std::atomic<node_count_t> hits = 0;
    std::atomic<node_count_t> writes = 0;

private:
    TtAge age_;

public:
    void newGame() { age_ = {}; }
    void newSearch() { reads = 0; hits = 0; writes = 0; }

    constexpr TtAge age() const { return age_; }
    constexpr void nextAge() { age_ = age_.next(); }
};
extern TtMeta the_ttMeta;

struct TtRecord;

// 8 byte
class TtEntry {
    enum {
        ShiftDraft = 0,
        ShiftAge   = ShiftDraft + Ply::bit_width(),
        ShiftEval  = ShiftAge   + TtAge::bit_width(),
        ShiftScore = ShiftEval  + Score::bit_width(),
        ShiftBound = ShiftScore + Score::bit_width(),
        ShiftMove  = ShiftBound + Bound::bit_width(),
        ShiftZ     = ShiftMove  + TtMove::bit_width(), // total size of all data fields
    };

    using _t = u64_t;

#ifndef NDEBUG
    union {
        _t v_;
        struct PACKED {
            Ply::_t   draft_ :Ply::bit_width();
            TtAge::_t age_   :TtAge::bit_width();
            Score::_t eval_  :Score::bit_width();
            Score::_t score_ :Score::bit_width();
            Bound::_t bound_ :Bound::bit_width();
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
        Ply ply,
        Bound _bound,
        Ply _draft,
        TtMove _ttMove
    ) : v_{
        (((static_cast<_t>(+_ttMove) << ShiftMove) ^ +z) & MoveZMask)
        | _eval.pack<_t>(ShiftEval)
        | _score.tt(ply).pack<_t>(ShiftScore)
        | _bound.pack<_t>(ShiftBound)
        | _draft.pack<_t>(ShiftDraft)
        | the_ttMeta.age().pack<_t>(ShiftAge)
    } {
        static_assert (sizeof(TtEntry) == sizeof(u64_t));

        assert (score(0_ply) == _score.tt(ply));
        assert (bound().is(_bound));
        assert (draft() == _draft);
        assert (the_ttMeta.age().isSame(age()));
        assert (ttMove(z) == _ttMove);
    }

    constexpr bool isNone() const { return v_ == 0; }
    constexpr bool isAny() const { return !isNone(); }
    constexpr bool operator == (Z z) const { return (v_ & ZMask) == (z & ZMask); }

    constexpr Score eval() const { return Score::unpack(v_, ShiftEval); }
    constexpr Score score(Ply ply) const { return Score::unpack(v_, ShiftScore).fromTt(ply); }
    constexpr Bound bound() const { return Bound::unpack(v_, ShiftBound); }
    constexpr TtAge age() const { return TtAge::unpack(v_, ShiftAge); }
    constexpr Ply draft() const { return Ply::unpack(v_, ShiftDraft); }
    constexpr TtMove ttMove(Z z) const { return TtMove::unpack(v_ ^ +z, ShiftMove); }

    // replacement weight, the higher the more valuable entry
    constexpr int weight() const { return (the_ttMeta.age().isFresh(age()) << Ply::bit_width()) | +draft(); }

    void refresh(TtEntry* p_ttEntry) {
        auto metaAge = the_ttMeta.age();
        if (!metaAge.isSame(age())) {
            v_ ^= age().pack<_t>(ShiftAge); // clear previous
            v_ |= metaAge.pack<_t>(ShiftAge); // set new value
            write(p_ttEntry);
        }
    }

    static TtEntry read(TtEntry* p_ttEntry) {
        ++the_ttMeta.reads;
        return std::bit_cast<TtEntry>(std::bit_cast<std::atomic<_t>*>(p_ttEntry)->load(std::memory_order_relaxed));
    }

    void write(TtEntry* p_ttEntry) const {
        std::bit_cast<std::atomic<_t>*>(p_ttEntry)->store(this->v_, std::memory_order_relaxed);
        ++the_ttMeta.writes;
    }

    static constexpr TtRecord probe(TtEntry* p_ttEntry, Z z);
};

struct TtRecord { TtEntry ttEntry; TtEntry* p_ttEntry; bool ttHit; };
constexpr TtRecord TtEntry::probe(TtEntry* p_ttEntry, Z z) {
    // direct entry
    auto ttEntry = read(p_ttEntry);
    if (ttEntry == z) { return {ttEntry, p_ttEntry, true}; }

    // other entry
    auto p_ttEntry2 = std::bit_cast<TtEntry*>(std::bit_cast<std::uintptr_t>(p_ttEntry) ^ sizeof(TtEntry));
    auto ttEntry2 = read(p_ttEntry2);
    if (ttEntry2 == z) { return {ttEntry2, p_ttEntry2, true}; }

    // overwrite the only non fresh entry or shallower or direct
    //TRICK: zeroed entry is never fresh
    if (ttEntry.weight() <= ttEntry2.weight()) {
        return {ttEntry, p_ttEntry, false};
    } else {
        return {ttEntry2, p_ttEntry2, false};
    }
}

#endif
