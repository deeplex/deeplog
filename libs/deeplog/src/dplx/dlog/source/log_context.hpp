
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <string_view>

#include <dplx/cncr/utils.hpp>

#include <dplx/dlog/core/strong_types.hpp>
#include <dplx/dlog/fwd.hpp>

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
        : mThresholdCache{}
        , mTargetPort(&targetPort)
        , mInstrumentationScope{}
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
    DPLX_ATTR_FORCE_INLINE constexpr auto span() const noexcept -> span_context
    {
        return mCurrentSpan;
    }
    DPLX_ATTR_FORCE_INLINE constexpr auto instrumentation_scope() const noexcept
            -> std::string_view
    {
        return mInstrumentationScope;
    }
};

} // namespace dplx::dlog
