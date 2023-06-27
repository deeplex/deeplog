
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <boost/variant2.hpp>

#include <dplx/dlog/concepts.hpp>
#include <dplx/dlog/core/log_clock.hpp>
#include <dplx/dlog/fwd.hpp>

namespace dplx::dlog
{

// NOLINTBEGIN(cppcoreguidelines-pro-type-member-init)

struct serialized_info_base
{
    bytes raw_data;
};

struct serialized_unknown_message_info : serialized_info_base
{
};
struct serialized_record_info : serialized_info_base
{
    log_clock::time_point timestamp;
    severity message_severity;
};
struct serialized_span_start_info : serialized_info_base
{
};
struct serialized_span_end_info : serialized_info_base
{
};
struct serialized_malformed_message_info : serialized_info_base
{
};

// NOLINTEND(cppcoreguidelines-pro-type-member-init)

using serialized_message_info
        = boost::variant2::variant<serialized_unknown_message_info,
                                   serialized_record_info,
                                   serialized_span_start_info,
                                   serialized_span_end_info,
                                   serialized_malformed_message_info>;

namespace detail
{

auto preparse_messages(std::span<bytes const> const &records,
                       std::span<serialized_message_info> parses) noexcept
        -> std::size_t;

void multicast_messages(std::span<std::unique_ptr<sink_frontend_base>> &sinks,
                        std::size_t binarySize,
                        std::span<serialized_message_info> parses) noexcept;

template <std::size_t MaxBatchSize>
struct consume_record_fn
{
    std::span<std::unique_ptr<sink_frontend_base>> sinks{};

    void operator()(std::span<bytes const> records) noexcept
    {
        serialized_message_info parses[MaxBatchSize];
        auto binarySize = detail::preparse_messages(records, parses);
        detail::multicast_messages(sinks, binarySize,
                                   std::span(parses).first(records.size()));
    }
};

} // namespace detail

} // namespace dplx::dlog
