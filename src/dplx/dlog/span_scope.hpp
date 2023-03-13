
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <dplx/dlog/arguments.hpp>
#include <dplx/dlog/attributes.hpp>
#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/detail/tls.hpp>
#include <dplx/dlog/fwd.hpp>

namespace dplx::dlog
{

enum class attach : bool
{
    no,
#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
    yes,
#endif
};

class [[nodiscard]] span_scope
{
    span_context mId{};
    bus_handle *mBus{};
    span_scope const *mPreviousScope{};

public:
    // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
    severity threshold{severity::none};

    ~span_scope() noexcept
    {
#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
        if (mBus != nullptr && detail::active_span == this)
        {
            detail::active_span = mPreviousScope;
        }
#endif
        if (mBus != nullptr && mId.spanId != span_id{})
        {
            (void)send_close_msg();
        }
    }
    span_scope() noexcept = default;

    span_scope(span_scope const &) = delete;
    auto operator=(span_scope const &) -> span_scope & = delete;

private:
    span_scope(span_context sctx,
               bus_handle &targetBus,
               span_scope const *parent,
               severity const thresholdInit,
               attach const mode = attach::no) noexcept
        : mId(sctx)
        , mBus(&targetBus)
        , mPreviousScope(mode == attach::no ? nullptr : parent)
        , threshold(thresholdInit)
    {
#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
        if (mode == attach::yes)
        {
            detail::active_span = this;
        }
#else
        assert(mode == attach::no);
#endif
    }
    auto send_close_msg() noexcept -> result<void>;

public:
#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
    static auto none(attach mode = attach::no) noexcept -> span_scope;
#endif
    static auto none(bus_handle &targetBus, attach mode = attach::no) noexcept
            -> span_scope;

    // construction flavors:
    //   - explicit, implicit
    //   - bus_handle, span_scope (aka root vs child)
    //   - no-, attach

#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
    static auto root(std::string_view name,
                     attach mode,
                     detail::attribute_args const &attrs) noexcept
            -> span_scope;
    static auto start(std::string_view name,
                      attach mode,
                      detail::attribute_args const &attrs) noexcept
            -> span_scope;
#endif
    static auto root(bus_handle &targetBus,
                     std::string_view name,
                     attach mode,
                     detail::attribute_args const &attrs) noexcept
            -> span_scope;
    static auto start(bus_handle &targetBus,
                      std::string_view name,
                      attach mode,
                      detail::attribute_args const &attrs) noexcept
            -> span_scope
    {
        return start(targetBus, span_context{}, name, mode, attrs);
    }
    static auto start(span_scope const &parent,
                      std::string_view name,
                      attach mode,
                      detail::attribute_args const &attrs) noexcept
            -> span_scope;
    static auto start(bus_handle &targetBus,
                      span_context parent,
                      std::string_view name,
                      attach mode,
                      detail::attribute_args const &attrs) noexcept
            -> span_scope;

    [[nodiscard]] auto context() const noexcept -> span_context
    {
        return mId;
    }
    [[nodiscard]] auto bus() const noexcept -> bus_handle *
    {
        return mBus;
    }
    [[nodiscard]] auto parent_scope() const noexcept -> span_scope const *
    {
        return mPreviousScope;
    }

#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
    void explicitly_attach() &noexcept
    {
        if (mPreviousScope == nullptr && detail::active_span != this)
        {
            mPreviousScope = detail::active_span;
            detail::active_span = this;
        }
    }
#endif
};

} // namespace dplx::dlog
