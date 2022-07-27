
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include <dplx/dlog/core.hpp>

#include <boost/predef/compiler.h>

#include <dplx/dp/decoder/tuple_utils.hpp>
#include <dplx/dp/item_parser.hpp>
#include <dplx/dp/skip_item.hpp>
#include <dplx/dp/streams/memory_input_stream.hpp>

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
    using parse = dp::item_parser<dp::memory_view>;

    dp::memory_view inStream(message);
    DPLX_TRY(auto &&tupleInfo, dp::parse_tuple_head(inStream));

    additional_record_info addInfo{};
    DPLX_TRY(addInfo.message_severity,
             dp::decode(dp::as_value<severity>, inStream));

    DPLX_TRY(dp::decode(inStream, addInfo.timestamp));

    DPLX_TRY(auto messageInfo, parse::generic(inStream));
    if (messageInfo.type != dp::type_code::text || messageInfo.indefinite())
    {
        return dp::errc::item_type_mismatch;
    }
    if (messageInfo.value > inStream.remaining_size())
    {
        return dp::errc::missing_data;
    }

    inStream.move_consumer(static_cast<int>(messageInfo.value));
    addInfo.message_size = messageInfo.encoded_length
                         + static_cast<unsigned>(messageInfo.value);

    if (tupleInfo.num_properties > 3)
    {
        addInfo.attributes_size = -inStream.consumed_size();

        DPLX_TRY(dp::skip_item(inStream));

        addInfo.attributes_size += inStream.consumed_size();
    }
    if (tupleInfo.num_properties > 4)
    {
        addInfo.format_args_size = -inStream.consumed_size();

        DPLX_TRY(dp::skip_item(inStream));

        addInfo.format_args_size += inStream.consumed_size();
    }

    for (auto &sink : sinks)
    {
        sink->push(message, addInfo);
    }

    return oc::success();
}

} // namespace dplx::dlog::detail
