
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>
#include <span>

#include <dplx/dp/api.hpp>

#include <dplx/dlog/detail/codec_dummy.hpp>
#include <dplx/dlog/detail/utils.hpp>
#include <dplx/dlog/disappointment.hpp>
#include <dplx/dlog/log_bus.hpp>

namespace dplx::dlog
{

using bytes = std::span<std::byte const>;
using writable_bytes = std::span<std::byte>;

// clang-format off
template <typename T>
concept source
    = requires (T t, detail::encodable_dummy const dummy)
        {
            { t.static_resource(dummy) }
                    -> detail::tryable_result<std::uint64_t>;

        };
// clang-format on

// clang-format off
template <typename T>
concept bus
        = std::derived_from<T, bus_handle>
        && requires(T t,
                    void (&dummy_consumer)(std::span<std::byte const>) noexcept)
        {
            { t.consume_content(dummy_consumer) }
                    -> detail::tryable;
        };
// clang-format on

// clang-format off
template <typename Fn>
concept bus_consumer
        = requires(Fn fn, std::span<std::byte const> const content)
        {
            static_cast<Fn &&>(fn)(content);
        };
// clang-format on

} // namespace dplx::dlog
