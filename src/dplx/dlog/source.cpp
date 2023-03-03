
// Copyright Henrik S. Gaßmann 2023.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/source.hpp"

#include <dplx/dp/items/emit_core.hpp>
#include <dplx/dp/items/emit_ranges.hpp>
#include <dplx/dp/items/item_size_of_ranges.hpp>
#include <dplx/scope_guard.hpp>

#include <dplx/dlog/attributes.hpp> // FIXME
#include <dplx/dlog/log_clock.hpp>

template <>
class dplx::dp::codec<dplx::dlog::detail::trivial_string_view>
{
public:
    static auto size_of(dp::emit_context &ctx,
                        dlog::detail::trivial_string_view const &str) noexcept
            -> std::uint64_t
    {
        return dp::item_size_of_u8string(ctx, str.size);
    }
    static auto encode(dp::emit_context &ctx,
                       dlog::detail::trivial_string_view const &str) noexcept
            -> dp::result<void>
    {
        return dp::emit_u8string(ctx, str.data, str.size);
    }
};

auto dplx::dp::codec<dplx::dlog::severity>::encode(
        emit_context &ctx, dplx::dlog::severity value) noexcept -> result<void>
{
    auto const bits = cncr::to_underlying(value);
    if (bits > cncr::to_underlying(dlog::severity::trace)) [[unlikely]]
    {
        return errc::item_value_out_of_range;
    }
    if (ctx.out.empty()) [[unlikely]]
    {
        DPLX_TRY(ctx.out.ensure_size(1U));
    }
    *ctx.out.data() = static_cast<std::byte>(bits);
    ctx.out.commit_written(1U);
    return oc::success();
}

auto dplx::dp::codec<dplx::dlog::resource_id>::size_of(
        emit_context &, dplx::dlog::resource_id value) noexcept -> std::uint64_t
{
    return dp::encoded_item_head_size<type_code::posint>(
            cncr::to_underlying(value));
}
auto dplx::dp::codec<dplx::dlog::resource_id>::encode(
        emit_context &ctx, dplx::dlog::resource_id value) noexcept
        -> result<void>
{
    return dp::emit_integer(ctx, cncr::to_underlying(value));
}

auto dplx::dp::codec<dplx::dlog::reification_type_id>::size_of(
        emit_context &, dplx::dlog::reification_type_id value) noexcept
        -> std::uint64_t
{
    return dp::encoded_item_head_size<type_code::posint>(
            cncr::to_underlying(value));
}
auto dplx::dp::codec<dplx::dlog::reification_type_id>::encode(
        emit_context &ctx, dplx::dlog::reification_type_id value) noexcept
        -> result<void>
{
    return dp::emit_integer(ctx, cncr::to_underlying(value));
}

auto dplx::dlog::detail::erased_loggable_ref::emit_reification_prefix(
        dp::emit_context &ctx, reification_type_id id) noexcept
        -> result<std::uint64_t>
{
    if (dp::result<void> emitRx = dp::emit_array(ctx, 2U); emitRx.has_error())
            [[unlikely]]
    {
        return static_cast<decltype(emitRx) &&>(emitRx).assume_error();
    }
    if (dp::result<void> encodeRx = dp::encode(ctx, id); encodeRx.has_error())
            [[unlikely]]
    {
        return static_cast<decltype(encodeRx) &&>(encodeRx).assume_error();
    }
    return 0U;
}

namespace dplx::dlog::detail
{

namespace
{

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
#define DPLX_X_WITH_THUNK 0

inline auto
item_size_of_any_loggable(dp::emit_context &ctx,
                          any_loggable_ref_storage_id id,
                          any_loggable_ref_storage const &value) noexcept
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
        return 1U + dp::encoded_size_of(ctx, as_reification_id(name))          \
             + dp::encoded_size_of(ctx, value.var);                            \
        break;
#include <dplx/dlog/detail/x_poly_types.inl>
#undef DPLX_X
    case thunk:
        return value.thunk.func(value.thunk.self, size_of, ctx).assume_value();
        break;
    case LIMIT:
    default:
        cncr::unreachable();
    }

    return 0U;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
inline auto encode_any_loggable(dp::emit_context &ctx,
                                any_loggable_ref_storage_id id,
                                any_loggable_ref_storage const &value) noexcept
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
        DPLX_TRY(dp::emit_array(ctx, 2U));                                     \
        DPLX_TRY(dp::encode(ctx, as_reification_id(name)));                    \
        DPLX_TRY(dp::encode(ctx, value.var));                                  \
        break;                                                                 \
    }
#include <dplx/dlog/detail/x_poly_types.inl>
#undef DPLX_X
    case thunk:
        if (auto &&encodeRx = value.thunk.func(value.thunk.self, encode, ctx);
            encodeRx.has_error())
        {
            return static_cast<decltype(encodeRx) &&>(encodeRx).assume_error();
        }
        break;
    case LIMIT:
    default:
        cncr::unreachable();
    }

    return oc::success();
}

#undef DPLX_X_WITH_THUNK
// NOLINTEND(cppcoreguidelines-pro-type-union-access)
// NOLINTEND(cppcoreguidelines-macro-usage)

} // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto vlog(bus_handle &messageBus, log_args const &args) noexcept -> result<void>
{
    if (static_cast<unsigned>(args.sev)
        > static_cast<unsigned>(severity::trace))
    {
        return errc::invalid_argument;
    }

    // layout:
    // array 6
    // +  ui    severity
    // +  ui64  timestamp
    // +  str   message
    // +  array format args
    // +  ui?   line
    // +  str?  file

    constexpr auto numArrayElements = 5U;
    constexpr auto timestampSize = 9U;
    auto const timeStamp = log_clock::now();
    dp::void_stream voidOut;
    dp::emit_context sizeCtx{voidOut};

    // compute buffer size
    constexpr auto encodedArraySize
            = dp::encoded_item_head_size<dp::type_code::array>(
                    numArrayElements);
    constexpr auto encodedMetaSize = /*severity:*/ 1U + timestampSize;

    auto encodedSize
            = static_cast<unsigned>(encodedArraySize + encodedMetaSize);

    encodedSize += static_cast<unsigned>(
            dp::item_size_of_u8string(sizeCtx, args.message.size()));

    encodedSize += static_cast<unsigned>(
            dp::encoded_item_head_size<dp::type_code::array>(
                    args.num_arguments));
    for (int i = 0; i < args.num_arguments; ++i)
    {
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        encodedSize += static_cast<unsigned>(detail::item_size_of_any_loggable(
                sizeCtx, args.part_types[i], args.message_parts[i]));
        // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }

    if (args.location.line >= 0)
    {
        encodedSize += static_cast<unsigned>(
                dp::item_size_of_integer(sizeCtx, args.location.line));
    }
    else
    {
        encodedSize += 1U; /*null*/
    }
    if (args.location.filenameSize >= 0)
    {
        encodedSize += static_cast<unsigned>(
                dp::item_size_of_u8string(sizeCtx, args.location.filenameSize));
    }
    else
    {
        encodedSize += 1U; /*null*/
    }

    // allocate an output buffer on the message bus
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    dlog::output_buffer_storage outStorage;
    DPLX_TRY(auto *out, messageBus.create_output_buffer_inplace(
                                outStorage, encodedSize, args.owner));
    bus_output_guard outGuard(*out);

    dp::emit_context ctx{*out};

    // write to output buffer
    (void)dp::emit_array(ctx, numArrayElements);
    // severity
    *ctx.out.data() = static_cast<std::byte>(args.sev);
    ctx.out.commit_written(1U);

    // timestamp
    *ctx.out.data() = static_cast<std::byte>(dp::type_code::posint);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    dp::detail::store(ctx.out.data() + 1, timeStamp.time_since_epoch().count());
    ctx.out.commit_written(timestampSize);

    // message
    (void)dp::emit_u8string(ctx, args.message.data(), args.message.size());

    (void)dp::emit_array<unsigned>(ctx, args.num_arguments);
    for (int i = 0; i < args.num_arguments; ++i)
    {
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        DPLX_TRY(detail::encode_any_loggable(ctx, args.part_types[i],
                                             args.message_parts[i]));
        // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }

    if (args.location.line >= 0)
    {
        DPLX_TRY(dp::emit_integer(ctx, args.location.line));
    }
    else
    {
        DPLX_TRY(dp::emit_null(ctx));
    }
    if (args.location.filenameSize >= 0)
    {
        DPLX_TRY(dp::emit_u8string(
                ctx, args.location.filename,
                static_cast<std::size_t>(args.location.filenameSize)));
    }
    else
    {
        DPLX_TRY(dp::emit_null(ctx));
    }
    return oc::success();
}

} // namespace dplx::dlog::detail