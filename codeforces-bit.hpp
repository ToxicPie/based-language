#ifndef __CODEFORCES_BIT_HPP
#define __CODEFORCES_BIT_HPP

#include <version>

// workaround for polygon which uses c++ <= 17
#if __cpp_lib_bitops >= 201907L

#include <bit>

#else

#include <cstdint>
#include <limits>
#include <type_traits>

namespace std {

template <typename T>
constexpr std::enable_if_t<std::is_unsigned_v<T>, T> rotl(T x, int s);
template <typename T>
constexpr std::enable_if_t<std::is_unsigned_v<T>, T> rotr(T x, int s);

template <typename T>
constexpr std::enable_if_t<std::is_unsigned_v<T>, T> rotl(T x, int s) {
    constexpr int N = std::numeric_limits<T>::digits;
    int r = s % N;
    if (r < 0) {
        return rotr(x, -s);
    }
    if (r == 0) {
        return x;
    }
    return (x << r) | (x >> (N - r));
}

template <typename T>
constexpr std::enable_if_t<std::is_unsigned_v<T>, T> rotr(T x, int s) {
    constexpr int N = std::numeric_limits<T>::digits;
    int r = s % N;
    if (r < 0) {
        return rotl(x, -s);
    }
    if (r == 0) {
        return x;
    }
    return (x >> r) | (x << (N - r));
}

template <typename T>
constexpr std::enable_if_t<std::is_unsigned_v<T>, bool> has_single_bit(T x) {
    return x && !(x & (x - 1));
}

}; // namespace std

#endif

#endif // __CODEFORCES_BIT_HPP
