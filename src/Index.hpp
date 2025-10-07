#ifndef INDEX_HPP
#define INDEX_HPP

#include <array>
#include <concepts>
#include <cstring>
#include <type_traits>
#include <ranges>
#include "bitops.hpp"
#include "io.hpp"

template <int _Size, typename _element_type = int, typename _storage_type = int>
class Index {
    using element_type = _element_type;
    using storage_t = _storage_type;
public:
    using _t = element_type;
    constexpr static storage_t Size = _Size;
    constexpr static storage_t Mask =  static_cast<storage_t>(Size-1);
    constexpr static element_type Last = static_cast<element_type>(_Size-1);

    template <typename T> using arrayOf = std::array<T, Size>;

protected:
    storage_t v;

public:
    explicit constexpr Index (element_type i = static_cast<element_type>(0)) : v{i} { assertOk(); }
    constexpr operator element_type () const { assertOk(); return static_cast<element_type>(v); }

    constexpr void assertOk() const { assert (isOk()); }
    [[nodiscard]] constexpr bool isOk() const { return static_cast<unsigned>(v) < static_cast<unsigned>(Size); }

    constexpr bool is(element_type i) const { return v == i; }

    constexpr Index& operator ++ () { assertOk(); ++v; return *this; }
    constexpr Index& operator -- () { assertOk(); --v; return *this; }
    constexpr Index operator ++ (int) { assertOk(); auto result = Index{v}; ++v; return result; }
    constexpr Index operator -- (int) { assertOk(); auto result = Index{v}; --v; return result; }

    constexpr Index& flip() { assertOk(); v = v ^ Mask; return *this; }
    constexpr Index operator ~ () const { return Index{static_cast<Index::_t>(v)}.flip(); }

    friend bool operator < (Index a, Index b) { b.assertOk(); return a.v < b.v; }
    friend bool operator <= (Index a, Index b) { b.assertOk(); return a.v <= b.v; }
    friend bool operator < (Index a, storage_t b) { return a.v < b; }
    friend bool operator <= (Index a, storage_t b) { return a.v <= b; }

    friend ostream& operator << (ostream& out, Index& index) { return out << static_cast<int>(index.v); }

    friend istream& operator >> (istream& in, Index& index) {
        int n;
        auto before = in.tellg();
        in >> n;
        if (n < 0 || Last < n) { return io::fail_pos(in, before); }
        index.v = n;
        return in;
    }

    static constexpr auto range() {
        return std::views::iota(0, Size) | std::views::transform([](int i) {
            return Index{static_cast<element_type>(i)};
        });
    }
};

template <int _Size, typename _element_type = int, typename _storage_type = int>
class IndexChar : public Index<_Size, _element_type, _storage_type> {
    using Base = Index<_Size, _element_type, _storage_type>;
    using Base::v;

    static io::czstring The_string;

public:
    using typename Base::_t;
    using Base::Base;
    using Base::assertOk;

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

    friend istream& operator >> (istream& in, IndexChar& index) {
        io::char_type c;
        if (in.get(c)) {
            if (!index.from_char(c)) { io::fail_char(in); }
        }
        return in;
    }

    static constexpr auto range() {
        return std::views::iota(0, _Size) | std::views::transform([](int i) {
            return IndexChar{static_cast<_element_type>(i)};
        });
    }

};

#endif
