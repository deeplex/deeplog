
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/log_fabric.hpp"

#include <algorithm>

#include <dplx/dlog/sink.hpp>

namespace dplx::dlog::detail
{

log_fabric_base::~log_fabric_base()
{
}

log_fabric_base::log_fabric_base() noexcept
{
}

log_fabric_base::log_fabric_base(log_fabric_base &&other) noexcept
    : log_record_port(std::move(other))
    , mSinks(std::move(other.mSinks))
{
}

auto log_fabric_base::operator=(log_fabric_base &&other) noexcept
        -> log_fabric_base &
{
    if (&other == this)
    {
        return *this;
    }
    log_record_port::operator=(std::move(other));
    mSinks = std::move(other.mSinks);
    return *this;
}

void log_fabric_base::sync_sinks() noexcept
{
    std::partition(mSinks.begin(), mSinks.end(),
                   [](auto &sink) { return sink->try_sync(); });
}

auto log_fabric_base::attach_sink(std::unique_ptr<sink_frontend_base> &&sink)
        -> sink_frontend_base *
{
    auto *const ptr = sink.get();
    mSinks.push_back(static_cast<decltype(sink) &&>(sink));
    return ptr;
}

void log_fabric_base::remove_sink(sink_frontend_base *const which) noexcept
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

auto log_fabric_base::release_sink(sink_frontend_base *const which) noexcept
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

void log_fabric_base::clear_sinks() noexcept
{
    mSinks.clear();
}

} // namespace dplx::dlog::detail
