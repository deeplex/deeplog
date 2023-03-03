
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>

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

struct output_buffer_storage
{
    static constexpr std::size_t static_size = 128U;
    alignas(void *) std::byte _state[static_size];
};

class bus_handle
{
protected:
    constexpr ~bus_handle() noexcept = default;
    constexpr bus_handle() noexcept = default;

    constexpr bus_handle(bus_handle const &) noexcept = default;
    constexpr auto operator=(bus_handle const &) noexcept
            -> bus_handle & = default;

    constexpr bus_handle(bus_handle &&) noexcept = default;
    constexpr auto operator=(bus_handle &&) noexcept -> bus_handle & = default;

public:
    auto
    create_output_buffer_inplace(output_buffer_storage &bufferPlacementStorage,
                                 std::size_t messageSize,
                                 span_id spanId) noexcept
            -> result<bus_output_buffer *>
    {
        return do_create_output_buffer_inplace(bufferPlacementStorage,
                                               messageSize, spanId);
    }

private:
    virtual auto do_create_output_buffer_inplace(
            output_buffer_storage &bufferPlacementStorage,
            std::size_t messageSize,
            span_id spanId) noexcept -> result<bus_output_buffer *>
            = 0;
};

class bus_output_guard
{
    bus_output_buffer &mOutput;

public:
    ~bus_output_guard() noexcept
    {
        (void)mOutput.sync_output();
        mOutput.~bus_output_buffer();
    }
    explicit bus_output_guard(bus_output_buffer &which) noexcept
        : mOutput(which)
    {
    }
};

} // namespace dplx::dlog
