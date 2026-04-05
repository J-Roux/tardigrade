#pragma once

#include "platform.hpp"

namespace tartigrada
{

namespace detail
{

// FNV-1a constants sized to the platform's size_t
#if SIZE_MAX == 0xFFFFFFFFFFFFFFFFULL
constexpr tartigrada::size_t fnv_basis = 14695981039346656037ULL;
constexpr tartigrada::size_t fnv_prime = 1099511628211ULL;
#elif SIZE_MAX == 0xFFFFFFFF
constexpr tartigrada::size_t fnv_basis = 2166136261UL;
constexpr tartigrada::size_t fnv_prime = 16777619UL;
#else
// 16-bit (AVR)
constexpr tartigrada::size_t fnv_basis = 0x811c;
constexpr tartigrada::size_t fnv_prime = 0x0101;
#endif

[[nodiscard]] constexpr tartigrada::size_t fnv1a(const char* s) noexcept
{
    tartigrada::size_t hash = fnv_basis;
    for (; *s; ++s)
    {
        hash ^= static_cast<tartigrada::size_t>(*s);
        hash *= fnv_prime;
    }
    return hash;
}

// Returns the full decorated function signature, which is unique per T.
template<typename T>
[[nodiscard]] constexpr const char* type_sig() noexcept
{
#if defined(_MSC_VER)
    return __FUNCSIG__;
#else
    return __PRETTY_FUNCTION__;
#endif
}

} // namespace detail

template<typename T>
[[nodiscard]] constexpr tartigrada::size_t type_id() noexcept
{
    return detail::fnv1a(detail::type_sig<T>());
}

} // namespace tartigrada
