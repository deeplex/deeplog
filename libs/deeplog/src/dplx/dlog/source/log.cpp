
// Copyright Henrik S. Gaßmann 2023.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/source/log.hpp"

#include <dplx/dp/api.hpp>
#include <dplx/dp/items/emit_core.hpp>
#include <dplx/dp/items/emit_ranges.hpp>
#include <dplx/dp/items/item_size_of_ranges.hpp>
#include <dplx/dp/items/parse_core.hpp>
#include <dplx/dp/items/parse_ranges.hpp>
#include <dplx/scope_guard.hpp>

#include <dplx/dlog/attributes.hpp>
#include <dplx/dlog/core/log_clock.hpp>
#include <dplx/dlog/source/record_output_buffer.hpp>

auto dplx::dp::codec<dplx::dlog::detail::trivial_string_view>::size_of(
        dp::emit_context &ctx,
        dlog::detail::trivial_string_view const &str) noexcept -> std::uint64_t
{
    return dp::item_size_of_u8string(ctx, str.size);
}
auto dplx::dp::codec<dplx::dlog::detail::trivial_string_view>::encode(
        dp::emit_context &ctx,
        dlog::detail::trivial_string_view const &str) noexcept
        -> dp::result<void>
{
    return dp::emit_u8string(ctx, str.data, str.size);
}

auto dplx::dp::codec<dplx::dlog::severity>::encode(
        emit_context &ctx, dplx::dlog::severity value) noexcept -> result<void>
{
    auto const bits = static_cast<underlying_type>(value) - encoding_offset;
    if (bits > encoded_max) [[unlikely]]
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

auto dplx::dp::codec<dplx::dlog::detail::reified_status_code>::decode(
        dp::parse_context &ctx,
        dlog::detail::reified_status_code &code) noexcept -> dp::result<void>
{
    DPLX_TRY(dp::expect_item_head(ctx, dp::type_code::array, 3U));
    DPLX_TRY(dp::parse_integer(ctx, code.mDomainId));
    DPLX_TRY(dp::parse_text(ctx, code.mDomainName));
    DPLX_TRY(dp::parse_text(ctx, code.mDomainName));
    return oc::success();
}

auto dplx::dp::codec<dplx::dlog::detail::trivial_status_code_view>::size_of(
        dp::emit_context &ctx,
        dlog::detail::trivial_status_code_view const code) noexcept
        -> std::uint64_t
{
    constexpr std::uint64_t prefixSize = 1U + 9U;
    return prefixSize
         + dp::item_size_of_u8string(ctx, code.code->domain().name().size())
         + dp::item_size_of_u8string(ctx, code.code->message().size());
}

auto dplx::dp::codec<dplx::dlog::detail::trivial_status_code_view>::encode(
        dp::emit_context &ctx,
        dlog::detail::trivial_status_code_view const code) noexcept
        -> dp::result<void>
{
    DPLX_TRY(dp::emit_array(ctx, 3U));
    DPLX_TRY(dp::emit_integer(ctx, code.code->domain().id()));
    auto domainName = code.code->domain().name();
    DPLX_TRY(dp::emit_u8string(ctx, domainName.data(), domainName.size()));
    auto message = code.code->message();
    DPLX_TRY(dp::emit_u8string(ctx, message.data(), message.size()));
    return oc::success();
}

auto dplx::dp::codec<dplx::dlog::detail::reified_system_code>::decode(
        dp::parse_context &ctx,
        dlog::detail::reified_system_code &code) noexcept -> dp::result<void>
{
    DPLX_TRY(dp::expect_item_head(ctx, dp::type_code::array, 4U));
    DPLX_TRY(dp::parse_integer(ctx, code.mDomainId));
    DPLX_TRY(dp::parse_integer(ctx, code.mRawValue));
    DPLX_TRY(dp::parse_text(ctx, code.mDomainName));
    DPLX_TRY(dp::parse_text(ctx, code.mDomainName));
    return oc::success();
}

auto dplx::dp::codec<dplx::dlog::detail::trivial_system_code_view>::size_of(
        dp::emit_context &ctx,
        dlog::detail::trivial_system_code_view const code) noexcept
        -> std::uint64_t
{
    constexpr std::uint64_t prefixSize = 1U + 9U;
    return prefixSize + dp::item_size_of_integer(ctx, code.code->value())
         + dp::item_size_of_u8string(ctx, code.code->domain().name().size())
         + dp::item_size_of_u8string(ctx, code.code->message().size());
}

auto dplx::dp::codec<dplx::dlog::detail::trivial_system_code_view>::encode(
        dp::emit_context &ctx,
        dlog::detail::trivial_system_code_view const code) noexcept
        -> dp::result<void>
{
    DPLX_TRY(dp::emit_array(ctx, 4U));
    DPLX_TRY(dp::emit_integer(ctx, code.code->domain().id()));
    DPLX_TRY(dp::emit_integer(ctx, code.code->value()));
    auto domainName = code.code->domain().name();
    DPLX_TRY(dp::emit_u8string(ctx, domainName.data(), domainName.size()));
    auto message = code.code->message();
    DPLX_TRY(dp::emit_u8string(ctx, message.data(), message.size()));
    return oc::success();
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
#define DPLX_X_WITH_THUNK         0
#define DPLX_X_WITH_SYSTEM_ERROR2 1

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

#undef DPLX_X_WITH_SYSTEM_ERROR2
#undef DPLX_X_WITH_THUNK
// NOLINTEND(cppcoreguidelines-pro-type-union-access)
// NOLINTEND(cppcoreguidelines-macro-usage)

} // namespace

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto vlog(log_context const &logCtx, log_args const &args) noexcept
        -> result<void>
{
    if (args.sev == severity::none) [[unlikely]]
    {
        return oc::success();
    }
    constexpr severity severity_max{24};
    if (args.sev > severity_max) [[unlikely]]
    {
        return errc::invalid_argument;
    }

    // layout:
    // array 6
    // +  ui    severity
    // +  arr?  owner
    // +  ui64  timestamp
    // +  str   message
    // +  array format args
    // +  map   attributes

    constexpr auto numArrayElements = 6U;
    constexpr auto timestampSize = 9U;
    auto const timeStamp = log_clock::now();
    bool const hasLine = args.location.line >= 0;
    bool const hasFileName = args.location.filenameSize >= 0;
    unsigned const numAttributes = static_cast<unsigned>(hasLine)
                                 + static_cast<unsigned>(hasFileName);
    dp::void_stream voidOut;
    dp::emit_context sizeCtx{voidOut};

    // compute buffer size
    constexpr auto encodedArraySize
            = dp::encoded_item_head_size<dp::type_code::array>(
                    numArrayElements);
    constexpr auto encodedMetaSize = /*severity:*/ 1U + timestampSize;

    auto encodedSize
            = static_cast<unsigned>(encodedArraySize + encodedMetaSize);

    encodedSize += /* ctx array/tuple prefix */ 1U;
    auto const instrumentationScope = logCtx.instrumentation_scope();
    encodedSize += instrumentationScope.empty()
                         ? 0U
                         : static_cast<unsigned>(dp::item_size_of_u8string(
                                 sizeCtx, instrumentationScope.size()));
    auto const ownerId = logCtx.span();
    auto const hasOwnerSpan = ownerId.spanId != span_id::invalid();
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    encodedSize += hasOwnerSpan ? 0U : 17U + 9U;

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

    encodedSize += 1U; // map with 0-2 entries
    if (hasLine)
    {
        encodedSize += /* rid: */ 1U
                     + static_cast<unsigned>(dp::item_size_of_integer(
                             sizeCtx, args.location.line));
    }
    if (hasFileName)
    {
        encodedSize += /* rid: */ 1U
                     + static_cast<unsigned>(dp::item_size_of_u8string(
                             sizeCtx, args.location.filenameSize));
    }

    // allocate an output buffer on the message bus
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    record_output_buffer_storage outStorage;
    DPLX_TRY(auto *out, logCtx.port()->allocate_record_buffer_inplace(
                                outStorage, encodedSize, ownerId.spanId));
    record_output_guard outGuard(*out);

    dp::emit_context ctx{*out};

    // write to output buffer
    (void)dp::emit_array(ctx, numArrayElements);
    // severity
    *ctx.out.data()
            = static_cast<std::byte>(static_cast<unsigned>(args.sev) - 1U);

    // logCtx
    ctx.out.data()[1] = static_cast<std::byte>(
            static_cast<unsigned>(dp::type_code::array)
            | (instrumentationScope.data() != nullptr ? 1U : 0U)
            | (hasOwnerSpan ? 2U : 0U));
    ctx.out.commit_written(2U);
    if (instrumentationScope.data() != nullptr)
    {
        (void)dp::emit_u8string(ctx, instrumentationScope.data(),
                                instrumentationScope.size());
    }
    if (hasOwnerSpan)
    {
        (void)dp::encode(ctx, ownerId.traceId);
        (void)dp::encode(ctx, ownerId.spanId);
    }

    // timestamp
    *ctx.out.data() = static_cast<std::byte>(27U); // NOLINT
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

    DPLX_TRY(dp::emit_map(ctx, numAttributes));
    if (hasLine)
    {
        DPLX_TRY(dp::emit_integer(ctx, static_cast<unsigned>(attr::line::id)));
        DPLX_TRY(dp::emit_integer(ctx, args.location.line));
    }
    if (hasFileName)
    {
        DPLX_TRY(dp::emit_integer(ctx, static_cast<unsigned>(attr::file::id)));
        DPLX_TRY(dp::emit_u8string(
                ctx, args.location.filename,
                static_cast<std::size_t>(args.location.filenameSize)));
    }
    return oc::success();
}

} // namespace dplx::dlog::detail
