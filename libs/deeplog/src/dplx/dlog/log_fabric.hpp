
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

#include <boost/unordered/unordered_flat_map.hpp>

#include <dplx/make.hpp>

#include <dplx/dlog/concepts.hpp>
#include <dplx/dlog/core/log_clock.hpp>
#include <dplx/dlog/core/serialized_messages.hpp>
#include <dplx/dlog/core/strong_types.hpp>
#include <dplx/dlog/sinks/sink_frontend.hpp>
#include <dplx/dlog/source/log_record_port.hpp>

namespace dplx::dlog::detail
{

struct transparent_string_hash : private boost::hash<std::string_view>
{
    using is_transparent = int;
    using is_avalanching = int;

    inline auto operator()(std::string_view value) const noexcept -> std::size_t
    {
        return boost::hash<std::string_view>::operator()(value);
    }
};

class log_fabric_base : public log_record_port
{
public:
    using scope_threshold_map
            = boost::unordered_flat_map<std::string,
                                        severity,
                                        transparent_string_hash,
                                        std::equal_to<void>>;

protected:
    using sink_owner = std::unique_ptr<sink_frontend_base>;

private:
    std::vector<sink_owner> mSinks;
    scope_threshold_map mThresholds;
    severity mDefaultThreshold;

protected:
    ~log_fabric_base() = default;
    explicit log_fabric_base(severity defaultThreshold
                             = dlog::default_threshold,
                             scope_threshold_map thresholds = {}) noexcept
        : mSinks()
        , mThresholds(std::move(thresholds))
        , mDefaultThreshold(defaultThreshold)
    {
    }

public:
    log_fabric_base(log_fabric_base const &) noexcept = delete;
    auto operator=(log_fabric_base const &) noexcept = delete;

protected:
    log_fabric_base(log_fabric_base &&other) noexcept
        : log_record_port(std::move(other))
        , mSinks(std::move(other.mSinks))
        , mThresholds(std::move(other.mThresholds))
        , mDefaultThreshold(std::exchange(other.mDefaultThreshold,
                                          dlog::default_threshold))
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
        // NOLINTBEGIN(bugprone-use-after-move)
        mSinks = std::move(other.mSinks);
        mThresholds = std::move(other.mThresholds);
        mDefaultThreshold = std::exchange(other.mDefaultThreshold,
                                          dlog::default_threshold);
        // NOLINTEND(bugprone-use-after-move)
        return *this;
    }

    auto sinks() noexcept -> std::vector<sink_owner> &
    {
        return mSinks;
    }
    void sync_sinks() noexcept;

public:
    auto attach_sink(std::unique_ptr<sink_frontend_base> &&sinkPtr)
            -> sink_frontend_base *;
    auto attach_sink(std::unique_ptr<sink_frontend_base> &&sinkPtr,
                     std::nothrow_t) noexcept -> result<sink_frontend_base *>;
    auto destroy_sink(sink_frontend_base *which) noexcept -> result<void>;
    void remove_sink(sink_frontend_base *which) noexcept;
    auto release_sink(sink_frontend_base *which) noexcept
            -> sink_frontend_base *;
    void clear_sinks() noexcept;

    void notify_epoch_changed() noexcept
    {
        sync_sinks();
    }

private:
    [[nodiscard]] auto do_default_threshold() const noexcept
            -> severity override;
    [[nodiscard]] auto do_threshold(char const *scopeName,
                                    std::size_t scopeNameSize) const noexcept
            -> severity override;
};

} // namespace dplx::dlog::detail

namespace dplx::dlog
{

template <bus MessageBus>
class log_fabric final : public detail::log_fabric_base
{
    MessageBus mMessageBus;

public:
    explicit log_fabric(MessageBus &&msgBus,
                        severity defaultThreshold = dlog::default_threshold,
                        scope_threshold_map thresholds = {})
        : log_fabric_base(defaultThreshold, std::move(thresholds))
        , mMessageBus(std::move(msgBus))
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
        return outcome::success();
    }
    template <sink Sink>
    auto create_sink(make<Sink> &&maker) -> result<Sink *>
    {
        DPLX_TRY(auto &&tmp, maker());
        std::unique_ptr<Sink> sinkPtr(new (std::nothrow) Sink(std::move(tmp)));
        if (!sinkPtr)
        {
            return system_error::errc::not_enough_memory;
        }

        DPLX_TRY(auto *attached, attach_sink(std::move(sinkPtr), std::nothrow));
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

template <dplx::dlog::bus MessageBus>
struct dplx::make<dplx::dlog::log_fabric<MessageBus>>
{
    using scope_threshold_map
            = dlog::detail::log_fabric_base::scope_threshold_map;

    make<MessageBus> make_bus;
    dlog::severity default_threshold{dlog::default_threshold};
    scope_threshold_map thresholds{};

    auto operator()() const noexcept -> result<dlog::log_fabric<MessageBus>>
    {
        DPLX_TRY(auto &&msgBus, make_bus());
        try
        {
            return dlog::log_fabric<MessageBus>{std::move(msgBus),
                                                default_threshold, thresholds};
        }
        catch (std::bad_alloc const &) // copying thresholds may throw
        {
            (void)msgBus.unlink();
            return dlog::errc::not_enough_memory;
        }
    }
};
