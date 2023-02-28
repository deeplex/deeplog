
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/record_container.hpp"

auto dplx::dp::codec<dplx::dlog::record_location>::decode(
        parse_context &ctx, dplx::dlog::record_location &value) noexcept
        -> result<void>
{
    using namespace std::string_literals;

    int line = -1;
    if (ctx.in.empty()) [[unlikely]]
    {
        DPLX_TRY(ctx.in.require_input(1U));
    }
    if (*ctx.in.data() == static_cast<std::byte>(type_code::null))
    {
        ctx.in.discard_buffered(1U);
    }
    else
    {
        DPLX_TRY(dp::decode(ctx, line));
    }

    std::string filename = "<unknown>"s;
    if (ctx.in.empty()) [[unlikely]]
    {
        DPLX_TRY(ctx.in.require_input(1U));
    }
    if (*ctx.in.data() == static_cast<std::byte>(type_code::null))
    {
        ctx.in.discard_buffered(1U);
    }
    else
    {
        DPLX_TRY(dp::decode(ctx, filename));
    }
    value = dplx::dlog::record_location(std::move(filename), line);

    return oc::success();
}
