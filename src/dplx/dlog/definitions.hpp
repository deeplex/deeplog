
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>

#include <dplx/dp/disappointment.hpp>
#include <dplx/dp/fwd.hpp>

// span aliasse
namespace dplx::dlog
{

using bytes = std::span<std::byte const>;
using writable_bytes = std::span<std::byte>;

} // namespace dplx::dlog

// resource id
namespace dplx::dlog
{

enum class resource_id : std::uint64_t
{
};

} // namespace dplx::dlog

// severity
namespace dplx::dlog
{

enum class severity : unsigned
{
    none = 0,
    critical = 1,
    error = 2,
    warn = 3,
    info = 4,
    debug = 5,
    trace = 6,
};

inline constexpr severity default_threshold = severity::warn;

} // namespace dplx::dlog

template <>
class dplx::dp::codec<dplx::dlog::severity>
{
    using underlying_type = std::underlying_type_t<dlog::severity>;

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
