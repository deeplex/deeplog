
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
#include <dplx/dlog/detail/tls.hpp>
#include <dplx/dlog/fwd.hpp>
#include <dplx/dlog/loggable.hpp>
#include <dplx/dlog/span_scope.hpp>

namespace dplx::dlog
{

namespace detail
{

auto vlog(bus_handle &messageBus, log_args const &args) noexcept
        -> result<void>;

} // namespace detail

inline constexpr struct log_fn
{
    template <typename... Args>
        requires(... && loggable<Args>)
    [[nodiscard]] auto
    operator()(bus_handle &msgBus,
               severity sev,
               fmt::format_string<reification_type_of_t<Args>...> message,
               detail::log_location location,
               Args const &...args) const noexcept -> result<void>
    {
        return detail::vlog(msgBus, detail::stack_log_args<Args...>{
                                            message, sev, span_context{},
                                            location, args...});
    }
    template <typename... Args>
        requires(... && loggable<Args>)
    [[nodiscard]] auto
    operator()(span_scope const &scope,
               severity sev,
               fmt::format_string<reification_type_of_t<Args>...> message,
               detail::log_location location,
               Args const &...args) const noexcept -> result<void>
    {
        auto *msgBus = scope.bus();
        if (msgBus == nullptr)
        {
            return oc::success();
        }

        return detail::vlog(*msgBus, detail::stack_log_args<Args...>{
                                             message, sev, scope.context(),
                                             location, args...});
    }

#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
    template <typename... Args>
        requires(... && loggable<Args>)
    [[nodiscard]] auto
    operator()(severity sev,
               fmt::format_string<reification_type_of_t<Args>...> message,
               detail::log_location location,
               Args const &...args) const noexcept -> result<void>
    {
        auto const *active = detail::active_span;
        if (active == nullptr)
        {
            return oc::success();
        }
        return detail::vlog(*active, detail::stack_log_args<Args...>{
                                             message, sev, span_context{},
                                             location, args...});
    }
#endif
} log;

} // namespace dplx::dlog
