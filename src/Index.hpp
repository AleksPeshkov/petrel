#ifndef INDEX_HPP
#define INDEX_HPP

#include <array>
#include <cstring>
#include "types.hpp"
#include "bitops.hpp"
#include "io.hpp"

#define FOR_EACH(Index, i) for (Index i = static_cast<Index::_t>(0); static_cast<index_t>(i) < Index::Size; ++i)

template <size_t _Size, typename _Value_type = index_t, class _Actual_type = _Value_type, size_t _Mask = _Size-1>
class Index {
    static io::czstring The_string;

public:
    typedef _Value_type _t;
    enum { Size = _Size, Mask = _Mask };
    template <typename T> using arrayOf = std::array<T, Size>;

protected:
    _t v;

public:
    constexpr Index () : v{} {}
    constexpr Index (_t i) : v{i} { assertValid(); }

    constexpr operator _t () const { return v; }

    constexpr void assertValid() const { assert (isValid()); }
    constexpr bool isValid() const { return static_cast<index_t>(v) < Size; }

    constexpr bool is(_t i) const { return v == i; }

    constexpr Index& operator ++ () { assertValid(); v = static_cast<_t>(v+1); return *this; }
    constexpr Index& operator -- () { v = static_cast<_t>(v-1); assertValid(); return *this; }

    constexpr Index& flip() { assertValid(); v = static_cast<_t>(v ^ Mask); return *this; }
    constexpr Index operator ~ () const { return Index{v}.flip(); }

    constexpr io::char_type to_char() const { return The_string[v]; }
    friend io::ostream& operator << (io::ostream& out, Index i) { return out << i.to_char(); }

    bool from_char(io::char_type c) {
        auto p = std::memchr(The_string, c, Size);
        if (!p) { return false; }
        this->v = static_cast<_t>(static_cast<io::czstring>(p) - The_string);
        assertValid();
        assert (c == to_char());
        return true;
    }

    friend io::istream& read(io::istream& in, Index& i) {
        io::char_type c;
        if (in.get(c)) {
            if (!i.from_char(c)) { io::fail_char(in); }
        }
        return in;
    }

};

#endif
