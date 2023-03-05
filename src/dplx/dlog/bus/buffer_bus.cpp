
// Copyright Henrik S. Gaßmann 2021-2023.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/bus/buffer_bus.hpp"

namespace dplx::dlog
{

auto bufferbus_handle::do_allocate_trace_id() noexcept -> trace_id
{
    return trace_id::random();
}
auto bufferbus_handle::do_allocate_span_id(trace_id trace) noexcept -> span_id
{
    if (trace == trace_id::invalid())
    {
        return span_id::invalid();
    }

    auto const ctr = mSpanPrngCtr++;
    auto const rawTraceId = std::bit_cast<std::array<std::uint64_t, 2>>(trace);

    return detail::derive_span_id(rawTraceId[0], rawTraceId[1], ctr);
}

} // namespace dplx::dlog
