
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
concept integer = std::integral<T>
               && detail::none_of<std::remove_cv_t<T>,
                                  bool,
                                  char,
                                  wchar_t,
                                  char8_t,
                                  char16_t,
                                  char32_t>;

template <typename T>
concept signed_integer = integer<T> && std::is_signed_v<T>;

template <typename T>
concept unsigned_integer = integer<T> && !
signed_integer<T>;

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
    return (((static_cast<T>(num < digits)) << (num & (digits - 1))) - 1U)
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
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
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

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
// type arguments can't be enclosed in parentheses
// NOLINTBEGIN(bugprone-macro-parentheses)

#define DPLX_DLOG_DECLARE_CODEC(_fq_type)                                      \
    template <>                                                                \
    class dplx::dp::codec<_fq_type>                                            \
    {                                                                          \
    public:                                                                    \
        static auto size_of(emit_context const &ctx,                           \
                            _fq_type const &value) noexcept -> std::uint64_t;  \
        static auto encode(emit_context const &ctx,                            \
                           _fq_type const &value) noexcept -> result<void>;    \
        static auto decode(parse_context &ctx, _fq_type &outValue) noexcept    \
                -> result<void>;                                               \
    }

#define DPLX_DLOG_DEFINE_AUTO_TUPLE_CODEC(_fq_type)                            \
    auto ::dplx::dp::codec<_fq_type>::size_of(emit_context const &ctx,         \
                                              _fq_type const &value) noexcept  \
            -> std::uint64_t                                                   \
    {                                                                          \
        static_assert(packable_tuple<_fq_type>);                               \
        return dp::size_of_tuple(ctx, value);                                  \
    }                                                                          \
    auto ::dplx::dp::codec<_fq_type>::encode(emit_context const &ctx,          \
                                             _fq_type const &value) noexcept   \
            -> result<void>                                                    \
    {                                                                          \
        return dp::encode_tuple(ctx, value);                                   \
    }                                                                          \
    auto ::dplx::dp::codec<_fq_type>::decode(                                  \
            parse_context &ctx, _fq_type &outValue) noexcept -> result<void>   \
    {                                                                          \
        return dp::decode_tuple(ctx, outValue);                                \
    }

#define DPLX_DLOG_DEFINE_AUTO_OBJECT_CODEC(_fq_type)                           \
    auto ::dplx::dp::codec<_fq_type>::size_of(emit_context const &ctx,         \
                                              _fq_type const &value) noexcept  \
            -> std::uint64_t                                                   \
    {                                                                          \
        static_assert(packable_object<_fq_type>);                              \
        return dp::size_of_object(ctx, value);                                 \
    }                                                                          \
    auto ::dplx::dp::codec<_fq_type>::encode(emit_context const &ctx,          \
                                             _fq_type const &value) noexcept   \
            -> result<void>                                                    \
    {                                                                          \
        return dp::encode_object(ctx, value);                                  \
    }                                                                          \
    auto ::dplx::dp::codec<_fq_type>::decode(                                  \
            parse_context &ctx, _fq_type &outValue) noexcept -> result<void>   \
    {                                                                          \
        return dp::decode_object(ctx, outValue);                               \
    }

// NOLINTEND(bugprone-macro-parentheses)
// NOLINTEND(cppcoreguidelines-macro-usage)
