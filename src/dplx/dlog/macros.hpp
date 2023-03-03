
// Copyright Henrik Steffen Gaßmann 2021, 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <dplx/dlog/arguments.hpp>
#include <dplx/dlog/source.hpp>

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

#if 0 && DPLX_DLOG_USE_SOURCE_LOCATION
#else
#define DPLX_DLOG_LOCATION                                                     \
    ::dplx::dlog::detail::make_location(__FILE__, __LINE__)
#endif

#if _MSVC_TRADITIONAL
#define DLOG_GENERIC(log, severity, message, ...)                              \
    do                                                                         \
    { /* due to shadowing this name isn't required to be unique */             \
        if (auto &&_dlog_materialized_temporary_ = (log);                      \
            _dlog_materialized_temporary_.threshold >= (severity))             \
            (void)_dlog_materialized_temporary_(                               \
                    (severity), (message), DPLX_DLOG_LOCATION, __VA_ARGS__);   \
    } while (0)
#else // _MSVC_TRADITIONAL
#define DLOG_GENERIC(log, severity, message, ...)                              \
    do                                                                         \
    { /* due to shadowing this name isn't required to be unique */             \
        if (auto &&_dlog_materialized_temporary_ = (log);                      \
            _dlog_materialized_temporary_.threshold >= (severity))             \
            (void)_dlog_materialized_temporary_(                               \
                    (severity), (message),                                     \
                    DPLX_DLOG_LOCATION __VA_OPT__(, __VA_ARGS__));             \
    } while (0)
#endif // _MSVC_TRADITIONAL

#define DLOG_CRITICAL(log, message, ...)                                       \
    DLOG_GENERIC(log, ::dplx::dlog::severity::critical, message, __VA_ARGS__)
#define DLOG_ERROR(log, message, ...)                                          \
    DLOG_GENERIC(log, ::dplx::dlog::severity::error, message, __VA_ARGS__)
#define DLOG_WARN(log, message, ...)                                           \
    DLOG_GENERIC(log, ::dplx::dlog::severity::warn, message, __VA_ARGS__)
#define DLOG_INFO(log, message, ...)                                           \
    DLOG_GENERIC(log, ::dplx::dlog::severity::info, message, __VA_ARGS__)
#define DLOG_DEBUG(log, message, ...)                                          \
    DLOG_GENERIC(log, ::dplx::dlog::severity::debug, message, __VA_ARGS__)
#define DLOG_TRACE(log, message, ...)                                          \
    DLOG_GENERIC(log, ::dplx::dlog::severity::trace, message, __VA_ARGS__)

// NOLINTEND(cppcoreguidelines-macro-usage)
