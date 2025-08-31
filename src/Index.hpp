#ifndef INDEX_HPP
#define INDEX_HPP

#include <array>
#include <cstring>
#include "types.hpp"
#include "bitops.hpp"
#include "io.hpp"

#define FOR_EACH(Index, i) for (Index i{static_cast<Index::_t>(0)}; i.isOk(); ++i)

template <int _Size, typename _element_type = int, int _Mask = _Size-1>
class Index {
public:
    typedef _element_type _t;
    enum { Size = _Size, Last = Size-1, Mask = _Mask };
    template <typename T> using arrayOf = std::array<T, Size>;

protected:
    _t v;

public:
    constexpr Index () : v{static_cast<_t>(_Size)} { static_assert (Size > 1); }
    constexpr Index (_t i) : v{i} { assertOk(); }

    constexpr operator const _t& () const { return v; }

    constexpr void assertOk() const { assert (isOk()); }
    constexpr bool isOk() const { return static_cast<unsigned>(v) < static_cast<unsigned>(Size); }

    constexpr bool is(_t i) const { return v == i; }

    constexpr Index& operator ++ () { assertOk(); v = static_cast<_t>(v+1); return *this; }
    constexpr Index& operator -- () { assertOk(); v = static_cast<_t>(v-1); return *this; }

    constexpr Index& flip() { assertOk(); v = static_cast<_t>(v ^ static_cast<_t>(Mask)); return *this; }
    constexpr Index operator ~ () const { return Index{v}.flip(); }

    friend ostream& operator << (ostream& out, Index& index) { return out << static_cast<int>(index.v); }

    friend istream& operator >> (istream& in, Index& index) {
        int n;
        auto before = in.tellg();
        in >> n;
        if (n < 0 || Last < n) { return io::fail_pos(in, before); }
        index.v = static_cast<Index::_t>(n);
        return in;
    }
};

template <int _Size, typename _element_type = int, int _Mask = _Size-1>
class IndexChar : public Index<_Size, _element_type, _Mask> {
    typedef Index<_Size, _element_type, _Mask> Base;
    using Base::v;

    static io::czstring The_string;

public:
    using typename Base::_t;
    using Base::Base;
    using Base::assertOk;
    constexpr operator const _t& () const { return v; }

    constexpr io::char_type to_char() const { return The_string[v]; }
    friend ostream& operator << (ostream& out, IndexChar index) { return out << index.to_char(); }

    bool from_char(io::char_type c) {
        auto p = std::memchr(The_string, c, _Size);
        if (!p) { return false; }
        v = static_cast<_t>(static_cast<io::czstring>(p) - The_string);
        assertOk();
        assert (c == to_char());
        return true;
    }

    friend istream& read(istream& in, IndexChar& index) {
        io::char_type c;
        if (in.get(c)) {
            if (!index.from_char(c)) { io::fail_char(in); }
        }
        return in;
    }

};

#endif
