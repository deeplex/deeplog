
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/core/serialized_messages.hpp"

#include <algorithm>
#include <span>

#include <dplx/dp/api.hpp>
#include <dplx/dp/codecs/auto_tuple.hpp>
#include <dplx/dp/items/skip_item.hpp>
#include <dplx/dp/streams/memory_input_stream.hpp>

#include <dplx/dlog/core/strong_types.hpp>
#include <dplx/dlog/sinks/sink_frontend.hpp>

namespace dplx::dlog::detail
{

namespace
{

auto preparse_record(dp::parse_context &ctx, bytes const rawMessage) noexcept
        -> serialized_message_info
{
    serialized_message_info info{
            serialized_record_info{{rawMessage}, {}, {}}
    };
    auto &parsed = *get_if<serialized_record_info>(&info);

    if (dp::decode(ctx, parsed.message_severity).has_failure()) [[unlikely]]
    {
        info = serialized_malformed_message_info{{rawMessage}};
    }
    if (dp::skip_item(ctx).has_failure()) [[unlikely]]
    {
        info = serialized_malformed_message_info{{rawMessage}};
    }
    if (dp::decode(ctx, parsed.timestamp).has_failure()) [[unlikely]]
    {
        info = serialized_malformed_message_info{{rawMessage}};
    }
    for (int i = 3; i < 6; ++i) // NOLINT(cppcoreguidelines-avoid-magic-numbers)
    {
        if (dp::skip_item(ctx).has_failure()) [[unlikely]]
        {
            info = serialized_malformed_message_info{{rawMessage}};
        }
    }
    parsed.raw_data = rawMessage.first(rawMessage.size() - ctx.in.size());
    return info;
}
auto preparse_span_start(dp::parse_context &ctx,
                         bytes const rawMessage) noexcept
        -> serialized_message_info
{
    serialized_message_info info{serialized_span_start_info{{rawMessage}}};
    auto &parsed = *get_if<serialized_span_start_info>(&info);
    for (int i = 0; i < 7; ++i) // NOLINT(cppcoreguidelines-avoid-magic-numbers)
    {
        if (dp::skip_item(ctx).has_failure()) [[unlikely]]
        {
            info = serialized_malformed_message_info{{rawMessage}};
        }
    }
    parsed.raw_data = rawMessage.first(rawMessage.size() - ctx.in.size());
    return info;
}
auto preparse_span_end(dp::parse_context &ctx, bytes const rawMessage) noexcept
        -> serialized_message_info
{
    serialized_message_info info{serialized_span_end_info{{rawMessage}}};
    auto &parsed = *get_if<serialized_span_end_info>(&info);
    for (int i = 0; i < 2; ++i) // NOLINT(cppcoreguidelines-avoid-magic-numbers)
    {
        if (dp::skip_item(ctx).has_failure()) [[unlikely]]
        {
            info = serialized_malformed_message_info{{rawMessage}};
        }
    }
    parsed.raw_data = rawMessage.first(rawMessage.size() - ctx.in.size());
    return info;
}

} // namespace

auto preparse_messages(std::span<bytes const> const &records,
                       std::span<serialized_message_info> parses) noexcept
        -> std::size_t
{
    std::size_t binarySize = 0U;
    for (std::size_t i = 0, limit = records.size(); i < limit; ++i)
    {
        binarySize += records[i].size();
        auto &info = (parses[i] = serialized_message_info{});
        auto &&buffer = dp::get_input_buffer(bytes(records[i]));
        dp::parse_context ctx{buffer};

        if (auto decodeTupleHeadRx = dp::decode_tuple_head(ctx);
            decodeTupleHeadRx.has_value())
        {
            switch (decodeTupleHeadRx.assume_value().num_properties)
            {
            case 6U: // NOLINT(cppcoreguidelines-avoid-magic-numbers)
                info = detail::preparse_record(ctx, records[i]);
                break;

            case 7U: // NOLINT(cppcoreguidelines-avoid-magic-numbers)
                info = detail::preparse_span_start(ctx, records[i]);
                break;
            case 2U: // NOLINT(cppcoreguidelines-avoid-magic-numbers)
                info = detail::preparse_span_end(ctx, records[i]);
                break;

            default:
                info = serialized_malformed_message_info{{records[i]}};
                continue;
            }
            binarySize -= ctx.in.size();
        }
        else
        {
            info = serialized_malformed_message_info{{records[i]}};
        }
    }
    return binarySize;
}

void multicast_messages(
        std::span<std::unique_ptr<sink_frontend_base>> &sinks,
        std::size_t const binarySize,
        std::span<serialized_message_info> const parses) noexcept
{
    auto begin = sinks.begin();
    auto newEnd
            = std::partition(begin, sinks.end(),
                             [parses, binarySize](auto &sink)
                             { return sink->try_consume(binarySize, parses); });

    sinks = std::span<std::unique_ptr<sink_frontend_base>>(begin, newEnd);
}

} // namespace dplx::dlog::detail
