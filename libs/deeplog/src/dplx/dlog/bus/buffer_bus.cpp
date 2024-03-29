
// Copyright Henrik S. Gaßmann 2021-2023.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/bus/buffer_bus.hpp"

namespace dplx::dlog
{

auto bufferbus_handle::create_span_context(trace_id trace,
                                           std::string_view,
                                           severity &) noexcept -> span_context
{
    if (trace == trace_id::invalid())
    {
        trace = trace_id::random();
    }

    auto const ctr = mSpanPrngCtr++;
    auto const rawTraceId = std::bit_cast<std::array<std::uint64_t, 2>>(trace);

    return {trace, detail::derive_span_id(rawTraceId[0], rawTraceId[1], ctr)};
}

} // namespace dplx::dlog
