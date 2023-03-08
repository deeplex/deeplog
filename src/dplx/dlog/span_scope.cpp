
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/span_scope.hpp"

#include <dplx/dp/api.hpp>
#include <dplx/dp/codecs/auto_tuple.hpp>
#include <dplx/dp/codecs/std-string.hpp>
#include <dplx/dp/object_def.hpp>
#include <dplx/dp/tuple_def.hpp>

#include <dplx/dlog/detail/tls.hpp>
#include <dplx/dlog/log_bus.hpp>
#include <dplx/dlog/log_clock.hpp>

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
    span_context parent;
    std::string_view name;
    detail::function_location function;
    log_clock::time_point timestamp;
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
    static constexpr tuple_def<tuple_member_def<&span_start_msg::id>{},
                               tuple_member_def<&span_start_msg::parent>{},
                               tuple_member_def<&span_start_msg::name>{},
                               tuple_member_def<&span_start_msg::function>{},
                               tuple_member_def<&span_start_msg::timestamp>{}>
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

namespace dplx::dlog
{

#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
auto span_scope::none(attach mode) noexcept -> span_scope
{
    if (auto const *prev = detail::active_span; prev != nullptr)
    {
        return none(*prev->bus(), mode);
    }
    return span_scope{};
}
#endif
auto span_scope::none(bus_handle &targetBus, attach mode) noexcept -> span_scope
{
    return {{}, targetBus, nullptr, targetBus.threshold, mode};
}
#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
auto span_scope::open(std::string_view name,
                      detail::function_location fn) noexcept -> span_scope
{
    if (auto const *prev = detail::active_span; prev != nullptr)
    {
        return open(*prev, name, attach::no, fn);
    }
    return span_scope{};
}
auto span_scope::open(std::string_view name,
                      attach mode,
                      detail::function_location fn) noexcept -> span_scope
{
    if (auto const *prev = detail::active_span; prev != nullptr)
    {
        return open(*prev, name, mode, fn);
    }
    return span_scope{};
}
#endif
auto span_scope::open(bus_handle &targetBus,
                      std::string_view name,
                      detail::function_location fn) noexcept -> span_scope
{
    return open(targetBus, name, attach::no, fn);
}
auto span_scope::open(span_scope const &parent,
                      std::string_view name,
                      detail::function_location fn) noexcept -> span_scope
{
    return open(parent, name, attach::no, fn);
}
auto span_scope::open(bus_handle &targetBus,
                      std::string_view name,
                      attach mode,
                      detail::function_location fn) noexcept -> span_scope
{
    using namespace std::string_view_literals;

    span_start_msg msg{
            .id = {targetBus.allocate_trace_id(), {}},
            .parent = {},
            .name = name.data() == nullptr ? ""sv : name,
            .function = fn,
            .timestamp = {},
    };
    if (msg.id.traceId != trace_id::invalid())
    {
        msg.id.spanId = targetBus.allocate_span_id(msg.id.traceId);
        msg.timestamp = log_clock::now();
        if (targetBus.write(msg.id.spanId, msg).has_value())
        {
            return {msg.id, targetBus, nullptr, targetBus.threshold, mode};
        }
    }

    return {};
}
auto span_scope::open(span_scope const &parent,
                      std::string_view name,
                      attach mode,
                      detail::function_location fn) noexcept -> span_scope
{
    using namespace std::string_view_literals;
    if (parent.mBus == nullptr)
    {
        return {};
    }
    if (parent.mId.traceId == trace_id::invalid())
    {
        return open(*parent.mBus, name, mode, fn);
    }

    span_start_msg msg{
            .id = {parent.mId.traceId,
                   parent.mBus->allocate_span_id(parent.mId.traceId)},
            .parent = parent.mId,
            .name = name.data() == nullptr ? ""sv : name,
            .function = fn,
            .timestamp = log_clock::now(),
    };
    if (parent.mBus->write(msg.id.spanId, msg).has_failure())
    {
        return {};
    }

    return {msg.id, *parent.mBus, &parent, parent.threshold, mode};
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

auto dplx::dp::codec<dplx::dlog::detail::function_location>::size_of(
        emit_context &ctx, function_location location) noexcept -> std::uint64_t
{
    return dp::item_size_of_u8string(ctx, location.functionSize);
}

auto dplx::dp::codec<dplx::dlog::detail::function_location>::encode(
        emit_context &ctx, function_location location) noexcept -> result<void>
{
    if (location.functionSize < 0)
    {
        return dp::emit_null(ctx);
    }
    return dp::emit_u8string(ctx, location.function,
                             static_cast<std::size_t>(location.functionSize));
}
