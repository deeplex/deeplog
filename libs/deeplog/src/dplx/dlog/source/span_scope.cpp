
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/source/span_scope.hpp"

#include <dplx/dp/api.hpp>
#include <dplx/dp/codecs/auto_tuple.hpp>
#include <dplx/dp/codecs/std-container.hpp>
#include <dplx/dp/codecs/std-string.hpp>
#include <dplx/dp/items/emit_core.hpp>
#include <dplx/dp/items/parse_core.hpp>
#include <dplx/dp/object_def.hpp>
#include <dplx/dp/tuple_def.hpp>

#include <dplx/dlog/attributes.hpp>
#include <dplx/dlog/core/log_clock.hpp>
#include <dplx/dlog/detail/tls.hpp>
#include <dplx/dlog/source/log_record_port.hpp>
#include <dplx/dlog/source/record_output_buffer.hpp>

namespace dplx::dlog
{

// NOLINTBEGIN(cppcoreguidelines-pro-type-member-init)

struct span
{
    span_context id;
    span_context parent;
    std::string name;
    std::string function;
    log_clock::time_point start;
    log_clock::time_point end;
};

struct span_start_msg
{
    span_context id;
    span_kind kind;
    span_context parent;
    log_clock::time_point timestamp;
    std::string_view name;
    std::span<span_context, 0> links;
    detail::attribute_args const &attributes;
};

struct span_end_msg
{
    span_context id;
    log_clock::time_point timestamp;
};

// NOLINTEND(cppcoreguidelines-pro-type-member-init)

} // namespace dplx::dlog

template <>
class dplx::dp::codec<dplx::dlog::span_start_msg>
{
    using span_start_msg = dlog::span_start_msg;
    struct attributes_fn
        : member_accessor_base<span_start_msg,
                               dlog::detail::attribute_args const>
    {
        DPLX_ATTR_FORCE_INLINE auto operator()(auto &self) const noexcept
        {
            return &self.attributes;
        }
    };

    static constexpr tuple_def<tuple_member_def<&span_start_msg::id>{},
                               tuple_member_def<&span_start_msg::kind>{},
                               tuple_member_def<&span_start_msg::parent>{},
                               tuple_member_def<&span_start_msg::timestamp>{},
                               tuple_member_def<&span_start_msg::name>{},
                               tuple_member_def<&span_start_msg::links>{},
                               tuple_member_fun<attributes_fn>{}>
            layout_descriptor{};

public:
    static auto size_of(emit_context &ctx, span_start_msg const &msg) noexcept
            -> std::uint64_t
    {
        return dp::size_of_tuple<layout_descriptor>(ctx, msg);
    }
    static auto encode(emit_context &ctx, span_start_msg const &msg) noexcept
            -> result<void>
    {
        return dp::encode_tuple<layout_descriptor>(ctx, msg);
    }
};
template <>
class dplx::dp::codec<dplx::dlog::span_end_msg>
{
    using span_end_msg = dlog::span_end_msg;
    static constexpr tuple_def<tuple_member_def<&span_end_msg::id>{},
                               tuple_member_def<&span_end_msg::timestamp>{}>
            layout_descriptor{};

public:
    static auto size_of(emit_context &ctx, span_end_msg const &msg) noexcept
            -> std::uint64_t
    {
        return dp::size_of_tuple<layout_descriptor>(ctx, msg);
    }
    static auto encode(emit_context &ctx, span_end_msg const &msg) noexcept
            -> result<void>
    {
        return dp::encode_tuple<layout_descriptor>(ctx, msg);
    }
};

auto dplx::dp::codec<dplx::dlog::span_kind>::decode(
        parse_context &ctx, dlog::span_kind &value) noexcept -> result<void>
{
    std::underlying_type_t<dlog::span_kind> rawValue{};
    DPLX_TRY(dp::parse_integer(
            ctx, rawValue,
            static_cast<std::underlying_type_t<dlog::span_kind>>(
                    dlog::span_kind::server)));
    value = static_cast<dlog::span_kind>(rawValue);
    return oc::success();
}
auto dplx::dp::codec<dplx::dlog::span_kind>::size_of(
        emit_context &ctx, dlog::span_kind const &value) noexcept
        -> std::uint64_t
{
    return dp::item_size_of_integer(
            ctx, static_cast<std::underlying_type_t<dlog::span_kind>>(value));
}
auto dplx::dp::codec<dplx::dlog::span_kind>::encode(
        emit_context &ctx, dlog::span_kind const &value) noexcept
        -> result<void>
{
    DPLX_TRY(dp::emit_integer(
            ctx, static_cast<std::underlying_type_t<dlog::span_kind>>(value)));
    return oc::success();
}

namespace dplx::dlog
{

span_scope::span_scope(log_context *const ctx,
                       span_context const id,
                       severity const threshold) noexcept
    : mSpanThreshold{threshold}
    , mPreviousThreshold{ctx->threshold()}
    , mContext{ctx}
    , mId{id}
    , mPreviousId{ctx->span()}
{
    ctx->span(id);
    ctx->override_threshold(threshold);
}

#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
auto span_scope::none() noexcept -> span_scope
{
    return none(DPLX_DLOG_INTERNAL_ACTIVE_CONTEXT);
}
#endif

auto span_scope::none(log_context &ctx) noexcept -> span_scope
{
    auto *const port = ctx.port();
    return span_scope(&ctx, span_context{},
                      nullptr == port ? detail::disable_threshold
                                      : port->default_threshold());
}

#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
auto span_scope::do_open(std::string_view name,
                         span_kind kind,
                         detail::attribute_args const &attrs) noexcept
        -> span_scope
{
    auto &&activeContext = DPLX_DLOG_INTERNAL_ACTIVE_CONTEXT;
    return do_open(activeContext, name, activeContext.span(), kind, attrs);
}
auto span_scope::do_open(std::string_view name,
                         span_context parent,
                         span_kind kind,
                         detail::attribute_args const &attrs) noexcept
        -> span_scope
{
    return do_open(DPLX_DLOG_INTERNAL_ACTIVE_CONTEXT, name, parent, kind,
                   attrs);
}
#endif

auto span_scope::do_open(log_context &ctx,
                         std::string_view name,
                         span_context parent,
                         span_kind kind,
                         detail::attribute_args const &attrs) noexcept
        -> span_scope
{
    using namespace std::string_view_literals;
    auto *const port = ctx.port();
    if (nullptr == port)
    {
        return span_scope{};
    }

    severity newThreshold = ctx.threshold();
    span_start_msg msg{
            .id = ctx.port()->create_span_context(parent.traceId, name,
                                                  newThreshold),
            .kind = kind,
            .parent = parent,
            .timestamp = {},
            .name = name.data() == nullptr ? ""sv : name,
            .links = {},
            .attributes = attrs,
    };
    if (msg.id.traceId != trace_id::invalid())
    {
        msg.timestamp = log_clock::now();
        if (enqueue_message(*ctx.port(), msg.id.spanId, msg).has_value())
        {
            return {&ctx, msg.id, newThreshold};
        }
    }
    return {};
}

auto span_scope::send_close_msg() noexcept -> result<void>
{
    span_end_msg msg{
            .id = mId,
            .timestamp = log_clock::now(),
    };
    DPLX_TRY(enqueue_message(*mContext->port(), mId.spanId, msg));
    return oc::success();
}

} // namespace dplx::dlog
