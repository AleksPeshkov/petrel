#ifndef SIMD_HPP
#define SIMD_HPP

#include <array>
#include <type_traits>
#include <utility>

template <typename vector_type>
concept VectorLike = requires(vector_type v) {
    requires !std::is_pointer_v<vector_type>;
    requires !std::is_array_v<vector_type>;
    { v + v[0] } -> std::same_as<vector_type>;
};

template <VectorLike Vector>
using element_t = std::remove_cvref_t<decltype(Vector{}[0])>;

template <VectorLike Vector>
constexpr auto lanes_of = sizeof(Vector) / sizeof(element_t<Vector>);

template <VectorLike Vector>
constexpr Vector splat(element_t<Vector> e) {
    return Vector{} + e;
}

template <VectorLike Vector>
constexpr element_t<Vector> extract(Vector v, std::size_t i = 0) {
    return std::bit_cast< std::array<element_t<Vector>, lanes_of<Vector>> >(v)[i];
}

template <VectorLike Vector>
constexpr Vector abs(Vector v) {
    #ifdef __clang__
        return __builtin_elementwise_abs(v);
    #else
        return v < 0 ? -v : v;
    #endif
}

template <VectorLike Vector>
constexpr Vector min(Vector a, Vector b) {
    #ifdef __clang__
        return __builtin_elementwise_min(a, b);
    #else
        return a < b ? a : b;
    #endif
}

template <VectorLike Vector>
constexpr Vector max(Vector a, Vector b) {
    #ifdef __clang__
        return __builtin_elementwise_max(a, b);
    #else
        return a < b ? b : a;
    #endif
}

template <VectorLike Vector>
constexpr Vector min(Vector a, element_t<Vector> hi) {
    return min(a, splat<Vector>(hi));
}

template <VectorLike Vector>
constexpr Vector max(Vector a, element_t<Vector> lo) {
    return max(a, splat<Vector>(lo));
}

template <VectorLike Vector>
constexpr Vector relu(Vector v) {
    return max(v, Vector{});
}

template <VectorLike Vector>
constexpr Vector clamp(Vector v, element_t<Vector> lo, element_t<Vector> hi) {
    return min(max(v, lo), hi);
}

template <VectorLike Vector, VectorLike Mask>
constexpr Vector shuffle(Vector vector, Mask mask) {
#ifdef __clang__
    return __builtin_shufflevector(static_cast<Mask>(vector), mask);
#else
    return __builtin_shuffle(static_cast<Mask>(vector), mask);
#endif
}

template <VectorLike Vector, std::size_t... Is>
constexpr element_t<Vector> hadd_impl(Vector v, std::index_sequence<Is...>) {
    return (v[Is] + ...);
}

template <VectorLike Vector>
constexpr element_t<Vector> hadd(Vector v) {
    #ifdef __clang__
        return __builtin_reduce_add(v);
    #else
        return hadd_impl(v, std::make_index_sequence<lanes_of<Vector>>{});
    #endif
}

#endif
