
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>

#include <dplx/cncr/utils.hpp>
#include <dplx/dp/api.hpp>
#include <dplx/dp/disappointment.hpp>
#include <dplx/dp/streams/output_buffer.hpp>

#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/disappointment.hpp>

namespace dplx::dlog
{

// the class is final and none of its base classes have public destructors
// NOLINTNEXTLINE(cppcoreguidelines-virtual-class-destructor)
class bus_output_buffer : public dp::output_buffer
{
public:
    virtual constexpr ~bus_output_buffer() noexcept = default;

protected:
    using output_buffer::output_buffer;

private:
    auto do_grow([[maybe_unused]] size_type requestedSize) noexcept
            -> dp::result<void> final
    {
        return dp::errc::end_of_stream;
    }
    auto do_bulk_write([[maybe_unused]] std::byte const *src,
                       [[maybe_unused]] std::size_t size) noexcept
            -> dp::result<void> final
    {
        return dp::errc::end_of_stream;
    }
};

class bus_output_guard
{
    bus_output_buffer &mOutput;

public:
    DPLX_ATTR_FORCE_INLINE ~bus_output_guard() noexcept
    {
        (void)mOutput.sync_output();
        mOutput.~bus_output_buffer();
    }
    DPLX_ATTR_FORCE_INLINE explicit bus_output_guard(
            bus_output_buffer &which) noexcept
        : mOutput(which)
    {
    }
};

struct output_buffer_storage
{
    static constexpr std::size_t static_size = 128U;
    alignas(void *) std::byte _state[static_size];
};

class bus_handle
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
    DPLX_ATTR_FORCE_INLINE auto
    create_output_buffer_inplace(output_buffer_storage &bufferPlacementStorage,
                                 std::size_t messageSize,
                                 span_id spanId) noexcept
            -> result<bus_output_buffer *>
    {
        return do_create_output_buffer_inplace(bufferPlacementStorage,
                                               messageSize, spanId);
    }

    [[nodiscard]] DPLX_ATTR_FORCE_INLINE auto allocate_span_context() noexcept
            -> span_context
    {
        return do_allocate_span_context();
    }
    [[nodiscard]] DPLX_ATTR_FORCE_INLINE auto allocate_trace_id() noexcept
            -> trace_id
    {
        return do_allocate_trace_id();
    }
    [[nodiscard]] DPLX_ATTR_FORCE_INLINE auto
    allocate_span_id(trace_id trace) noexcept -> span_id
    {
        return do_allocate_span_id(trace);
    }

    template <dp::encodable T>
    auto write(span_id sid, T const &msg) noexcept -> result<void>
    {
        auto const msgSize = dp::encoded_size_of(msg);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
        output_buffer_storage outStorage;
        DPLX_TRY(auto *out,
                 do_create_output_buffer_inplace(outStorage, msgSize, sid));
        bus_output_guard outGuard{*out};
        return dp::encode(*out, msg);
    }

private:
    virtual auto do_create_output_buffer_inplace(
            output_buffer_storage &bufferPlacementStorage,
            std::size_t messageSize,
            span_id spanId) noexcept -> result<bus_output_buffer *>
            = 0;
    virtual auto do_allocate_span_context() noexcept -> span_context = 0;
    virtual auto do_allocate_trace_id() noexcept -> trace_id = 0;
    virtual auto do_allocate_span_id(trace_id trace) noexcept -> span_id = 0;
};

} // namespace dplx::dlog

namespace dplx::dlog::detail
{

auto derive_span_id(std::uint64_t traceIdP0,
                    std::uint64_t traceIdP1,
                    std::uint64_t ctr) noexcept -> span_id;

}
