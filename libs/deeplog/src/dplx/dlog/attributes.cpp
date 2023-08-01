
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/attributes.hpp"

#include <dplx/dp/api.hpp>
#include <dplx/dp/items/emit_core.hpp>
#include <dplx/dp/items/emit_ranges.hpp>
#include <dplx/dp/items/item_size_of_ranges.hpp>
#include <dplx/scope_guard.hpp>

#include <dplx/dlog/core/log_clock.hpp>

namespace dplx::dlog::detail
{

namespace
{

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
#define DPLX_X_WITH_THUNK         0
#define DPLX_X_WITH_SYSTEM_ERROR2 1

inline auto item_size_of_any_attribute(dp::emit_context &ctx,
                                       any_loggable_ref_storage_id id,
                                       any_loggable_ref_storage const &value,
                                       resource_id attributeId) noexcept
        -> std::uint64_t
{
    using enum any_loggable_ref_storage_id;
    using enum erased_loggable_thunk_mode;

    switch (id)
    {
    case null:
        break;
#define DPLX_X(name, type, var)                                                \
    case name:                                                                 \
        return dp::encoded_size_of(ctx, attributeId)                           \
             + dp::encoded_size_of(ctx, value.var);                            \
        break;
#include <dplx/dlog/detail/x_poly_types.inl>
#undef DPLX_X
    case thunk:
        return dp::encoded_size_of(ctx, attributeId)
             + value.thunk.func(value.thunk.self, size_of_raw, ctx)
                       .assume_value();
        break;
    case LIMIT:
    default:
        cncr::unreachable();
    }

    return 0U;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
inline auto encode_any_attribute(dp::emit_context &ctx,
                                 any_loggable_ref_storage_id id,
                                 any_loggable_ref_storage const &value,
                                 resource_id attributeId) noexcept
        -> result<void>
{
    using enum any_loggable_ref_storage_id;
    using enum erased_loggable_thunk_mode;

    switch (id)
    {
    case null:
        break;
#define DPLX_X(name, type, var)                                                \
    case name:                                                                 \
    {                                                                          \
        DPLX_TRY(dp::encode(ctx, attributeId));                                \
        DPLX_TRY(dp::encode(ctx, value.var));                                  \
        break;                                                                 \
    }
#include <dplx/dlog/detail/x_poly_types.inl>
#undef DPLX_X
    case thunk:
        if (auto &&encodeRx = dp::encode(ctx, attributeId);
            encodeRx.has_error())
        {
            return static_cast<decltype(encodeRx) &&>(encodeRx).assume_error();
        }
        if (auto &&encodeRx
            = value.thunk.func(value.thunk.self, encode_raw, ctx);
            encodeRx.has_error())
        {
            return static_cast<decltype(encodeRx) &&>(encodeRx).assume_error();
        }
        break;
    case LIMIT:
    default:
        cncr::unreachable();
    }

    return outcome::success();
}

#undef DPLX_X_WITH_SYSTEM_ERROR2
#undef DPLX_X_WITH_THUNK
// NOLINTEND(cppcoreguidelines-pro-type-union-access)
// NOLINTEND(cppcoreguidelines-macro-usage)

} // namespace

auto encoded_size_of_attributes(dp::emit_context &ctx,
                                attribute_args const &attrs) noexcept
        -> std::uint64_t
{
    std::uint_fast16_t const numAttributes{attrs.num_attributes};
    std::uint64_t accumulator = 0U;
    for (std::uint_fast16_t i = 0U; i < numAttributes; ++i)
    {
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        accumulator += item_size_of_any_attribute(ctx, attrs.attribute_types[i],
                                                  attrs.attributes[i],
                                                  attrs.ids[i]);
        // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }
    return accumulator;
}
auto encode_attributes(dp::emit_context &ctx,
                       attribute_args const &attrs) noexcept -> dp::result<void>
{
    std::uint_fast16_t const numAttributes{attrs.num_attributes};
    for (std::uint_fast16_t i = 0; i < numAttributes; ++i)
    {
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        DPLX_TRY(encode_any_attribute(ctx, attrs.attribute_types[i],
                                      attrs.attributes[i], attrs.ids[i]));
        // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }
    return outcome::success();
}

} // namespace dplx::dlog::detail

auto dplx::dp::codec<dplx::dlog::detail::attribute_args>::size_of(
        emit_context &ctx, value_type const &attrs) noexcept -> std::uint64_t
{
    return dp::encoded_item_head_size<type_code::map>(attrs.num_attributes)
         + dlog::detail::encoded_size_of_attributes(ctx, attrs);
}
auto dplx::dp::codec<dplx::dlog::detail::attribute_args>::encode(
        emit_context &ctx, value_type const &attrs) noexcept -> result<void>
{
    DPLX_TRY(dp::emit_map(ctx, attrs.num_attributes));
    return dlog::detail::encode_attributes(ctx, attrs);
}
