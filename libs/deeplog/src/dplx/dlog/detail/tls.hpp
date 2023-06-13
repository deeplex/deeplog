
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <dplx/predef/os/windows.h>

#include <dplx/dlog/config.hpp>
#include <dplx/dlog/detail/workaround.hpp>
#include <dplx/dlog/fwd.hpp>
#include <dplx/dlog/source/log_context.hpp>

namespace dplx::dlog::detail
{

#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern thread_local constinit span_scope const *active_span;

void deactivate_span() noexcept;

#endif

#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT

#if defined(DPLX_OS_WINDOWS_AVAILABLE)
// Windows DLLs cannot export threadlocal symbols

auto active_context() noexcept -> log_context;
void active_context(log_context nextActiveContext) noexcept;

#else // ^^^ workaround WINDOWS_AVAILABLE / no workaround vvv

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern thread_local constinit log_context active_context_;

inline auto active_context() noexcept -> log_context
{
    return active_context_;
}
inline void active_context(log_context nextActiveContext) noexcept
{
    active_context_ = nextActiveContext;
}

#endif // ^^^ no workaround ^^^

#endif // ^^^ !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT ^^^

} // namespace dplx::dlog::detail
