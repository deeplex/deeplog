
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>

#include <dplx/cncr/utils.hpp>

namespace dplx::dlog::detail
{

template <typename I, typename O>
struct in_out_result
{
    DPLX_ATTR_NO_UNIQUE_ADDRESS I in;
    DPLX_ATTR_NO_UNIQUE_ADDRESS O out;

    template <class I2, class O2>
        requires std::convertible_to<I const &, I2>
              && std::convertible_to<O const &, O2>
    constexpr operator in_out_result<I2, O2>() const &
    {
        return {in, out};
    }
    template <class I2, class O2>
        requires std::convertible_to<I, I2> && std::convertible_to<O, O2>
    constexpr operator in_out_result<I2, O2>() &&
    {
        return {static_cast<I &&>(in), static_cast<O &&>(out)};
    }
};

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)

constexpr auto is_hex_digit(char const digit) noexcept -> bool
{
    unsigned const udigit = static_cast<unsigned char>(digit);
    return (udigit - '0' <= 9U) || ((udigit | 0x20U) - 'a' <= 5U);
}

constexpr auto value_of_hex_digit(char const digit) noexcept -> std::uint8_t
{
    return static_cast<std::uint8_t>((static_cast<unsigned>(digit) & 0xfU)
                                     + (static_cast<unsigned>(digit) >> 6U)
                                               * 9U);
}

constexpr auto hex_digit_of_nibble(unsigned nibble) noexcept -> char
{
    char const lut[] = "0123456789abcdef";
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
    return lut[nibble & 0xfU];
}

template <std::input_iterator I, std::sentinel_for<I> S, typename O>
    requires std::same_as<std::iter_value_t<I>, char>
          && (std::output_iterator<O, std::byte>
              || std::output_iterator<O, std::uint8_t>)
             constexpr auto hex_decode(I first, S last, O result) noexcept
             -> in_out_result<I, O>
{
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    while (first != last)
    {
        std::uint8_t const hi = detail::value_of_hex_digit(*first++);
        std::uint8_t const lo = detail::value_of_hex_digit(*first++);
        *result++ = static_cast<std::iter_value_t<O>>((hi << 4) | lo);
    }
    return {static_cast<I &&>(first), static_cast<O &&>(result)};

    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

template <std::input_iterator I,
          std::sentinel_for<I> S,
          std::output_iterator<char> O>
    requires std::convertible_to<std::iter_value_t<I>, std::uint8_t>
constexpr auto hex_encode(I first, S last, O result) noexcept
        -> in_out_result<I, O>
{
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    while (first != last)
    {
        unsigned const byte = static_cast<std::uint8_t>(*first++);
        *result++ = detail::hex_digit_of_nibble(byte >> 4);
        *result++ = detail::hex_digit_of_nibble(byte);
    }
    return {static_cast<I &&>(first), static_cast<O &&>(result)};

    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

} // namespace dplx::dlog::detail
