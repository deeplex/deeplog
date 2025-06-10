
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#include <fmt/core.h>

#include <dplx/cncr/utils.hpp>
#include <dplx/dp/fwd.hpp>

#include <dplx/dlog/detail/hex.hpp>
#include <dplx/dlog/fwd.hpp>

namespace dplx::dlog
{

enum class resource_id : std::uint64_t
{
};

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

namespace detail
{
inline constexpr severity disable_threshold{0x19};
}

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

    static constexpr auto
    from_bytes(std::span<std::byte const, state_size> raw) noexcept -> trace_id
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
        alignas(trace_id) cncr::blob<std::byte, state_size> bits;
        for (std::size_t i = 0; i < state_size; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            bits.values[i] = raw[i];
        }
        return std::bit_cast<trace_id>(bits);
    }

    friend constexpr auto operator==(trace_id lhs, trace_id rhs) noexcept
            -> bool
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            = default;
};

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

    static constexpr auto
    from_bytes(std::span<std::byte const, state_size> const raw) noexcept
            -> span_id
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
        alignas(span_id) cncr::blob<std::byte, state_size> bits;
        for (std::size_t i = 0; i < state_size; ++i)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            bits.values[i] = raw[i];
        }
        return std::bit_cast<span_id>(bits);
    }

    friend constexpr auto operator==(span_id lhs, span_id rhs) noexcept -> bool
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index)
            = default;
};

struct span_context
{
    trace_id traceId;
    span_id spanId;

    friend constexpr auto operator==(span_context, span_context) noexcept
            -> bool
            = default;
};

} // namespace dplx::dlog

////////////////////////////////////////////////////////////////////////////////
// std::hash specializations
namespace std
{

template <>
struct hash<dplx::dlog::trace_id>
{
    using is_avalanching = std::true_type;
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
struct hash<dplx::dlog::span_id>
{
    using is_avalanching = std::true_type;
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

} // namespace std

////////////////////////////////////////////////////////////////////////////////
// fmt formatter specializations
namespace fmt
{

template <>
struct formatter<dplx::dlog::trace_id>
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
            dplx::dlog::detail::throw_fmt_format_error("invalid format");
        }
        return it;
    }

    template <typename FormatContext>
    static constexpr auto format(trace_id id, FormatContext &ctx)
            -> decltype(ctx.out())
    {
        using bag = dplx::cncr::blob<std::byte, trace_id::state_size,
                                     alignof(trace_id)>;
        auto const raw = std::bit_cast<bag>(id);
        return dplx::dlog::detail::hex_encode(std::begin(raw.values),
                                              std::end(raw.values), ctx.out())
                .out;
    }
};

template <>
struct formatter<dplx::dlog::span_id>
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
            dplx::dlog::detail::throw_fmt_format_error("invalid format");
        }
        return it;
    }

    template <typename FormatContext>
    static constexpr auto format(span_id id, FormatContext &ctx)
            -> decltype(ctx.out())
    {
        using bag = dplx::cncr::blob<std::byte, span_id::state_size,
                                     alignof(span_id)>;
        auto const raw = std::bit_cast<bag>(id);
        return dplx::dlog::detail::hex_encode(std::begin(raw.values),
                                              std::end(raw.values), ctx.out())
                .out;
    }
};

} // namespace fmt

////////////////////////////////////////////////////////////////////////////////
// deeppack codecs
namespace dplx::dp
{

template <>
class codec<dlog::resource_id>
{
    using underlying_type = std::underlying_type_t<dlog::resource_id>;

public:
    static auto size_of(emit_context &, dlog::resource_id) noexcept
            -> std::uint64_t;
    static auto encode(emit_context &ctx, dlog::resource_id value) noexcept
            -> result<void>;
    static auto decode(parse_context &ctx, dlog::resource_id &outValue) noexcept
            -> result<void>;
};

template <>
class codec<dlog::severity>
{
    using underlying_type = std::underlying_type_t<dlog::severity>;
    static inline constexpr underlying_type encoded_max = 23;
    static inline constexpr underlying_type encoding_offset = 1;

public:
    static constexpr auto size_of(emit_context &, dlog::severity) noexcept
            -> std::uint64_t
    {
        return 1U;
    }
    static auto encode(emit_context &ctx, dlog::severity value) noexcept
            -> result<void>;
    static auto decode(parse_context &ctx, dlog::severity &outValue) noexcept
            -> result<void>;
};

template <>
struct codec<dlog::trace_id>
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

template <>
struct codec<dlog::span_id>
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

template <>
class codec<dlog::span_context>
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

} // namespace dplx::dp
