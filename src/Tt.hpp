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

    mutable node_count_t hits = 0;
    mutable node_count_t reads = 0;
    mutable node_count_t writes = 0;

    Tt(size_t bytes) { setSize(bytes); }
    ~Tt() { free(); }

    constexpr size_t size() const { return size_; }
    void setSize(size_t bytes) { allocate(bytes); zeroFill(); }
    void newGame() { zeroFill(); }
    void newSearch() { zeroed_ = false; reads = 0; writes = 0; hits = 0; }

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

    void allocate(size_t _bytes) {
        const auto minBytes = minSize();
        auto bytes = ::bit_floor(std::max(_bytes, minBytes));

        if (bytes != size_) {
            free();

            for (; bytes >= minBytes; bytes >>= 1) {
                // 2MB align to trigger linux huge page support if possible
                auto ptr = System::allocateAligned(bytes, std::min<size_t>(bytes, 2*1024*1024));

                if (ptr) {
                    allocated_ = ptr;
                    size_ = bytes;
                    zeroed_ = false;
                    break;
                }
            }
        }

        assert (bytes == size_);
    }
};
extern Tt The_transpositionTable;

// 8 byte, always replace strategy, so no age field, only one score, depth and bound flags
class TtEntry {
    enum {
        ShiftScore = 0,
        ShiftBound = ShiftScore + Score::bit_width(),
        ShiftDraft = ShiftBound + Bound::bit_width(),
        ShiftMove  = ShiftDraft + Ply::bit_width(),
        ShiftZ     = ShiftMove  + TtMove::bit_width(), // total size of all data fields
    };

    using _t = u64_t;

#ifndef NDEBUG
    union {
        _t v_;
        struct PACKED {
            Score::_t score_ :Score::bit_width();
            Bound::_t bound_ :Bound::bit_width();
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
        Score _score,
        Bound _bound,
        Ply _draft,
        TtMove _ttMove
    ) : v_{
        (((static_cast<_t>(+_ttMove) << ShiftMove) ^ +z) & MoveZMask)
        | _score.pack<_t>(ShiftScore)
        | _bound.pack<_t>(ShiftBound)
        | _draft.pack<_t>(ShiftDraft)
    } {
        static_assert (sizeof(TtEntry) == sizeof(u64_t));

        assert (score() == _score);
        assert (bound().is(_bound));
        assert (draft() == _draft);
        assert (ttMove(z) == _ttMove);
    }

    constexpr bool none() const { return v_ == 0; }
    constexpr bool any() const { return !none(); }
    constexpr bool operator == (Z z) const { return (v_ & ZMask) == (z & ZMask); }

    constexpr Score score() const { return Score::unpack(v_, ShiftScore); }
    constexpr Bound bound() const { return Bound::unpack(v_, ShiftBound); }
    constexpr Ply draft() const { return Ply::unpack(v_, ShiftDraft); }
    constexpr TtMove ttMove(Z z) const { return TtMove::unpack(v_ ^ +z, ShiftMove); }

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
