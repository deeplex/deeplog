
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <limits>
#include <type_traits>

namespace dplx::dlog::detail
{

template <typename T, typename... Ts>
concept any_of = (std::same_as<T, Ts> || ...);

template <typename T, typename... Ts>
concept none_of = (!std::same_as<T, Ts> && ...);

template <typename T>
concept integer = std::integral<T> &&detail::none_of<std::remove_cv_t<T>,
                                                     bool,
                                                     char,
                                                     wchar_t,
                                                     char8_t,
                                                     char16_t,
                                                     char32_t>;

template <typename T>
concept signed_integer = integer<T> &&std::is_signed_v<T>;

template <typename T>
concept unsigned_integer = integer<T> && !signed_integer<T>;

template <unsigned_integer T, unsigned_integer U>
constexpr auto div_ceil(T dividend, U divisor) noexcept
        -> std::common_type_t<T, U>
{
    return dividend / divisor + (dividend % divisor != 0);
}

template <unsigned_integer T>
constexpr auto make_mask(int num, int off = 0) noexcept -> T
{
    constexpr auto digits = std::numeric_limits<T>::digits;
    return (((static_cast<T>(num < digits)) << (num & (digits - 1))) - 1u)
        << off;
}

} // namespace dplx::dlog::detail
