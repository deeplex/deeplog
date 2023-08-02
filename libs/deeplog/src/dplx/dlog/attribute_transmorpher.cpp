
// Copyright 2023 Henrik Steffen Gaßmann
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/attribute_transmorpher.hpp"

#include <dplx/dp/api.hpp>
#include <dplx/dp/codecs/core.hpp>
#include <dplx/dp/items/emit_ranges.hpp>
#include <dplx/dp/items/item_size_of_ranges.hpp>
#include <dplx/dp/items/parse_ranges.hpp>

namespace dplx::dlog
{

auto attribute_type_registry::decode(dp::parse_context &ctx) const noexcept
        -> result<any_attribute>
{
    DPLX_TRY(auto const key, dp::decode(dp::as_value<key_type>, ctx));

    auto const it = mKnownTypes.find(key);
    if (it == mKnownTypes.end())
    {
        return errc::unknown_attribute_type_id;
    }

    revive_fn reviveAttribute = it->second;
    return reviveAttribute(ctx);
}

} // namespace dplx::dlog

auto dplx::dp::codec<dplx::dlog::attribute_container>::size_of(
        emit_context &ctx,
        dplx::dlog::attribute_container const &attrs) noexcept -> std::uint64_t
{
    return dp::item_size_of_map(
            ctx, attrs.mAttributes,
            [](emit_context &lctx, auto &idAttrPair) noexcept -> std::uint64_t
            { return dp::encoded_size_of(lctx, idAttrPair.second); });
}

auto dplx::dp::codec<dplx::dlog::attribute_container>::encode(
        emit_context &ctx,
        dplx::dlog::attribute_container const &attrs) noexcept -> result<void>
{
    return dp::emit_map(
            ctx, attrs.mAttributes,
            [](emit_context &lctx, auto &idAttrPair) noexcept -> result<void>
            { return dp::encode(lctx, idAttrPair.second); });
}

auto dplx::dp::codec<dplx::dlog::attribute_container>::decode(
        parse_context &ctx, dplx::dlog::attribute_container &outValue) noexcept
        -> result<void>
{
    outValue.mAttributes.clear();
    if (ctx.states.try_access(dlog::attribute_type_registry_state) == nullptr)
    {
        return dp::expect_item_head(ctx, type_code::map, 0U);
    }

    auto parseRx = dp::parse_map_finite(
            ctx, outValue.mAttributes,
            [](dp::parse_context &lctx,
               dlog::attribute_container::map_type &store,
               std::size_t const) noexcept -> result<void>
            {
                auto *registry = lctx.states.try_access(
                        dlog::attribute_type_registry_state);
                DPLX_TRY(auto &&attr, registry->decode(lctx));
                try
                {
                    if (!store.try_emplace(attr.id(),
                                           static_cast<decltype(attr) &&>(attr))
                                 .second)
                    {
                        // ignore duplicate keys
                        return outcome::success();
                    }
                }
                catch (std::bad_alloc const &)
                {
                    return errc::not_enough_memory;
                }

                return outcome::success();
            });
    if (parseRx.has_failure()) [[unlikely]]
    {
        return static_cast<decltype(parseRx) &&>(parseRx).as_failure();
    }
    return outcome::success();
}
