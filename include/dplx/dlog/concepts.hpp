
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>

#include <span>

#include <dplx/dp/concepts.hpp>

#include <dplx/dlog/detail/codec_dummy.hpp>
#include <dplx/dlog/detail/utils.hpp>
#include <dplx/dlog/disappointment.hpp>

namespace dplx::dlog
{

using bytes = std::span<std::byte const>;
using writable_bytes = std::span<std::byte>;

// clang-format off
template <typename T>
concept source
    = requires (T &&t, detail::encodable_dummy const &dummy)
        {
            { t.static_resource(dummy) }
                    -> detail::tryable_result<std::uint64_t>;

        };
// clang-format on

// clang-format off
template <typename T>
concept bus
        = dp::output_stream<typename T::output_stream>
        && requires(T &&t,
                    typename T::logger_token logger,
                    unsigned int const size,
                    void (&dummy_consumer)(std::span<std::byte const>) noexcept)
        {
            typename T::logger_token;
            { t.create_token() }
                    -> detail::tryable_result<typename T::logger_token>;

            typename T::output_stream;
            { t.write(logger, size) }
                    -> detail::tryable_result<typename T::output_stream>;
            t.commit(logger);

            { t.consume_content(dummy_consumer) }
                    -> detail::tryable;
        };
// clang-format on

// clang-format off
template <typename Fn>
concept bus_consumer
        = requires(Fn &&fn, std::span<std::byte const> const content)
        {
            static_cast<Fn &&>(fn)(content);
        };
// clang-format on

} // namespace dplx::dlog
