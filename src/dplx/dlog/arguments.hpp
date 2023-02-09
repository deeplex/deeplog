
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <concepts>
#include <type_traits>

#include <dplx/cncr/math_supplement.hpp>
#include <dplx/dp.hpp>
#include <dplx/dp/api.hpp>
#include <dplx/dp/codecs/core.hpp>
#include <dplx/dp/codecs/std-string.hpp>
#include <dplx/dp/items/emit_core.hpp>

#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/detail/utils.hpp>
#include <dplx/dlog/disappointment.hpp>

namespace dplx::dlog
{

template <typename T>
struct argument;

// clang-format off
template <typename T>
concept loggable_argument
    =  requires
    {
        typename T;
        typename argument<T>;
    }
    && dp::encodable<argument<T>>
    && requires
        {
            { argument<T>::type_id }
                    -> std::convertible_to<resource_id>;
        };
// clang-format on

template <cncr::integer T>
struct argument<T>
{
    using type = T;
    type value;

    static constexpr resource_id type_id{std::is_unsigned_v<T> ? 0U : 1U};
};

template <typename Char, typename T>
    requires(std::same_as<Char, char> || std::same_as<Char, char8_t>)
struct named_arg
{
    std::basic_string_view<Char> name;
    T const &value;
};

inline constexpr struct arg_fn
{
    template <typename T>
    auto operator()(std::string_view name, T const &value) const noexcept
            -> named_arg<char, T>
    {
        return {name, value};
    }
    template <typename T>
    auto operator()(std::u8string_view name, T const &value) const noexcept
            -> named_arg<char8_t, T>
    {
        return {name, value};
    }
} arg{};

template <typename Char, typename T>
struct argument<named_arg<Char, T>>
{
    using type = T;
    std::basic_string_view<Char> name;
    type const &value;

    explicit constexpr argument(named_arg<Char, T> const &self) noexcept
        : name(self.name)
        , value(self.value)
    {
    }

    static constexpr resource_id type_id{argument<T>::type_id};
};

} // namespace dplx::dlog

template <typename T>
class dplx::dp::codec<dplx::dlog::argument<T>>
{
public:
    static inline auto size_of(emit_context const &ctx,
                               dplx::dlog::argument<T> arg) noexcept
            -> std::uint64_t
    {
        return dp::encoded_item_head_size<type_code::array>(2U)
             + dp::encoded_size_of(ctx, static_cast<dlog::resource_id>(
                                                dlog::argument<T>::type_id))
             + dp::encoded_size_of(ctx, arg.value);
    }
    static inline auto encode(emit_context const &ctx,
                              dplx::dlog::argument<T> arg) noexcept
            -> result<void>
    {
        DPLX_TRY(dp::emit_array(ctx, 2U));
        DPLX_TRY(dp::encode(ctx, static_cast<dlog::resource_id>(
                                         dlog::argument<T>::type_id)));
        return dp::encode(ctx, arg.value);
    }
};

template <typename Char, dplx::dlog::loggable_argument T>
class dplx::dp::codec<dplx::dlog::argument<dplx::dlog::named_arg<Char, T>>>
{
public:
    static inline auto
    size_of(emit_context const &ctx,
            dplx::dlog::argument<dplx::dlog::named_arg<Char, T>> const
                    &arg) noexcept -> std::uint64_t
    {
        return dp::encoded_item_head_size<type_code::array>(3U)
             + dp::encoded_size_of(ctx, static_cast<dlog::resource_id>(
                                                dlog::argument<T>::type_id))
             + dp::encoded_size_of(ctx, arg.value);
    }
    static inline auto
    encode(emit_context const &ctx,
           dplx::dlog::argument<dplx::dlog::named_arg<Char, T>> const
                   &arg) noexcept -> result<void>
    {
        DPLX_TRY(dp::emit_array(ctx, 3U));
        DPLX_TRY(dp::encode(ctx, static_cast<dlog::resource_id>(
                                         dlog::argument<T>::type_id)));
        DPLX_TRY(dp::encode(ctx, arg.name));
        return dp::encode(ctx, arg.value);
    }
};
