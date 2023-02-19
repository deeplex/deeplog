#include "definitions.hpp"

// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include <dplx/cncr/utils.hpp>
#include <dplx/dp/items/parse_core.hpp>

#include "dplx/dlog/definitions.hpp"

namespace dplx::dlog
{
}

// encode functions are located together with vlogger::vlog()

auto dplx::dp::codec<dplx::dlog::severity>::decode(
        parse_context &ctx, dplx::dlog::severity &outValue) noexcept
        -> result<void>
{
    underlying_type underlyingValue; // NOLINT(cppcoreguidelines-init-variables)
    if (result<void> parseRx = dp::parse_integer<underlying_type>(
                ctx, underlyingValue,
                cncr::to_underlying(dlog::severity::trace));
        parseRx.has_failure())
    {
        return static_cast<result<void> &&>(parseRx).as_failure();
    }
    outValue = static_cast<dlog::severity>(underlyingValue);
    return oc::success();
}

auto dplx::dp::codec<dplx::dlog::resource_id>::decode(
        parse_context &ctx, dplx::dlog::resource_id &outValue) noexcept
        -> result<void>
{
    underlying_type underlyingValue; // NOLINT(cppcoreguidelines-init-variables)
    if (result<void> parseRx
        = dp::parse_integer<underlying_type>(ctx, underlyingValue);
        parseRx.has_failure())
    {
        return static_cast<result<void> &&>(parseRx).as_failure();
    }
    outValue = static_cast<dlog::resource_id>(underlyingValue);
    return oc::success();
}
