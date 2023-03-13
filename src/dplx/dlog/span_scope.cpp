
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/span_scope.hpp"

#include <dplx/dp/api.hpp>
#include <dplx/dp/codecs/auto_tuple.hpp>
#include <dplx/dp/codecs/std-container.hpp>
#include <dplx/dp/codecs/std-string.hpp>
#include <dplx/dp/items/emit_core.hpp>
#include <dplx/dp/items/parse_core.hpp>
#include <dplx/dp/object_def.hpp>
#include <dplx/dp/tuple_def.hpp>

#include <dplx/dlog/detail/tls.hpp>
#include <dplx/dlog/log_bus.hpp>
#include <dplx/dlog/log_clock.hpp>

#if DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
#define DPLX_DLOG_ACTIVE_SPAN nullptr
#else
#define DPLX_DLOG_ACTIVE_SPAN ::dplx::dlog::detail::active_span
#endif

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
    // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
    std::underlying_type_t<dlog::span_kind> rawValue;
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

#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
auto span_scope::none(attach const mode) noexcept -> span_scope
{
    if (auto const *prev = detail::active_span; prev != nullptr)
    {
        return none(*prev->bus(), mode);
    }
    return span_scope{};
}
#endif
auto span_scope::none(bus_handle &targetBus, attach const mode) noexcept
        -> span_scope
{
    return {{}, targetBus, nullptr, targetBus.threshold, mode};
}
#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
auto span_scope::root(std::string_view const name,
                      attach const mode,
                      detail::attribute_args const &attrs,
                      span_kind const kind) noexcept -> span_scope
{
    if (auto const *prev = detail::active_span; prev != nullptr)
    {
        return root(*prev->bus(), name, mode, attrs, kind);
    }
    return span_scope{};
}
auto span_scope::start(std::string_view const name,
                       attach const mode,
                       detail::attribute_args const &attrs,
                       span_kind const kind) noexcept -> span_scope
{
    if (auto const *prev = detail::active_span; prev != nullptr)
    {
        return start(*prev->bus(), prev->context(), name, mode, attrs, kind);
    }
    return span_scope{};
}
#endif
auto span_scope::root(bus_handle &targetBus,
                      std::string_view const name,
                      attach const mode,
                      detail::attribute_args const &attrs,
                      span_kind const kind) noexcept -> span_scope
{
    using namespace std::string_view_literals;

    span_start_msg msg{
            .id = targetBus.allocate_span_context(),
            .kind = kind,
            .parent = {},
            .timestamp = {},
            .name = name.data() == nullptr ? ""sv : name,
            .links = {},
            .attributes = attrs,
    };
    if (msg.id.traceId != trace_id::invalid())
    {
        msg.timestamp = log_clock::now();
        if (targetBus.write(msg.id.spanId, msg).has_value())
        {
            return {msg.id, targetBus, DPLX_DLOG_ACTIVE_SPAN,
                    targetBus.threshold, mode};
        }
    }

    return {};
}
auto span_scope::start(span_scope const &parent,
                       std::string_view name,
                       attach const mode,
                       detail::attribute_args const &attrs,
                       span_kind const kind) noexcept -> span_scope
{
    using namespace std::string_view_literals;
    if (parent.mBus == nullptr)
    {
        return {};
    }
    if (parent.mId.traceId == trace_id::invalid())
    {
        return root(*parent.mBus, name, mode, attrs);
    }

    span_start_msg msg{
            .id = {parent.mId.traceId,
                   parent.mBus->allocate_span_id(parent.mId.traceId)},
            .kind = kind,
            .parent = parent.mId,
            .timestamp = log_clock::now(),
            .name = name.data() == nullptr ? ""sv : name,
            .links = {},
            .attributes = attrs,
    };
    if (parent.mBus->write(msg.id.spanId, msg).has_failure())
    {
        return {};
    }

    return {msg.id, *parent.mBus, DPLX_DLOG_ACTIVE_SPAN, parent.threshold,
            mode};
}
auto span_scope::start(bus_handle &targetBus,
                       span_context const parent,
                       std::string_view const name,
                       attach const mode,
                       detail::attribute_args const &attrs,
                       span_kind const kind) noexcept -> span_scope
{
    using namespace std::string_view_literals;
    if (parent.traceId == trace_id::invalid())
    {
        return root(targetBus, name, mode, attrs);
    }

    span_start_msg msg{
            .id = {parent.traceId, targetBus.allocate_span_id(parent.traceId)},
            .kind = kind,
            .parent = parent,
            .timestamp = log_clock::now(),
            .name = name.data() == nullptr ? ""sv : name,
            .links = {},
            .attributes = attrs,
    };
    if (targetBus.write(msg.id.spanId, msg).has_failure())
    {
        return {};
    }

    return {msg.id, targetBus, DPLX_DLOG_ACTIVE_SPAN, targetBus.threshold,
            mode};
}

auto span_scope::send_close_msg() noexcept -> result<void>
{
    span_end_msg msg{
            .id = mId,
            .timestamp = log_clock::now(),
    };
    return mBus->write(mId.spanId, msg);
}

} // namespace dplx::dlog
