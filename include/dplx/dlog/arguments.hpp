
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
};

template <typename T>
inline auto tag_invoke(dp::encoded_size_of_fn, argument<T> const &arg) noexcept
        -> int
{
    return dp::encoded_size_of(2u) + dp::encoded_size_of(argument<T>::type_id)
         + dp::encoded_size_of(arg.value);
}

template <typename Char, typename T>
struct named_arg
{
    std::basic_string_view<Char> name;
    T const &value;
};

template <typename Char, typename T>
inline auto tag_invoke(dp::encoded_size_of_fn,
                       named_arg<Char, T> const &arg) noexcept -> int
{
    return dp::encoded_size_of(3u) + dp::encoded_size_of(arg.name)
         + dp::encoded_size_of(argument<T>::type_id)
         + dp::encoded_size_of(arg.value);
}

template <typename T>
struct argument<named_arg<char, T>>
{
    using type = T;
    char const *name;
    type value;

    static constexpr resource_id type_id{23u};
};
template <typename T>
struct argument<named_arg<char8_t, T>>
{
    using type = T;
    char8_t const *name;
    type value;

    static constexpr resource_id type_id{23u};
};

} // namespace dplx::dlog

namespace dplx::dp
{

template <typename T, output_stream Stream>
class basic_encoder<dlog::argument<T>, Stream>
{
    using emit = item_emitter<Stream>;
    using key_encoder = basic_encoder<std::uint64_t, Stream>;
    using value_encoder = basic_encoder<T, Stream>;

public:
    using value_type = dlog::argument<T>;

    auto operator()(Stream &stream,
                    detail::select_proper_param_type<value_type> arg) const
            -> result<void>
    {
        DPLX_TRY(emit::array(stream, 2u));
        DPLX_TRY(key_encoder()(dlog::argument<T>::type_id));
        DPLX_TRY(value_encoder()(arg.value));
        return oc::success();
    }
};

template <typename Char, typename T, output_stream Stream>
class basic_encoder<dlog::named_arg<Char, T>, Stream>
{
    using emit = item_emitter<Stream>;
    using name_encoder = basic_encoder<std::basic_string_view<Char>, Stream>;
    using type_encoder = basic_encoder<dlog::resource_id, Stream>;
    using value_encoder = basic_encoder<T, Stream>;

public:
    using value_type = dlog::argument<fmt::detail::named_arg<char, T>>;

    auto operator()(Stream &stream, value_type const &arg) const -> result<void>
    {
        DPLX_TRY(emit::array(stream, 3u));
        DPLX_TRY(name_encoder()(arg.name));
        DPLX_TRY(type_encoder()(dlog::argument<T>::type_id));
        DPLX_TRY(value_encoder()(arg.value));
        return oc::success();
    }
};

} // namespace dplx::dp
