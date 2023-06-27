
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <dplx/cncr/utils.hpp>

#include <dplx/dlog/core/strong_types.hpp>
#include <dplx/dlog/fwd.hpp>

namespace dplx::dlog
{

class log_record_port
{
protected:
    ~log_record_port() = default;
    log_record_port() noexcept = default;

    log_record_port(log_record_port const &) noexcept = default;
    auto operator=(log_record_port const &) noexcept
            -> log_record_port & = default;

    log_record_port(log_record_port &&) noexcept = default;
    auto operator=(log_record_port &&) noexcept -> log_record_port & = default;

public:
    [[nodiscard]] DPLX_ATTR_FORCE_INLINE auto allocate_record_buffer_inplace(
            record_output_buffer_storage &bufferPlacementStorage,
            std::size_t messageSize,
            span_id spanId) noexcept -> result<record_output_buffer *>
    {
        return do_allocate_record_buffer_inplace(bufferPlacementStorage,
                                                 messageSize, spanId);
    }
    [[nodiscard]] DPLX_ATTR_FORCE_INLINE auto
    create_span_context(trace_id traceId,
                        std::string_view name,
                        severity &thresholdInOut) noexcept -> span_context
    {
        return do_create_span_context(traceId, name, thresholdInOut);
    }
    [[nodiscard]] DPLX_ATTR_FORCE_INLINE auto default_threshold() noexcept
            -> severity
    {
        // TODO: make virtual and configurable
        return dlog::default_threshold;
    }

private:
    virtual auto do_allocate_record_buffer_inplace(
            record_output_buffer_storage &bufferPlacementStorage,
            std::size_t messageSize,
            span_id spanId) noexcept -> result<record_output_buffer *>
            = 0;

    virtual auto do_create_span_context(trace_id id,
                                        std::string_view name,
                                        severity &thresholdInOut) noexcept
            -> span_context
            = 0;
};

} // namespace dplx::dlog

namespace dplx::dlog::detail
{

auto derive_span_id(std::uint64_t traceIdP0,
                    std::uint64_t traceIdP1,
                    std::uint64_t ctr) noexcept -> span_id;

}
