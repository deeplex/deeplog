
// Copyright Henrik Steffen Gaßmann 2021, 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <dplx/predef/compiler.h>

#include <dplx/dlog/arguments.hpp>
#include <dplx/dlog/source.hpp>
#include <dplx/dlog/span_scope.hpp>

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
#if 0 && DPLX_DLOG_USE_SOURCE_LOCATION
#else
#define DPLX_DLOG_LOCATION                                                     \
    ::dplx::dlog::detail::make_location(__FILE__, __LINE__)
#endif

#if 0 && DPLX_DLOG_USE_SOURCE_LOCATION
#else

#if defined(DPLX_COMP_MSVC_AVAILABLE)
#define DPLX_DLOG_FUNC_LOCATION                                                \
    ::dplx::dlog::detail::make_function_location(__FUNCSIG__)
#elif defined(DPLX_COMP_GNUC_AVAILABLE)
#define DPLX_DLOG_FUNC_LOCATION                                                \
    ::dplx::dlog::detail::make_function_location(__PRETTY_FUNCTION__)
#else
#define DPLX_DLOG_FUNC_LOCATION                                                \
    ::dplx::dlog::detail::make_function_location("<unsupported compiler>")
#endif

#endif
// NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)

#if _MSVC_TRADITIONAL
#define DLOG_GENERIC_EX(ctx, severity, message, ...)                           \
    do                                                                         \
    { /* due to shadowing this name isn't required to be unique */             \
        if (auto &&_dlog_materialized_temporary_ = (ctx);                      \
            _dlog_materialized_temporary_.threshold >= (severity))             \
            (void)::dplx::dlog::log(_dlog_materialized_temporary_, (severity), \
                                    (message), DPLX_DLOG_LOCATION,             \
                                    __VA_ARGS__);                              \
    } while (0)
#else // _MSVC_TRADITIONAL
#define DLOG_GENERIC_EX(ctx, severity, message, ...)                           \
    do                                                                         \
    { /* due to shadowing this name isn't required to be unique */             \
        if (auto &&_dlog_materialized_temporary_ = (ctx);                      \
            _dlog_materialized_temporary_.threshold >= (severity))             \
            (void)::dplx::dlog::log(                                           \
                    _dlog_materialized_temporary_, (severity), (message),      \
                    DPLX_DLOG_LOCATION __VA_OPT__(, __VA_ARGS__));             \
    } while (0)
#endif // _MSVC_TRADITIONAL

#define DLOG_CRITICAL_EX(ctx, message, ...)                                    \
    DLOG_GENERIC_EX(ctx, ::dplx::dlog::severity::critical, message, __VA_ARGS__)
#define DLOG_ERROR_EX(ctx, message, ...)                                       \
    DLOG_GENERIC_EX(ctx, ::dplx::dlog::severity::error, message, __VA_ARGS__)
#define DLOG_WARN_EX(ctx, message, ...)                                        \
    DLOG_GENERIC_EX(ctx, ::dplx::dlog::severity::warn, message, __VA_ARGS__)
#define DLOG_INFO_EX(ctx, message, ...)                                        \
    DLOG_GENERIC_EX(ctx, ::dplx::dlog::severity::info, message, __VA_ARGS__)
#define DLOG_DEBUG_EX(ctx, message, ...)                                       \
    DLOG_GENERIC_EX(ctx, ::dplx::dlog::severity::debug, message, __VA_ARGS__)
#define DLOG_TRACE_EX(ctx, message, ...)                                       \
    DLOG_GENERIC_EX(ctx, ::dplx::dlog::severity::trace, message, __VA_ARGS__)

#if _MSVC_TRADITIONAL
#define DLOG_OPEN_SPAN_EX(ctx, name, ...)                                      \
    ::dplx::dlog::span_scope::open(ctx, name, __VA_ARGS__,                     \
                                   DPLX_DLOG_FUNC_LOCATION)
#else
#define DLOG_OPEN_SPAN_EX(ctx, name, ...)                                      \
    ::dplx::dlog::span_scope::open(ctx, name __VA_OPT__(, ) __VA_ARGS__,       \
                                   DPLX_DLOG_FUNC_LOCATION)
#endif

#if 0 && !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
#define DLOG_OPEN_SPAN(name, ...)
#define DLOG_ATTACH_SPAN(name, ...)
#define DLOG_ATTACH_SPAN_EX(ctx, name, ...)
#endif

// NOLINTEND(cppcoreguidelines-macro-usage)
