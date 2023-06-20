
// Copyright Henrik Steffen Gaßmann 2021, 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstdint>

#include <fmt/core.h>

#include <dplx/dlog/arguments.hpp>
#include <dplx/dlog/definitions.hpp>
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

inline constexpr struct log_fn
{
    template <typename... Args>
        requires(... && loggable<Args>)
    [[nodiscard]] auto
    operator()(log_context const &ctx,
               severity sev,
               fmt::format_string<reification_type_of_t<Args>...> message,
               detail::log_location location,
               Args const &...args) const noexcept -> result<void>
    {
        if (sev < ctx.threshold()) [[unlikely]]
        {
            return oc::success();
        }

        return detail::vlog(ctx, detail::stack_log_args<Args...>{
                                         message, sev, location, args...});
    }
} log;

} // namespace dplx::dlog
