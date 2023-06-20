
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <source_location>

#include <dplx/dp/macros.hpp>

#include <dplx/dlog/attributes.hpp>
#include <dplx/dlog/config.hpp>
#include <dplx/dlog/core/strong_types.hpp>
#include <dplx/dlog/fwd.hpp>
#include <dplx/dlog/source/log_context.hpp>

namespace dplx::dlog
{

enum class span_kind : unsigned
{
    internal,
    consumer,
    producer,
    client,
    server,
};

#if DPLX_DLOG_USE_SOURCE_LOCATION
#define DPLX_DLOG_DETAIL_ATTR_FUNCTION_DEF                                     \
    , std::source_location const loc = std::source_location::current()
#define DPLX_DLOG_DETAIL_ATTR_FUNCTION_USE                                     \
    , attr::function                                                           \
    {                                                                          \
        {                                                                      \
            loc.function_name(),                                               \
                    std::char_traits<char>::length(loc.function_name())        \
        }                                                                      \
    }
#else
#define DPLX_DLOG_DETAIL_ATTR_FUNCTION_DEF
#define DPLX_DLOG_DETAIL_ATTR_FUNCTION_USE
#endif

class [[nodiscard]] span_scope
{
    severity mSpanThreshold;
    severity mPreviousThreshold;
    log_context *mContext;
    span_context mId;
    span_context mPreviousId;

public:
    constexpr ~span_scope() noexcept
    {
        if (nullptr != mContext)
        {
            mContext->span(mPreviousId);
            mContext->threshold(mPreviousThreshold);
            (void)send_close_msg();
        }
    }
    constexpr span_scope() noexcept
        : mSpanThreshold{}
        , mPreviousThreshold{}
        , mContext{}
        , mId{}
        , mPreviousId{}
    {
    }

    span_scope(span_scope const &) = delete;
    auto operator=(span_scope const &) -> span_scope & = delete;

    span_scope(span_scope &&) = delete;
    auto operator=(span_scope &&) -> span_scope && = delete;

private:
    span_scope(log_context *ctx, span_context id, severity threshold) noexcept;

public:
#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
    static auto none() noexcept -> span_scope;
#endif
    static auto none(log_context &ctx) noexcept -> span_scope;

    template <typename... Attrs>
        requires(... && attribute<Attrs>)
    static auto open(
            std::string_view name,
            Attrs const &...attrs,
            span_kind kind
            = span_kind::internal DPLX_DLOG_DETAIL_ATTR_FUNCTION_DEF) noexcept
            -> span_scope
    {
        return do_open(
                name, kind,
                make_attributes(attrs... DPLX_DLOG_DETAIL_ATTR_FUNCTION_USE));
    }
    template <typename... Attrs>
        requires(... && attribute<Attrs>)
    static auto open(
            std::string_view name,
            span_context parent,
            Attrs const &...attrs,
            span_kind kind
            = span_kind::internal DPLX_DLOG_DETAIL_ATTR_FUNCTION_DEF) noexcept
            -> span_scope
    {
        return do_open(
                name, parent, kind,
                make_attributes(attrs... DPLX_DLOG_DETAIL_ATTR_FUNCTION_USE));
    }
    template <typename... Attrs>
        requires(... && attribute<Attrs>)
    static auto open(
            log_context &ctx,
            std::string_view name,
            Attrs const &...attrs,
            span_kind kind
            = span_kind::internal DPLX_DLOG_DETAIL_ATTR_FUNCTION_DEF) noexcept
            -> span_scope
    {
        return do_open(
                ctx, name, ctx.span(), kind,
                make_attributes(attrs... DPLX_DLOG_DETAIL_ATTR_FUNCTION_USE));
    }
    template <typename... Attrs>
        requires(... && attribute<Attrs>)
    static auto open(
            log_context &ctx,
            std::string_view name,
            span_context parent,
            Attrs const &...attrs,
            span_kind kind
            = span_kind::internal DPLX_DLOG_DETAIL_ATTR_FUNCTION_DEF) noexcept
            -> span_scope
    {
        return do_open(
                ctx, name, parent, kind,
                make_attributes(attrs... DPLX_DLOG_DETAIL_ATTR_FUNCTION_USE));
    }

    [[nodiscard]] auto context() const noexcept -> span_context
    {
        return mId;
    }

private:
#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
    static auto do_open(std::string_view name,
                        span_kind kind,
                        detail::attribute_args const &attrs) noexcept
            -> span_scope;
    static auto do_open(std::string_view name,
                        span_context parent,
                        span_kind kind,
                        detail::attribute_args const &attrs) noexcept
            -> span_scope;
#endif
    static auto do_open(log_context &ctx,
                        std::string_view name,
                        span_context parent,
                        span_kind kind,
                        detail::attribute_args const &attrs) noexcept
            -> span_scope;

    auto send_close_msg() noexcept -> result<void>;
};

#undef DPLX_DLOG_DETAIL_ATTR_FUNCTION

} // namespace dplx::dlog

DPLX_DP_DECLARE_CODEC_SIMPLE(dplx::dlog::span_kind);
