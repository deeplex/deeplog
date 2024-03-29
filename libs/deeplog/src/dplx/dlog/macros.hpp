
// Copyright Henrik Steffen Gaßmann 2021, 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <dplx/predef/compiler.h>

#include <dplx/dlog/attributes.hpp>
#include <dplx/dlog/config.hpp>
#include <dplx/dlog/source/log.hpp>
#include <dplx/dlog/source/log_context.hpp>

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

////////////////////////////////////////////////////////////////////////////////
// source_location shims

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
#if DPLX_DLOG_USE_SOURCE_LOCATION
#define DPLX_DLOG_LOCATION ::std::source_location::current()

#else
#define DPLX_DLOG_LOCATION                                                     \
    ::dplx::dlog::detail::make_location(__FILE__, __LINE__)

#endif
// NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)

////////////////////////////////////////////////////////////////////////////////
// log macros

#if _MSVC_TRADITIONAL

#define DLOG_TO(ctx, severity, message, ...)                                   \
    do                                                                         \
    { /* due to shadowing this name isn't required to be unique */             \
        if (auto &&_dlog_materialized_temporary_ = (ctx);                      \
            (severity) >= _dlog_materialized_temporary_.threshold())           \
            (void)::dplx::dlog::log(_dlog_materialized_temporary_, (severity), \
                                    (message), DPLX_DLOG_LOCATION,             \
                                    __VA_ARGS__);                              \
    }                                                                          \
    while (0)

#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
#define DLOG_(severityName, message, ...)                                      \
    DLOG_TO(::dplx::dlog::detail::active_context(),                            \
            (::dplx::dlog::severity::severityName), message, __VA_ARGS__)
#endif

#else // _MSVC_TRADITIONAL

#define DLOG_TO(ctx, severity, message, ...)                                   \
    do                                                                         \
    { /* due to shadowing this name isn't required to be unique */             \
        if (auto &&_dlog_materialized_temporary_ = (ctx);                      \
            (severity) >= _dlog_materialized_temporary_.threshold())           \
            (void)::dplx::dlog::log(                                           \
                    _dlog_materialized_temporary_, (severity), (message),      \
                    DPLX_DLOG_LOCATION __VA_OPT__(, __VA_ARGS__));             \
    }                                                                          \
    while (0)

#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
#define DLOG_(severityName, message, ...)                                      \
    DLOG_TO(::dplx::dlog::detail::active_context(),                            \
            (::dplx::dlog::severity::severityName),                            \
            message __VA_OPT__(, __VA_ARGS__))
#endif

#endif // _MSVC_TRADITIONAL

// NOLINTEND(cppcoreguidelines-macro-usage)
