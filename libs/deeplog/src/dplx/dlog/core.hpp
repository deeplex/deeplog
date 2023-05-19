
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
#include <dplx/dlog/definitions.hpp>

namespace dplx::dlog
{

// NOLINTBEGIN(cppcoreguidelines-pro-type-member-init)

struct serialized_info_base
{
    bytes raw_data;
};

struct serialized_unknown_message_info : serialized_info_base
{
};
struct serialized_record_info : serialized_info_base
{
    log_clock::time_point timestamp;
    severity message_severity;
};
struct serialized_span_start_info : serialized_info_base
{
};
struct serialized_span_end_info : serialized_info_base
{
};
struct serialized_malformed_message_info : serialized_info_base
{
};

// NOLINTEND(cppcoreguidelines-pro-type-member-init)

using serialized_message_info
        = boost::variant2::variant<serialized_unknown_message_info,
                                   serialized_record_info,
                                   serialized_span_start_info,
                                   serialized_span_end_info,
                                   serialized_malformed_message_info>;

class sink_frontend_base
{
protected:
    // NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
    severity mThreshold;
    bool mActive;
    // NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

public:
    virtual ~sink_frontend_base() = default;

protected:
    sink_frontend_base(sink_frontend_base const &) = default;
    auto operator=(sink_frontend_base const &)
            -> sink_frontend_base & = default;

    sink_frontend_base(sink_frontend_base &&) noexcept = default;
    auto operator=(sink_frontend_base &&) noexcept
            -> sink_frontend_base & = default;

    explicit sink_frontend_base(severity threshold) noexcept
        : mThreshold(threshold)
        , mActive(true)
    {
    }

public:
    [[nodiscard]] auto try_consume(
            std::size_t const binarySize,
            std::span<serialized_message_info const> const &messages) noexcept
            -> bool
    {
        if (!mActive) [[unlikely]]
        {
            return false;
        }
        return (mActive = do_try_consume(binarySize, messages));
    }
    [[nodiscard]] auto is_active() const noexcept -> bool
    {
        return mActive;
    }

    [[nodiscard]] auto try_sync() noexcept -> bool
    {
        if (!mActive) [[unlikely]]
        {
            return false;
        }
        return (mActive = do_try_sync());
    }

private:
    [[nodiscard]] virtual auto
    do_try_consume(std::size_t binarySize,
                   std::span<serialized_message_info const> messages) noexcept
            -> bool
            = 0;

    [[nodiscard]] virtual auto do_try_sync() noexcept -> bool
    {
        return true;
    }
};

namespace detail
{

auto preparse_messages(std::span<bytes const> const &records,
                       std::span<serialized_message_info> parses) noexcept
        -> std::size_t;

void multicast_messages(std::span<std::unique_ptr<sink_frontend_base>> &sinks,
                        std::size_t binarySize,
                        std::span<serialized_message_info> parses) noexcept;

void sync_sinks(std::span<std::unique_ptr<sink_frontend_base>> sinks) noexcept;

template <std::size_t MaxBatchSize>
struct consume_record_fn
{
    std::span<std::unique_ptr<sink_frontend_base>> sinks{};

    void operator()(std::span<bytes const> records) noexcept
    {
        serialized_message_info parses[MaxBatchSize];
        auto binarySize = detail::preparse_messages(records, parses);
        detail::multicast_messages(sinks, binarySize,
                                   std::span(parses).first(records.size()));
    }
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
        , mSinks()
    {
    }

    auto connector() noexcept -> Bus &
    {
        return mBus;
    }

    auto retire_log_records() noexcept -> result<int>
    {
        detail::consume_record_fn<Bus::consume_batch_size> drain{mSinks};

        DPLX_TRY(mBus.consume_messages(drain));
        detail::sync_sinks(mSinks);
        return oc::success();
    }

    auto attach_sink(std::unique_ptr<sink_frontend_base> &&sink)
            -> sink_frontend_base *
    {
        auto *const ptr = sink.get();
        mSinks.push_back(static_cast<decltype(sink) &&>(sink));
        return ptr;
    }
    void remove_sink(sink_frontend_base *const which) noexcept
    {
        for (auto it = mSinks.begin(), end = mSinks.end(); it != end; ++it)
        {
            if (which == it->get())
            {
                mSinks.erase(it);
                break;
            }
        }
    }
    auto release_sink(sink_frontend_base *const which) noexcept
            -> sink_frontend_base *
    {
        for (auto it = mSinks.begin(), end = mSinks.end(); it != end; ++it)
        {
            if (which == it->get())
            {
                (void)it->release();
                mSinks.erase(it);
                break;
            }
        }
        return which;
    }
    void clear_sinks() noexcept
    {
        mSinks.clear();
    }

private:
};

} // namespace dplx::dlog
