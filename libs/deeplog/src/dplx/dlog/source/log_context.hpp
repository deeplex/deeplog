
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <string_view>
#include <utility>

#include <dplx/cncr/utils.hpp>
#include <dplx/predef/os/windows.h>

#include <dplx/dlog/core/strong_types.hpp>
#include <dplx/dlog/detail/workaround.hpp>
#include <dplx/dlog/fwd.hpp>
#include <dplx/dlog/source/log_record_port.hpp>

namespace dplx::dlog
{

class scope_name
{
    std::string_view mValue;

public:
    template <std::convertible_to<std::string_view> S>
    consteval scope_name(S const &name) noexcept
        : mValue(name)
    {
    }

    DPLX_ATTR_FORCE_INLINE operator std::string_view() const noexcept
    {
        return mValue;
    }
};

class log_context
{
    severity mThresholdCache;
    log_record_port *mTargetPort;
    std::string_view mInstrumentationScope;
    span_context mCurrentSpan;

public:
    constexpr log_context() noexcept
        : mThresholdCache{detail::disable_threshold}
        , mTargetPort{}
        , mInstrumentationScope{}
        , mCurrentSpan{}
    {
    }
    explicit log_context(scope_name name) noexcept
        : mThresholdCache{detail::disable_threshold}
        , mTargetPort{}
        , mInstrumentationScope(name)
        , mCurrentSpan{}
    {
    }
    explicit log_context(log_record_port &targetPort,
                         span_context span = span_context{})
        : mThresholdCache{targetPort.default_threshold()}
        , mTargetPort(&targetPort)
        , mInstrumentationScope{}
        , mCurrentSpan(span)
    {
    }
    explicit log_context(log_record_port &targetPort,
                         scope_name name,
                         span_context span = span_context{})
        : mThresholdCache{targetPort.threshold(name)}
        , mTargetPort(&targetPort)
        , mInstrumentationScope(name)
        , mCurrentSpan(span)
    {
    }

    DPLX_ATTR_FORCE_INLINE constexpr auto port() const noexcept
            -> log_record_port *
    {
        return mTargetPort;
    }
    DPLX_ATTR_FORCE_INLINE constexpr auto threshold() const noexcept -> severity
    {
        return mThresholdCache;
    }
    DPLX_ATTR_FORCE_INLINE constexpr void
    override_threshold(severity nextThreshold) noexcept
    {
        mThresholdCache = nextThreshold;
    }
    DPLX_ATTR_FORCE_INLINE constexpr auto span() const noexcept -> span_context
    {
        return mCurrentSpan;
    }
    DPLX_ATTR_FORCE_INLINE constexpr void span(span_context next) noexcept
    {
        mCurrentSpan = next;
    }
    DPLX_ATTR_FORCE_INLINE constexpr auto instrumentation_scope() const noexcept
            -> std::string_view
    {
        return mInstrumentationScope;
    }
};

} // namespace dplx::dlog

#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT

#if defined(DPLX_OS_WINDOWS_AVAILABLE)
// Windows DLLs cannot export thread_local symbols

namespace dplx::dlog
{

namespace detail
{

auto active_context() noexcept -> log_context &;

}

inline auto set_thread_context(log_context next) noexcept -> log_context
{
    return std::exchange(detail::active_context(), next);
}

} // namespace dplx::dlog

#else  // ^^^ workaround WINDOWS_AVAILABLE / no workaround vvv

namespace dplx::dlog
{

namespace detail
{

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern thread_local constinit log_context active_context_;

inline auto active_context() noexcept -> log_context &
{
    return active_context_;
}

} // namespace detail

inline auto set_thread_context(log_context next) noexcept -> log_context
{
    return std::exchange(detail::active_context_, next);
}

} // namespace dplx::dlog

#endif // ^^^ no workaround ^^^

#else  // ^^^ !DISABLE_IMPLICIT_CONTEXT / DISABLE_IMPLICIT_CONTEXT vvv

namespace dplx::dlog
{

inline auto set_thread_context(log_context next) noexcept -> log_context
        = delete;

}

#endif // ^^^ DISABLE_IMPLICIT_CONTEXT ^^^
