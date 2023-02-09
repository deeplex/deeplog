
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <vector>

#include <dplx/dlog/concepts.hpp>
#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/log_clock.hpp>

namespace dplx::dlog
{

struct additional_record_info
{
    log_clock::time_point timestamp;
    severity message_severity;
    unsigned message_size;
    unsigned attributes_size;
    unsigned format_args_size;

    static constexpr auto message_offset() noexcept -> int
    {
        return 1 + 1 + encoded_timestamp_size;
    }
    [[nodiscard]] constexpr auto attributes_offset() const noexcept -> int
    {
        return 1 + 1 + encoded_timestamp_size + static_cast<int>(message_size);
    }
    [[nodiscard]] constexpr auto format_args_offset() const noexcept -> int
    {
        return 1 + 1 + encoded_timestamp_size + static_cast<int>(message_size)
             + static_cast<int>(attributes_size);
    }

    static constexpr int encoded_timestamp_size = 9;
};

class sink_frontend_base
{
public:
    using predicate_type = std::function<bool(
            bytes rawMessage, additional_record_info const &additionalInfo)>;

private:
    severity mThreshold;
    result<void> mLastRx;
    predicate_type mShouldDiscard;

public:
    virtual ~sink_frontend_base() = default;

protected:
    /*
    sink_frontend_base(sink_frontend_base const &) = default;
    auto operator=(sink_frontend_base const &)
            -> sink_frontend_base & = default;
            */
    sink_frontend_base(sink_frontend_base &&) noexcept = default;
    auto operator=(sink_frontend_base &&) noexcept
            -> sink_frontend_base & = default;

    explicit sink_frontend_base(severity threshold,
                                predicate_type shouldDiscard) noexcept
        : mThreshold(threshold)
        , mLastRx(oc::success())
        , mShouldDiscard(std::move(shouldDiscard))
    {
    }

private:
    // exactly four pointer sized arguments to avoid the stack with
    // Microsoft's x64 calling convention
    virtual auto retire(bytes rawMessage,
                        additional_record_info const &additionalInfo) noexcept
            -> result<void>
            = 0;

    virtual auto drain_opened_impl() noexcept -> result<void>
    {
        return oc::success();
    }
    virtual auto drain_closed_impl() noexcept -> result<void>
    {
        return oc::success();
    }

public:
    void push(bytes rawMessage,
              additional_record_info const &additionalInfo) noexcept;

    auto drain_opened() noexcept -> result<void>
    {
        if (mLastRx.has_value())
        {
            mLastRx = drain_opened_impl();
        }
        return oc::success(); // FIXME: think about lazy error passing
    }
    auto drain_closed() noexcept -> result<void>
    {
        if (mLastRx.has_value())
        {
            mLastRx = drain_closed_impl();
        }
        return oc::success(); // FIXME: think about lazy error passing
    }

    [[nodiscard]] static auto last_rx() noexcept -> result<void>
    {
        return oc::success(); // FIXME: think about lazy error passing
    }
};

namespace detail
{

struct consume_record_fn
{
    std::span<std::unique_ptr<sink_frontend_base>> sinks;

    void operator()(bytes const message) const noexcept
    {
        (void)multicast(message, sinks);
    }

private:
    static auto multicast(
            bytes message,
            std::span<std::unique_ptr<sink_frontend_base> const> sinks) noexcept
            -> result<void>;
};

} // namespace detail

template <bus Bus>
class core
{
    using sink_owner = std::unique_ptr<sink_frontend_base>;

    Bus mBus;
    std::vector<sink_owner> mSinks;

public:
    explicit core(Bus &&rig)
        : mBus(std::move(rig))
    {
    }

    auto connector() noexcept -> Bus &
    {
        return mBus;
    }

    auto retire_log_records() -> result<int>
    {
        auto validEnd
                = std::partition(mSinks.begin(), mSinks.end(),
                                 [](sink_owner const &sink)
                                 { return sink->drain_opened().has_value(); });

        auto numValidSinks
                = static_cast<std::size_t>(validEnd - mSinks.begin());
        detail::consume_record_fn const drain{
                std::span(mSinks.data(), numValidSinks)};

        DPLX_TRY(mBus.consume_content(drain));

        int numSinkFailures = 0;
        for (auto &sink : mSinks)
        {
            if (sink->drain_closed().has_error())
            {
                numSinkFailures += 1;
            }
        }
        return numSinkFailures;
    }

    void attach_sink(std::unique_ptr<sink_frontend_base> &&sink)
    {
        mSinks.push_back(std::move(sink));
    }
    void remove_sink(sink_frontend_base *which) noexcept
    {
        if (auto const where
            = std::ranges::find(mSinks, which, &sink_owner::get);
            where != mSinks.end())
        {
            mSinks.erase(where);
        }
    }
    void release_sink(sink_frontend_base *which) noexcept
    {
        if (auto const where
            = std::ranges::find(mSinks, which, &sink_owner::get);
            where != mSinks.end())
        {
            where->release();
            mSinks.erase(where);
        }
    }
    void clear_sinks() noexcept
    {
        mSinks.clear();
    }

private:
};

} // namespace dplx::dlog
