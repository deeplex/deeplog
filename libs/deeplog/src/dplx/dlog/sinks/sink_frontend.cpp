
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/sinks/sink_frontend.hpp"

#include <dplx/dp/streams/output_buffer.hpp>

namespace dplx::dlog::detail
{

namespace
{

struct copy_message_fn
{
    dp::output_buffer &out;
    severity threshold;

    inline auto operator()(serialized_info_base const &message) const
            -> dp::result<void>
    {
        return out.bulk_write(message.raw_data);
    }
    inline auto operator()(serialized_record_info const &message) const
            -> dp::result<void>
    {
        if (message.message_severity >= threshold)
        {
            return out.bulk_write(message.raw_data);
        }
        return dp::oc::success();
    }
};

} // namespace

auto concate_messages(dp::output_buffer &out,
                      std::span<serialized_message_info const> const &messages,
                      severity const threshold) noexcept -> result<void>
{
    for (auto const &message : messages)
    {
        DPLX_TRY(visit(copy_message_fn{out, threshold}, message));
    }
    return oc::success();
}

} // namespace dplx::dlog::detail
