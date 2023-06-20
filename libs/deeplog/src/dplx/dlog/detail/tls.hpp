
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>
#include <type_traits>

#include <dplx/predef/os/windows.h>

#include <dplx/dlog/config.hpp>
#include <dplx/dlog/detail/workaround.hpp>
#include <dplx/dlog/fwd.hpp>
#include <dplx/dlog/source/log_context.hpp>

#if defined(DPLX_DLOG_BUILDING) && !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT

namespace dplx::dlog::detail
{

#if defined(DPLX_OS_WINDOWS_AVAILABLE)

#if DPLX_DLOG_WORKAROUND_ISSUE_DEVCOM_1406069

struct most_trivial_string_view
{
    char const *data_;
    std::size_t size_;
};
struct log_context_
{
    severity mThresholdCache;
    log_record_port *mTargetPort;
    most_trivial_string_view mInstrumentationScope;
    span_context mCurrentSpan;
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern thread_local constinit log_context_ active_context_;
#define DPLX_DLOG_INTERNAL_ACTIVE_CONTEXT                                      \
    *reinterpret_cast<log_context *>(&::dplx::dlog::detail::active_context_)

#else // ^^^ workaround DevCom-1406069 / no workaround vvv

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern thread_local constinit log_context active_context_;
#define DPLX_DLOG_INTERNAL_ACTIVE_CONTEXT ::dplx::dlog::detail::active_context_

#endif // ^^^ no workaround ^^^

#else // ^^^ workaround WINDOWS_AVAILABLE / no workaround vvv

#define DPLX_DLOG_INTERNAL_ACTIVE_CONTEXT ::dplx::dlog::detail::active_context_

#endif // ^^^ no workaround ^^^

} // namespace dplx::dlog::detail

#endif // ^^^ DPLX_DLOG_BUILDING && !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT ^^^
