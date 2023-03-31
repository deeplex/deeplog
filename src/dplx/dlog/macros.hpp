
// Copyright Henrik Steffen Gaßmann 2021, 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <dplx/predef/compiler.h>

#include <dplx/dlog/arguments.hpp>
#include <dplx/dlog/attributes.hpp>
#include <dplx/dlog/source.hpp>
#include <dplx/dlog/span_scope.hpp>

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

////////////////////////////////////////////////////////////////////////////////
// source_location shims

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
#if DPLX_DLOG_USE_SOURCE_LOCATION
#error "DPLX_DLOG_USE_SOURCE_LOCATION not implemented"
#else

#define DPLX_DLOG_LOCATION                                                     \
    ::dplx::dlog::detail::make_location(__FILE__, __LINE__)

#if defined(DPLX_COMP_MSVC_AVAILABLE)
#define DPLX_DLOG_FUNCSIG                                                      \
    ::dplx::dlog::detail::make_attribute_function(__FUNCSIG__)
#elif defined(DPLX_COMP_GNUC_AVAILABLE) || defined(DPLX_COMP_CLANG_AVAILABLE)
#define DPLX_DLOG_FUNCSIG                                                      \
    ::dplx::dlog::detail::make_attribute_function(__PRETTY_FUNCTION__)
#else
#define DPLX_DLOG_FUNCSIG                                                      \
    ::dplx::dlog::detail::make_attribute_function("<unsupported compiler>")
#endif

#endif
// NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)

////////////////////////////////////////////////////////////////////////////////
// log macros

#if _MSVC_TRADITIONAL

#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
#define DLOG_GENERIC(severity, message, ...)                                   \
    do                                                                         \
    { /* due to shadowing this name isn't required to be unique */             \
        if (auto &&_dlog_materialized_temporary_                               \
            = (::dplx::dlog::detail::active_span);                             \
            _dlog_materialized_temporary_ != nullptr                           \
            && (severity) >= _dlog_materialized_temporary_->threshold)         \
            (void)::dplx::dlog::log(*_dlog_materialized_temporary_,            \
                                    (severity), (message), DPLX_DLOG_LOCATION, \
                                    __VA_ARGS__);                              \
    } while (0)
#endif
#define DLOG_GENERIC_EX(ctx, severity, message, ...)                           \
    do                                                                         \
    { /* due to shadowing this name isn't required to be unique */             \
        if (auto &&_dlog_materialized_temporary_ = (ctx);                      \
            (severity) >= _dlog_materialized_temporary_.threshold)             \
            (void)::dplx::dlog::log(_dlog_materialized_temporary_, (severity), \
                                    (message), DPLX_DLOG_LOCATION,             \
                                    __VA_ARGS__);                              \
    } while (0)

#else // _MSVC_TRADITIONAL

#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
#define DLOG_GENERIC(severity, message, ...)                                   \
    do                                                                         \
    { /* due to shadowing this name isn't required to be unique */             \
        if (auto &&_dlog_materialized_temporary_                               \
            = (::dplx::dlog::detail::active_span);                             \
            _dlog_materialized_temporary_ != nullptr                           \
            && (severity) >= _dlog_materialized_temporary_->threshold)         \
            (void)::dplx::dlog::log(                                           \
                    *_dlog_materialized_temporary_, (severity), (message),     \
                    DPLX_DLOG_LOCATION __VA_OPT__(, __VA_ARGS__));             \
    } while (0)
#endif
#define DLOG_GENERIC_EX(ctx, severity, message, ...)                           \
    do                                                                         \
    { /* due to shadowing this name isn't required to be unique */             \
        if (auto &&_dlog_materialized_temporary_ = (ctx);                      \
            (severity) >= _dlog_materialized_temporary_.threshold)             \
            (void)::dplx::dlog::log(                                           \
                    _dlog_materialized_temporary_, (severity), (message),      \
                    DPLX_DLOG_LOCATION __VA_OPT__(, __VA_ARGS__));             \
    } while (0)

#endif // _MSVC_TRADITIONAL

#define DLOG_FATAL(message, ...)                                               \
    DLOG_GENERIC(::dplx::dlog::severity::fatal, message, __VA_ARGS__)
#define DLOG_ERROR(message, ...)                                               \
    DLOG_GENERIC(::dplx::dlog::severity::error, message, __VA_ARGS__)
#define DLOG_WARN(message, ...)                                                \
    DLOG_GENERIC(::dplx::dlog::severity::warn, message, __VA_ARGS__)
#define DLOG_INFO(message, ...)                                                \
    DLOG_GENERIC(::dplx::dlog::severity::info, message, __VA_ARGS__)
#define DLOG_DEBUG(message, ...)                                               \
    DLOG_GENERIC(::dplx::dlog::severity::debug, message, __VA_ARGS__)
#define DLOG_TRACE(message, ...)                                               \
    DLOG_GENERIC(::dplx::dlog::severity::trace, message, __VA_ARGS__)

#define DLOG_FATAL_EX(ctx, message, ...)                                       \
    DLOG_GENERIC_EX(ctx, ::dplx::dlog::severity::fatal, message, __VA_ARGS__)
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

////////////////////////////////////////////////////////////////////////////////
// span scope macros

#if _MSVC_TRADITIONAL

#define DPLX_DLOG_XDEF_SPAN_SCOPE(mode, name, attachOpt, ...)                  \
    ::dplx::dlog::span_scope::mode(                                            \
            name, ::dplx::dlog::attach::attachOpt,                             \
            ::dplx::dlog::make_attributes(DPLX_DLOG_FUNCSIG, __VA_ARGS__))
#define DPLX_DLOG_XDEF_SPAN_SCOPE_EX(mode, ctx, name, attachOpt, ...)          \
    ::dplx::dlog::span_scope::mode(                                            \
            ctx, name, ::dplx::dlog::attach::attachOpt,                        \
            ::dplx::dlog::make_attributes(DPLX_DLOG_FUNCSIG, __VA_ARGS__))

#else

#define DPLX_DLOG_XDEF_SPAN_SCOPE(mode, name, attachOpt, ...)                  \
    ::dplx::dlog::span_scope::mode(                                            \
            name, ::dplx::dlog::attach::attachOpt,                             \
            ::dplx::dlog::make_attributes(                                     \
                    DPLX_DLOG_FUNCSIG __VA_OPT__(, __VA_ARGS__)))
#define DPLX_DLOG_XDEF_SPAN_SCOPE_EX(mode, ctx, name, attachOpt, ...)          \
    ::dplx::dlog::span_scope::mode(                                            \
            ctx, name, ::dplx::dlog::attach::attachOpt,                        \
            ::dplx::dlog::make_attributes(                                     \
                    DPLX_DLOG_FUNCSIG __VA_OPT__(, __VA_ARGS__)))

#endif

#define DLOG_SPAN_START_EX(parent, name, ...)                                  \
    DPLX_DLOG_XDEF_SPAN_SCOPE_EX(start, parent, name, no, __VA_ARGS__)
#define DLOG_SPAN_START_ROOT_EX(ctx, name, ...)                                \
    DPLX_DLOG_XDEF_SPAN_SCOPE_EX(root, ctx, name, no, __VA_ARGS__)

#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT

#define DLOG_SPAN_START(name, ...)                                             \
    DPLX_DLOG_XDEF_SPAN_SCOPE(start, name, no, __VA_ARGS__)
#define DLOG_SPAN_START_ROOT(name, ...)                                        \
    DPLX_DLOG_XDEF_SPAN_SCOPE(root, name, no, __VA_ARGS__)

#define DLOG_SPAN_ATTACH(name, ...)                                            \
    DPLX_DLOG_XDEF_SPAN_SCOPE(start, name, yes, __VA_ARGS__)
#define DLOG_SPAN_ATTACH_EX(parent, name, ...)                                 \
    DPLX_DLOG_XDEF_SPAN_SCOPE_EX(start, parent, name, yes, __VA_ARGS__)
#define DLOG_SPAN_ATTACH_ROOT(name, ...)                                       \
    DPLX_DLOG_XDEF_SPAN_SCOPE(root, name, yes, __VA_ARGS__)
#define DLOG_SPAN_ATTACH_ROOT_EX(ctx, name, ...)                               \
    DPLX_DLOG_XDEF_SPAN_SCOPE_EX(root, ctx, name, yes, __VA_ARGS__)

#endif

// NOLINTEND(cppcoreguidelines-macro-usage)
