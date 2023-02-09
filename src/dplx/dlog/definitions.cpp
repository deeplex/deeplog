#include "definitions.hpp"

// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include <dplx/cncr/utils.hpp>
#include <dplx/dp/items/emit_core.hpp>
#include <dplx/dp/items/item_size_of_core.hpp>
#include <dplx/dp/items/parse_core.hpp>

#include "dplx/dlog/definitions.hpp"

namespace dplx::dlog
{
}

auto dplx::dp::codec<dplx::dlog::severity>::encode(
        emit_context const &ctx, dplx::dlog::severity value) noexcept
        -> result<void>
{
    auto const bits = cncr::to_underlying(value);
    if (bits > cncr::to_underlying(dlog::severity::trace))
    {
        return errc::item_value_out_of_range;
    }
    if (ctx.out.size() < 1U)
    {
        DPLX_TRY(ctx.out.ensure_size(1U));
    }
    *ctx.out.data() = static_cast<std::byte>(bits);
    ctx.out.commit_written(1U);
    return oc ::success();
}

auto dplx::dp::codec<dplx::dlog::severity>::decode(
        parse_context &ctx, dplx::dlog::severity &outValue) noexcept
        -> result<void>
{
    underlying_type underlyingValue;
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

auto dplx::dp::codec<dplx::dlog::resource_id>::size_of(
        emit_context const &, dplx::dlog::resource_id value) noexcept
        -> std::uint64_t
{
    return dp::encoded_item_head_size<type_code::posint>(
            cncr::to_underlying(value));
}

auto dplx::dp::codec<dplx::dlog::resource_id>::encode(
        emit_context const &ctx, dplx::dlog::resource_id value) noexcept
        -> result<void>
{
    return dp::emit_integer(ctx, cncr::to_underlying(value));
}

auto dplx::dp::codec<dplx::dlog::resource_id>::decode(
        parse_context &ctx, dplx::dlog::resource_id &outValue) noexcept
        -> result<void>
{
    underlying_type underlyingValue;
    if (result<void> parseRx
        = dp::parse_integer<underlying_type>(ctx, underlyingValue);
        parseRx.has_failure())
    {
        return static_cast<result<void> &&>(parseRx).as_failure();
    }
    outValue = static_cast<dlog::resource_id>(underlyingValue);
    return oc::success();
}
