
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/core/strong_types.hpp"

#include <dplx/cncr/utils.hpp>
#include <dplx/dp/api.hpp>
#include <dplx/dp/items/emit_core.hpp>
#include <dplx/dp/items/parse_core.hpp>
#include <dplx/dp/items/parse_ranges.hpp>

#include <dplx/dlog/llfio.hpp>
#include <dplx/dlog/loggable.hpp>

namespace dplx::dlog
{

auto trace_id::random() noexcept -> trace_id
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    cncr::blob<char, state_size, alignof(trace_id)> bits;
    llfio::utils::random_fill(static_cast<char *>(bits.values),
                              sizeof(bits.values));
    return std::bit_cast<trace_id>(bits);
}

} // namespace dplx::dlog

////////////////////////////////////////////////////////////////////////////////
// deeppack codecs
namespace dplx::dp
{

// encode functions are located together with detail::vlog()

auto codec<dlog::severity>::decode(parse_context &ctx,
                                   dlog::severity &outValue) noexcept
        -> result<void>
{
    underlying_type underlyingValue; // NOLINT(cppcoreguidelines-init-variables)
    if (result<void> parseRx
        = dp::parse_integer<underlying_type>(ctx, underlyingValue, encoded_max);
        parseRx.has_failure())
    {
        return static_cast<result<void> &&>(parseRx).as_failure();
    }
    outValue = static_cast<dlog::severity>(underlyingValue + encoding_offset);
    return outcome::success();
}

auto codec<dlog::resource_id>::decode(parse_context &ctx,
                                      dlog::resource_id &outValue) noexcept
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
    return outcome::success();
}

auto codec<dlog::reification_type_id>::decode(
        parse_context &ctx, dlog::reification_type_id &outValue) noexcept
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
    return outcome::success();
}

auto codec<dlog::trace_id>::decode(parse_context &ctx,
                                   dlog::trace_id &id) noexcept -> result<void>
{
    DPLX_TRY(dp::expect_item_head(ctx, type_code::binary,
                                  dlog::trace_id::state_size));

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return ctx.in.bulk_read(reinterpret_cast<std::byte *>(id._state),
                            dlog::trace_id::state_size);
}

auto codec<dlog::trace_id>::encode(emit_context &ctx,
                                   dlog::trace_id id) noexcept -> result<void>
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return dp::emit_binary(ctx, reinterpret_cast<std::byte const *>(id._state),
                           dlog::trace_id::state_size);
}

auto codec<dlog::span_id>::decode(parse_context &ctx,
                                  dlog::span_id &id) noexcept -> result<void>
{
    DPLX_TRY(dp::expect_item_head(ctx, type_code::binary,
                                  dlog::span_id::state_size));

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return ctx.in.bulk_read(reinterpret_cast<std::byte *>(id._state),
                            dlog::span_id::state_size);
}
auto codec<dlog::span_id>::encode(emit_context &ctx, dlog::span_id id) noexcept
        -> result<void>
{
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return dp::emit_binary(ctx, reinterpret_cast<std::byte const *>(id._state),
                           dlog::span_id::state_size);
}

auto codec<dlog::span_context>::decode(parse_context &ctx,
                                       dlog::span_context &spanCtx) noexcept
        -> result<void>
{
    if (ctx.in.empty()) [[unlikely]]
    {
        DPLX_TRY(ctx.in.require_input(1U));
    }
    if (*ctx.in.data() == static_cast<std::byte>(type_code::null))
    {
        ctx.in.discard_buffered(1U);
        spanCtx = {};
        return outcome::success();
    }

    DPLX_TRY(dp::expect_item_head(ctx, type_code::array, 2U));
    DPLX_TRY(dp::codec<dlog::trace_id>::decode(ctx, spanCtx.traceId));
    DPLX_TRY(dp::codec<dlog::span_id>::decode(ctx, spanCtx.spanId));
    return outcome::success();
}
auto codec<dlog::span_context>::encode(emit_context &ctx,
                                       dlog::span_context spanCtx) noexcept
        -> result<void>
{
    if (spanCtx == dlog::span_context{})
    {
        return dp::emit_null(ctx);
    }

    DPLX_TRY(dp::emit_array(ctx, 2U));
    DPLX_TRY(dp::codec<dlog::trace_id>::encode(ctx, spanCtx.traceId));
    DPLX_TRY(dp::codec<dlog::span_id>::encode(ctx, spanCtx.spanId));
    return outcome::success();
}

} // namespace dplx::dp
