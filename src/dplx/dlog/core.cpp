
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/core.hpp"

#include <boost/predef/compiler.h>

#include <dplx/dp/codecs/auto_tuple.hpp>
#include <dplx/dp/items/skip_item.hpp>
#include <dplx/dp/legacy/memory_input_stream.hpp>

#if defined BOOST_COMP_MSVC_AVAILABLE

#pragma warning(disable : 4146) // C4146: unary minus operator applied to
                                // unsigned type, result still unsigned

#endif

namespace dplx::dlog
{

void sink_frontend_base::push(
        bytes rawMessage, additional_record_info const &additionalInfo) noexcept
{
    if (mLastRx.has_error()) [[unlikely]]
    {
        return;
    }
    if (mThreshold < additionalInfo.message_severity)
    {
        return;
    }
    if (mShouldDiscard) [[unlikely]]
    {
        if (mShouldDiscard(rawMessage, additionalInfo))
        {
            return;
        }
    }

    mLastRx = retire(rawMessage, additionalInfo);
}

} // namespace dplx::dlog

namespace dplx::dlog::detail
{

auto consume_record_fn::multicast(
        bytes const message,
        std::span<std::unique_ptr<sink_frontend_base> const> sinks) noexcept
        -> result<void>
{
    dp::memory_view messageView(message);
    auto &&buffer = dp::get_input_buffer(messageView);
    dp::parse_context ctx{buffer};

    DPLX_TRY(auto &&tupleInfo, dp::decode_tuple_head(ctx));

    additional_record_info addInfo{};
    DPLX_TRY(addInfo.message_severity, dp::decode(dp::as_value<severity>, ctx));

    DPLX_TRY(dp::decode(ctx, addInfo.timestamp));

    DPLX_TRY(auto messageInfo, dp::parse_item_head(ctx));
    if (messageInfo.type != dp::type_code::text || messageInfo.indefinite())
    {
        return dp::errc::item_type_mismatch;
    }
    if (messageInfo.value > buffer.input_size())
    {
        return dp::errc::missing_data;
    }

    buffer.discard_buffered(messageInfo.value);
    addInfo.message_size = messageInfo.encoded_length
                         + static_cast<unsigned>(messageInfo.value);

    if (tupleInfo.num_properties > 3)
    {
        DPLX_TRY(buffer.sync_input());
        addInfo.attributes_size = -messageView.consumed_size();

        DPLX_TRY(dp::skip_item(ctx));

        DPLX_TRY(buffer.sync_input());
        addInfo.attributes_size += messageView.consumed_size();
    }
    if (tupleInfo.num_properties > 4)
    {
        DPLX_TRY(buffer.sync_input());
        addInfo.format_args_size = -messageView.consumed_size();

        DPLX_TRY(dp::skip_item(ctx));

        DPLX_TRY(buffer.sync_input());
        addInfo.format_args_size += messageView.consumed_size();
    }

    for (auto &sink : sinks)
    {
        sink->push(message, addInfo);
    }

    return oc::success();
}

} // namespace dplx::dlog::detail
