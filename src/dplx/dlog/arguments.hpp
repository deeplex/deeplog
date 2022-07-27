
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <concepts>
#include <type_traits>

#include <dplx/dp/concepts.hpp>
#include <dplx/dp/encoder/api.hpp>
#include <dplx/dp/encoder/core.hpp>
#include <dplx/dp/encoder/narrow_strings.hpp>
#include <dplx/dp/item_emitter.hpp>

#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/detail/utils.hpp>
#include <dplx/dlog/disappointment.hpp>

namespace dplx::dlog
{

template <typename T>
struct argument;

// clang-format off
template <typename T, typename Stream>
concept loggable_argument
    = dp::encodable<argument<T>, Stream>
    && requires
        {
            typename argument<T>;
            { argument<T>::type_id }
                    -> std::convertible_to<resource_id>;
        };
// clang-format on

template <detail::integer T>
struct argument<T>
{
    using type = T;
    type value;

    static constexpr resource_id type_id{std::is_unsigned_v<T> ? 0u : 1u};

    friend inline auto tag_invoke(dp::encoded_size_of_fn,
                                  argument const &self) noexcept
    {
        return dp::additional_information_size(2u)
             + dp::encoded_size_of(argument<T>::type_id)
             + dp::encoded_size_of(self.value);
    }
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

    friend inline auto tag_invoke(dp::encoded_size_of_fn,
                                  argument const &self) noexcept
    {
        return dp::additional_information_size(3u)
             + dp::encoded_size_of(type_id) + dp::encoded_size_of(self.name)
             + dp::encoded_size_of(self.value);
    }
};

} // namespace dplx::dlog

namespace dplx::dp
{

template <typename T, output_stream Stream>
class basic_encoder<dlog::argument<T>, Stream>
{
    using emit = item_emitter<Stream>;
    using type_encoder = basic_encoder<dlog::resource_id, Stream>;
    using value_encoder = basic_encoder<T, Stream>;

public:
    using value_type = dlog::argument<T>;

    auto operator()(Stream &stream,
                    detail::select_proper_param_type<value_type> arg) const
            -> result<void>
    {
        DPLX_TRY(emit::array(stream, 2u));
        DPLX_TRY(type_encoder()(stream, dlog::argument<T>::type_id));
        DPLX_TRY(value_encoder()(stream, arg.value));
        return oc::success();
    }
};

template <typename Char, typename T, output_stream Stream>
    requires dlog::loggable_argument<T, Stream>
class basic_encoder<dlog::argument<dlog::named_arg<Char, T>>, Stream>
{
    using emit = item_emitter<Stream>;
    using type_encoder = basic_encoder<dlog::resource_id, Stream>;
    using name_encoder = basic_encoder<std::basic_string_view<Char>, Stream>;
    using value_encoder = basic_encoder<T, Stream>;

public:
    using value_type = dlog::argument<dlog::named_arg<Char, T>>;

    auto operator()(Stream &stream, value_type const &arg) const -> result<void>
    {
        DPLX_TRY(emit::array(stream, 3u));
        DPLX_TRY(type_encoder()(stream, dlog::argument<T>::type_id));
        DPLX_TRY(name_encoder()(stream, arg.name));
        DPLX_TRY(value_encoder()(stream, arg.value));
        return oc::success();
    }
};

} // namespace dplx::dp
