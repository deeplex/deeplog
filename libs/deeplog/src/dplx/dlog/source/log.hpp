
// Copyright Henrik Steffen Gaßmann 2021, 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstdint>

#if __cpp_lib_source_location >= 201907L
#include <source_location>
#endif

#include <fmt/core.h>

#include <dplx/dlog/arguments.hpp>
#include <dplx/dlog/config.hpp>
#include <dplx/dlog/core/strong_types.hpp>
#include <dplx/dlog/fwd.hpp>
#include <dplx/dlog/loggable.hpp>
#include <dplx/dlog/source/log_context.hpp>

namespace dplx::dlog
{

namespace detail
{

auto vlog(log_context const &logCtx, log_args const &args) noexcept
        -> result<void>;

} // namespace detail

template <typename... Args>
    requires(... && loggable<Args>)
[[nodiscard]] inline auto log(
        log_context const &ctx,
        severity sev,
        fmt::format_string<reification_type_of_t<Args>...> message,
#if DPLX_DLOG_USE_SOURCE_LOCATION
        std::source_location const &location,
#else
        detail::log_location const &location,
#endif
        Args const &...args) noexcept -> result<void>
{
    if (sev < ctx.threshold()) [[unlikely]]
    {
        return oc::success();
    }

    return detail::vlog(ctx, detail::stack_log_args<Args...>{
                                     message, sev, location, args...});
}

} // namespace dplx::dlog
