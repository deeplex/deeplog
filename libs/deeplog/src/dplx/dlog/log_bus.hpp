
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <dplx/cncr/utils.hpp>
#include <dplx/dp/api.hpp>
#include <dplx/dp/disappointment.hpp>
#include <dplx/dp/streams/output_buffer.hpp>

#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/disappointment.hpp>
#include <dplx/dlog/source/log_record_port.hpp>
#include <dplx/dlog/source/record_output_buffer.hpp>

namespace dplx::dlog
{

template <typename Fn>
concept raw_message_consumer = requires(
        Fn fn, std::span<std::span<std::byte const> const> const msgs) {
                                   {
                                       static_cast<Fn &&>(fn)(msgs)
                                   } noexcept;
                               };

using bus_output_buffer = record_output_buffer;
using output_buffer_storage = record_output_buffer_storage;
using bus_output_guard = record_output_guard;

class bus_handle : public log_record_port
{
public:
    severity threshold{default_threshold};

protected:
    constexpr ~bus_handle() noexcept = default;
    constexpr bus_handle() noexcept = default;

    constexpr bus_handle(bus_handle const &) noexcept = default;
    constexpr auto operator=(bus_handle const &) noexcept
            -> bus_handle & = default;

    constexpr bus_handle(bus_handle &&) noexcept = default;
    constexpr auto operator=(bus_handle &&) noexcept -> bus_handle & = default;

    constexpr explicit bus_handle(severity initialThreshold) noexcept
        : threshold(initialThreshold)
    {
    }

public:
    [[nodiscard]] DPLX_ATTR_FORCE_INLINE auto allocate_span_context() noexcept
            -> span_context
    {
        severity dummy{};
        return create_span_context("", dummy);
    }
    [[nodiscard]] DPLX_ATTR_FORCE_INLINE auto
    allocate_span_id(trace_id trace) noexcept -> span_id
    {
        severity dummy{};
        return create_span_context(trace, "", dummy).spanId;
    }

    template <dp::encodable T>
    auto write(span_id sid, T const &msg) noexcept -> result<void>
    {
        return enqueue_message(*this, sid, msg);
    }

private:
};

} // namespace dplx::dlog
