
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

#include <boost/variant2.hpp>

#include <dplx/dlog/concepts.hpp>
#include <dplx/dlog/core/log_clock.hpp>
#include <dplx/dlog/core/serialized_messages.hpp>
#include <dplx/dlog/core/strong_types.hpp>
#include <dplx/dlog/sinks/sink_frontend.hpp>
#include <dplx/dlog/source/log_record_port.hpp>

namespace dplx::dlog
{

namespace detail
{

class log_fabric_base : public log_record_port
{
protected:
    using sink_owner = std::unique_ptr<sink_frontend_base>;

private:
    std::vector<sink_owner> mSinks;

protected:
    ~log_fabric_base() = default;
    log_fabric_base() noexcept = default;

public:
    log_fabric_base(log_fabric_base const &) noexcept = delete;
    auto operator=(log_fabric_base const &) noexcept = delete;

protected:
    log_fabric_base(log_fabric_base &&other) noexcept
        : log_record_port(std::move(other))
        , mSinks(std::move(other.mSinks))
    {
    }
    auto operator=(log_fabric_base &&other) noexcept -> log_fabric_base &
    {
        if (&other == this)
        {
            return *this;
        }
        log_record_port::operator=(std::move(other));
        // the above move slices
        mSinks = std::move(other.mSinks); // NOLINT(bugprone-use-after-move)
        return *this;
    }

    auto sinks() noexcept -> std::vector<sink_owner> &
    {
        return mSinks;
    }
    void sync_sinks() noexcept;

public:
    auto attach_sink(std::unique_ptr<sink_frontend_base> &&sink)
            -> sink_frontend_base *;
    auto attach_sink(std::unique_ptr<sink_frontend_base> &&sink,
                     std::nothrow_t) noexcept -> result<sink_frontend_base *>;
    auto destroy_sink(sink_frontend_base *which) noexcept -> result<void>;
    void remove_sink(sink_frontend_base *which) noexcept;
    auto release_sink(sink_frontend_base *which) noexcept
            -> sink_frontend_base *;
    void clear_sinks() noexcept;
};

} // namespace detail

template <bus MessageBus>
class log_fabric final : public detail::log_fabric_base
{
    MessageBus mMessageBus;

public:
    explicit log_fabric(MessageBus &&rig)
        : log_fabric_base()
        , mMessageBus(std::move(rig))
    {
    }

    auto message_bus() noexcept -> MessageBus &
    {
        return mMessageBus;
    }
    auto retire_log_records() noexcept -> result<int>
    {
        detail::consume_record_fn<MessageBus::consume_batch_size> drain{
                sinks()};

        DPLX_TRY(mMessageBus.consume_messages(drain));
        sync_sinks();
        return oc::success();
    }
    template <typename Sink>
    auto create_sink(typename Sink::config_type &&config) -> result<Sink *>
    {
        DPLX_TRY(auto &&sink, Sink::create_unique(std::move(config)));
        DPLX_TRY(auto *attached, attach_sink(std::move(sink), std::nothrow));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
        return static_cast<Sink *>(attached);
    }

private:
    // Inherited via log_record_port
    auto do_allocate_record_buffer_inplace(
            record_output_buffer_storage &bufferPlacementStorage,
            std::size_t messageSize,
            span_id spanId) noexcept -> result<record_output_buffer *> override
    {
        return mMessageBus.allocate_record_buffer_inplace(
                bufferPlacementStorage, messageSize, spanId);
    }
    auto do_create_span_context(trace_id id,
                                std::string_view name,
                                severity &thresholdInOut) noexcept
            -> span_context override
    {
        return mMessageBus.create_span_context(id, name, thresholdInOut);
    }
};

} // namespace dplx::dlog