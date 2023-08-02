
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/log_fabric.hpp"

#include <algorithm>

namespace dplx::dlog::detail
{

void log_fabric_base::sync_sinks() noexcept
{
    std::partition(mSinks.begin(), mSinks.end(),
                   [](auto &sink) { return sink->try_sync(); });
}

auto log_fabric_base::attach_sink(std::unique_ptr<sink_frontend_base> &&sinkPtr)
        -> sink_frontend_base *
{
    auto *const ptr = sinkPtr.get();
    mSinks.push_back(std::move(sinkPtr));
    return ptr;
}

auto log_fabric_base::attach_sink(std::unique_ptr<sink_frontend_base> &&sinkPtr,
                                  std::nothrow_t) noexcept
        -> result<sink_frontend_base *>
try
{
    auto *const ptr = sinkPtr.get();
    mSinks.push_back(std::move(sinkPtr));
    return ptr;
}
catch (std::bad_alloc const &)
{
    return system_error2::errc::not_enough_memory;
}

auto log_fabric_base::destroy_sink(sink_frontend_base *which) noexcept
        -> result<void>
{
    auto const it = std::ranges::find(
            mSinks, which, [](sink_owner const &ptr) { return ptr.get(); });
    if (it == mSinks.end())
    {
        return errc::unknown_sink;
    }
    if (!it->get()->try_finalize())
    {
        return errc::sink_finalization_failed;
    }
    mSinks.erase(it);
    return outcome::success();
}

void log_fabric_base::remove_sink(sink_frontend_base *const which) noexcept
{
    std::erase_if(mSinks, [which](sink_owner const &ptr)
                  { return which == ptr.get(); });
}

auto log_fabric_base::release_sink(sink_frontend_base *const which) noexcept
        -> sink_frontend_base *
{
    auto const it = std::ranges::find(
            mSinks, which, [](sink_owner const &ptr) { return ptr.get(); });
    if (it != mSinks.end())
    {
        (void)it->release(); // returns `which`
        mSinks.erase(it);
    }
    return which;
}

void log_fabric_base::clear_sinks() noexcept
{
    mSinks.clear();
}

auto log_fabric_base::do_default_threshold() const noexcept -> severity
{
    return mDefaultThreshold;
}

auto log_fabric_base::do_threshold(
        char const *const scopeName,
        std::size_t const scopeNameSize) const noexcept -> severity
{
    if (auto const it
        = mThresholds.find(std::string_view{scopeName, scopeNameSize});
        it != mThresholds.end())
    {
        return it->second;
    }
    return mDefaultThreshold;
}

} // namespace dplx::dlog::detail
