
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <concepts>
#include <cstddef>
#include <span>
#include <string_view>

#include <dplx/dp/fwd.hpp>
#include <dplx/make.hpp>

#include <dplx/dlog/fwd.hpp>

namespace dplx::dlog
{

using bytes = std::span<std::byte const>;
using writable_bytes = std::span<std::byte>;

// clang-format off
template <typename T>
concept bus
        = makable<T>
       && requires(T instance,
                   record_output_buffer_storage &bufferPlacementStorage,
                   std::size_t const messageSize,
                   span_id const spanId,
                   void (&dummy_consumer)(std::span<bytes const>) noexcept)
        {
            { instance.allocate_record_buffer_inplace(
                bufferPlacementStorage, messageSize, spanId) }
                    -> cncr::tryable_result<record_output_buffer *>;
            { instance.consume_messages(dummy_consumer) }
                    -> cncr::tryable;
        };
// clang-format on

template <bus MessageBus>
class log_fabric;

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
              { static_cast<Fn &&>(fn)(msgs) } noexcept;
          };

template <typename T>
concept sink_backend = makable<T> && std::derived_from<T, dp::output_buffer>;

template <typename T>
concept sink = makable<T> && std::derived_from<T, sink_frontend_base>;

template <sink_backend Backend>
class basic_sink_frontend;

} // namespace dplx::dlog
