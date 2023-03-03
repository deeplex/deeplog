
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/definitions.hpp"

#include <dplx/cncr/utils.hpp>
#include <dplx/dp/items/emit_core.hpp>
#include <dplx/dp/items/parse_core.hpp>
#include <dplx/dp/items/parse_ranges.hpp>

#include <dplx/dlog/llfio.hpp>
#include <dplx/dlog/loggable.hpp>

namespace dplx::dlog
{

auto trace_id::random() noexcept -> trace_id
{
    struct // NOLINT(cppcoreguidelines-pro-type-member-init)
    {
        alignas(trace_id) char bytes[state_size];
    } value;
    llfio::utils::random_fill(static_cast<char *>(value.bytes),
                              sizeof(value.bytes));
    return std::bit_cast<trace_id>(value);
}

} // namespace dplx::dlog

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

auto dplx::dp::codec<dplx::dlog::reification_type_id>::decode(
        parse_context &ctx, dplx::dlog::reification_type_id &outValue) noexcept
        -> result<void>
{
    underlying_type underlyingValue; // NOLINT(cppcoreguidelines-init-variables)
    if (result<void> parseRx
        = dp::parse_integer<underlying_type>(ctx, underlyingValue);
        parseRx.has_failure())
    {
        return static_cast<result<void> &&>(parseRx).as_failure();
    }
    outValue = static_cast<dlog::reification_type_id>(underlyingValue);
    return oc::success();
}

auto dplx::dp::codec<dplx::dlog::trace_id>::decode(parse_context &ctx,
                                                   dlog::trace_id &id) noexcept
        -> result<void>
{
    DPLX_TRY(dp::expect_item_head(ctx, type_code::binary,
                                  dlog::trace_id::state_size));

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return ctx.in.bulk_read(reinterpret_cast<std::byte *>(id._state),
                            dlog::trace_id::state_size);
}
auto dplx::dp::codec<dplx::dlog::trace_id>::encode(emit_context &ctx,
                                                   dlog::trace_id id) noexcept
        -> result<void>
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return dp::emit_binary(ctx, reinterpret_cast<std::byte const *>(id._state),
                           dlog::trace_id::state_size);
}

auto dplx::dp::codec<dplx::dlog::span_id>::decode(parse_context &ctx,
                                                  dlog::span_id &id) noexcept
        -> result<void>
{
    DPLX_TRY(dp::expect_item_head(ctx, type_code::binary,
                                  dlog::span_id::state_size));

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return ctx.in.bulk_read(reinterpret_cast<std::byte *>(id._state),
                            dlog::span_id::state_size);
}
auto dplx::dp::codec<dplx::dlog::span_id>::encode(emit_context &ctx,
                                                  dlog::span_id id) noexcept
        -> result<void>
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return dp::emit_binary(ctx, reinterpret_cast<std::byte const *>(id._state),
                           dlog::span_id::state_size);
}
