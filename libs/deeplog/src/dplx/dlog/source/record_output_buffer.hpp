
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>

#include <dplx/cncr/utils.hpp>
#include <dplx/dp.hpp>
#include <dplx/dp/streams/output_buffer.hpp>

namespace dplx::dlog
{

class record_output_buffer : public dp::output_buffer
{
public:
    virtual constexpr ~record_output_buffer() noexcept = default;

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

struct record_output_buffer_storage
{
    static constexpr std::size_t static_size = 128U;
    alignas(record_output_buffer) std::byte _state[static_size];
};

class record_output_guard
{
    record_output_buffer &mOutput;

public:
    DPLX_ATTR_FORCE_INLINE ~record_output_guard() noexcept
    {
        (void)mOutput.sync_output();
        mOutput.~record_output_buffer();
    }
    DPLX_ATTR_FORCE_INLINE explicit record_output_guard(
            record_output_buffer &which) noexcept
        : mOutput(which)
    {
    }
};

inline constexpr struct enqueue_message_fn
{
    template <dp::encodable T>
    inline auto operator()(log_record_port &destination,
                           span_id sid,
                           T const &msg) const noexcept -> result<void>
    {
        auto const msgSize = dp::encoded_size_of(msg);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
        record_output_buffer_storage outStorage;
        DPLX_TRY(auto *out, destination.allocate_record_buffer_inplace(
                                    outStorage, msgSize, sid));
        record_output_guard outGuard{*out};
        return dp::encode(*out, msg);
    }
} enqueue_message{};

} // namespace dplx::dlog
