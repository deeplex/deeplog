
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>
#include <span>

#include <dplx/dp/api.hpp>

#include <dplx/dlog/core/strong_types.hpp>
#include <dplx/dlog/detail/codec_dummy.hpp>
#include <dplx/dlog/detail/utils.hpp>
#include <dplx/dlog/fwd.hpp>

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
        = requires(T instance,
                   record_output_buffer_storage &bufferPlacementStorage,
                   std::size_t const messageSize,
                   span_id const spanId,
                   void (&dummy_consumer)(std::span<bytes const>) noexcept)
        {
            { instance.allocate_record_buffer_inplace(
                bufferPlacementStorage, messageSize, spanId) }
                    -> detail::tryable_result<record_output_buffer *>;
            { instance.consume_messages(dummy_consumer) }
                    -> detail::tryable;
        };
// clang-format on

// clang-format off
template <typename T>
concept bus_ex
    = bus<T>
    && requires(T instance,
                trace_id const traceId,
                std::string_view const name,
                severity thresholdInOut)
        {
            { instance.create_span_context(traceId, name, thresholdInOut) }
                    ->  std::same_as<span_context>;
        };
// clang-format on

template <typename Fn>
concept raw_message_consumer
        = requires(Fn fn, std::span<bytes const> const msgs) {
              {
                  static_cast<Fn &&>(fn)(msgs)
              } noexcept;
          };

} // namespace dplx::dlog
