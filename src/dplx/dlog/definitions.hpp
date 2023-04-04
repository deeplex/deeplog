
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <bit>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>

#include <fmt/core.h>

#include <dplx/dp/disappointment.hpp>
#include <dplx/dp/fwd.hpp>

#include <dplx/dlog/detail/hex.hpp>

// span aliasse
namespace dplx::dlog
{

using bytes = std::span<std::byte const>;
using writable_bytes = std::span<std::byte>;

namespace detail
{

template <std::size_t Size>
struct byte_bag
{
    static constexpr std::size_t static_size = Size;
    std::byte bytes[Size];
};

} // namespace detail

} // namespace dplx::dlog

// resource id
namespace dplx::dlog
{

enum class resource_id : std::uint64_t
{
};

} // namespace dplx::dlog

template <>
class dplx::dp::codec<dplx::dlog::resource_id>
{
    using underlying_type = std::underlying_type_t<dlog::resource_id>;

public:
    static auto size_of(emit_context &, dplx::dlog::resource_id) noexcept
            -> std::uint64_t;
    static auto encode(emit_context &ctx,
                       dplx::dlog::resource_id value) noexcept -> result<void>;
    static auto decode(parse_context &ctx,
                       dplx::dlog::resource_id &outValue) noexcept
            -> result<void>;
};

// severity
namespace dplx::dlog
{

enum class severity : unsigned
{
    none = 0,
    trace = 1,
    debug = 5,
    info = 9,
    warn = 13,
    error = 17,
    fatal = 21,
};

inline constexpr severity default_threshold = severity::warn;

} // namespace dplx::dlog

template <>
class dplx::dp::codec<dplx::dlog::severity>
{
    using underlying_type = std::underlying_type_t<dlog::severity>;
    static inline constexpr underlying_type encoded_max = 23;
    static inline constexpr underlying_type encoding_offset = 1;

public:
    static constexpr auto size_of(emit_context &, dplx::dlog::severity) noexcept
            -> std::uint64_t
    {
        return 1U;
    }
    static auto encode(emit_context &ctx, dplx::dlog::severity value) noexcept
            -> result<void>;
    static auto decode(parse_context &ctx,
                       dplx::dlog::severity &outValue) noexcept -> result<void>;
};

namespace dplx::dlog
{

struct trace_id
{
    static constexpr std::size_t state_size = 16;

    std::size_t _state[state_size / sizeof(std::size_t)];
    static_assert(sizeof(_state) == state_size);

    static auto random() noexcept -> trace_id;
    static consteval auto invalid() noexcept -> trace_id
    {
        return {};
    }

    static auto from_bytes(std::span<std::byte const, state_size> raw) noexcept
            -> trace_id
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
        alignas(std::size_t) detail::byte_bag<state_size> value;
        for (std::size_t i = 0; i < state_size; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            value.bytes[i] = raw[i];
        }
        return std::bit_cast<trace_id>(value);
    }

    friend inline auto operator==(trace_id lhs, trace_id rhs) noexcept -> bool
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            = default;
};

} // namespace dplx::dlog

template <>
struct std::hash<dplx::dlog::trace_id>
{
    using is_avalanching = void;
    using value_type = dplx::dlog::trace_id;

    auto operator()(value_type const value) const noexcept -> std::size_t
    {
        if constexpr (sizeof(std::size_t) == sizeof(std::uint64_t))
        {
            return value._state[0] ^ value._state[1];
        }
        else if constexpr (sizeof(std::size_t) == sizeof(std::uint32_t))
        {
            return value._state[0] ^ value._state[1] ^ value._state[2]
                 ^ value._state[3];
        }
        else
        {
            return value._state[0];
        }
    }
};

template <>
struct fmt::formatter<dplx::dlog::trace_id>
{
private:
    using trace_id = dplx::dlog::trace_id;

public:
    static constexpr auto parse(format_parse_context &ctx)
            -> decltype(ctx.begin())
    {
        auto &&it = ctx.begin();
        if (it != ctx.end() && *it != '}')
        {
            ctx.on_error("invalid format");
        }
        return it;
    }

    template <typename FormatContext>
    static constexpr auto format(trace_id id, FormatContext &ctx)
            -> decltype(ctx.out())
    {
        using bag = dplx::dlog::detail::byte_bag<trace_id::state_size>;
        auto const raw = std::bit_cast<bag>(id);
        return dplx::dlog::detail::hex_encode(std::begin(raw.bytes),
                                              std::end(raw.bytes), ctx.out())
                .out;
    }
};

template <>
struct dplx::dp::codec<dplx::dlog::trace_id>
{
    static auto decode(parse_context &ctx, dlog::trace_id &id) noexcept
            -> result<void>;
    static constexpr auto size_of(emit_context &, dlog::trace_id) noexcept
            -> std::uint64_t
    {
        return 1U + dlog::trace_id::state_size;
    }
    static auto encode(emit_context &ctx, dlog::trace_id id) noexcept
            -> result<void>;
};

namespace dplx::dlog
{

struct span_id
{
    static constexpr std::size_t state_size = 8;

    // this member is public due to the stupid Windows x64 calling convention
    std::size_t _state[state_size / sizeof(std::size_t)];
    static_assert(sizeof(_state) == state_size);

    static consteval auto invalid() noexcept -> span_id
    {
        return {};
    }

    static auto
    from_bytes(std::span<std::byte const, state_size> const raw) noexcept
            -> span_id
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
        alignas(std::size_t) detail::byte_bag<state_size> value;
        for (std::size_t i = 0; i < state_size; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            value.bytes[i] = raw[i];
        }
        return std::bit_cast<span_id>(value);
    }

    friend inline auto operator==(span_id lhs, span_id rhs) noexcept -> bool
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            = default;
};

} // namespace dplx::dlog

template <>
struct std::hash<dplx::dlog::span_id>
{
    using is_avalanching = void;
    using value_type = dplx::dlog::span_id;

    auto operator()(value_type const value) const noexcept -> std::size_t
    {
        if constexpr (sizeof(std::size_t) == sizeof(std::uint32_t))
        {
            return value._state[0] ^ value._state[1];
        }
        else
        {
            return value._state[0];
        }
    }
};

template <>
struct fmt::formatter<dplx::dlog::span_id>
{
private:
    using span_id = dplx::dlog::span_id;

public:
    static constexpr auto parse(format_parse_context &ctx)
            -> decltype(ctx.begin())
    {
        auto &&it = ctx.begin();
        if (it != ctx.end() && *it != '}')
        {
            ctx.on_error("invalid format");
        }
        return it;
    }

    template <typename FormatContext>
    static constexpr auto format(span_id id, FormatContext &ctx)
            -> decltype(ctx.out())
    {
        using bag = dplx::dlog::detail::byte_bag<span_id::state_size>;
        auto const raw = std::bit_cast<bag>(id);
        return dplx::dlog::detail::hex_encode(std::begin(raw.bytes),
                                              std::end(raw.bytes), ctx.out())
                .out;
    }
};

template <>
struct dplx::dp::codec<dplx::dlog::span_id>
{
    static auto decode(parse_context &ctx, dlog::span_id &id) noexcept
            -> result<void>;
    static constexpr auto size_of(emit_context &, dlog::span_id) noexcept
            -> std::uint64_t
    {
        return 1U + dlog::span_id::state_size;
    }
    static auto encode(emit_context &ctx, dlog::span_id id) noexcept
            -> result<void>;
};

namespace dplx::dlog
{

struct span_context
{
    trace_id traceId;
    span_id spanId;

    friend inline auto operator==(span_context, span_context) noexcept -> bool
            = default;
};

} // namespace dplx::dlog

template <>
class dplx::dp::codec<dplx::dlog::span_context>
{
public:
    static auto decode(parse_context &ctx, dlog::span_context &spanCtx) noexcept
            -> result<void>;
    static constexpr auto size_of(emit_context &ctx,
                                  dlog::span_context spanCtx) noexcept
            -> std::uint64_t
    {
        return spanCtx == dlog::span_context{}
                     ? 1U
                     : 1U + codec<dlog::trace_id>::size_of(ctx, {})
                               + codec<dlog::span_id>::size_of(ctx, {});
    }
    static auto encode(emit_context &ctx, dlog::span_context spanCtx) noexcept
            -> result<void>;
};
