
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <algorithm>
#include <limits>
#include <type_traits>

namespace dplx::dlog::detail
{

template <typename T, typename... Ts>
concept any_of = (std::same_as<T, Ts> || ...);

template <typename T, typename... Ts>
concept none_of = (!std::same_as<T, Ts> && ...);

template <typename T>
concept integer = std::integral<T> && detail::none_of < std::remove_cv_t<T>,
bool, char, wchar_t, char8_t, char16_t, char32_t > ;

template <typename T>
concept signed_integer = integer<T> && std::is_signed_v<T>;

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

template <typename T>
using remove_cref_t = std::remove_const_t<std::remove_reference_t<T>>;

template <typename Enum>
    requires std::is_enum_v<Enum>
constexpr auto to_underlying(Enum value) noexcept ->
        typename std::underlying_type<Enum>::type
{
    return static_cast<std::underlying_type_t<Enum>>(value);
}

template <std::size_t N, typename T>
consteval auto make_byte_array(std::initializer_list<T> vs,
                               std::byte const fill = std::byte{0xFE}) noexcept
        -> std::array<std::byte, N>
{
    std::array<std::byte, N> bs;
    auto last
            = std::transform(vs.begin(), vs.end(), bs.data(),
                             [](auto v) { return static_cast<std::byte>(v); });
    for (auto const bsEnd = bs.data() + N; last != bsEnd; ++last)
    {
        *last = fill;
    }
    return bs;
}
} // namespace dplx::dlog::detail
