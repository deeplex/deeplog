
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <concepts>
#include <cstddef>
#include <span>
#include <utility>

#include <dplx/dp/fwd.hpp>
#include <dplx/make.hpp>

#include <dplx/dlog/core/serialized_messages.hpp>
#include <dplx/dlog/core/strong_types.hpp>
#include <dplx/dlog/fwd.hpp>

namespace dplx::dlog::detail
{

auto concate_messages(dp::output_buffer &out,
                      std::span<serialized_message_info const> const &messages,
                      severity threshold) noexcept -> result<void>;

}

namespace dplx::dlog
{

class sink_frontend_base
{
protected:
    // NOLINTBEGIN(cppcoreguidelines-non-private-member-variables-in-classes)
    system_error2::system_code mLastStatus;
    severity mThreshold;
    // NOLINTEND(cppcoreguidelines-non-private-member-variables-in-classes)

public:
    virtual ~sink_frontend_base() = default;

    sink_frontend_base(sink_frontend_base const &) = delete;
    auto operator=(sink_frontend_base const &) -> sink_frontend_base & = delete;

protected:
    sink_frontend_base(sink_frontend_base &&) noexcept = default;
    auto operator=(sink_frontend_base &&) noexcept
            -> sink_frontend_base & = default;

    explicit sink_frontend_base(severity threshold) noexcept
        : mLastStatus{}
        , mThreshold(threshold)

    {
    }

public:
    [[nodiscard]] auto try_consume(
            std::size_t const binarySize,
            std::span<serialized_message_info const> const &messages) noexcept
            -> bool
    {
        if (!is_active()) [[unlikely]]
        {
            return false;
        }
        auto consumeRx = do_consume(binarySize, messages);
        if (consumeRx.has_value()) [[likely]]
        {
            return true;
        }
        mLastStatus = std::move(consumeRx).assume_error();
        return false;
    }
    [[nodiscard]] auto is_active() const noexcept -> bool
    {
        return mThreshold < detail::disable_threshold && !mLastStatus.failure();
    }

    [[nodiscard]] auto try_sync() noexcept -> bool
    {
        if (!is_active()) [[unlikely]]
        {
            return false;
        }
        auto syncRx = do_sync();
        if (syncRx.has_value()) [[likely]]
        {
            return true;
        }
        mLastStatus = std::move(syncRx).assume_error();
        return false;
    }

    [[nodiscard]] auto try_finalize() noexcept -> bool
    {
        if (!is_active()) [[unlikely]]
        {
            return false;
        }
        auto finalizeRx = do_finalize();
        if (finalizeRx.has_value()) [[likely]]
        {
            mThreshold = detail::disable_threshold;
            return true;
        }
        mLastStatus = std::move(finalizeRx).assume_error();
        return false;
    }

    [[nodiscard]] auto last_status() const noexcept
            -> system_error2::status_code<void> const &
    {
        return mLastStatus;
    }
    void clear_last_status() noexcept
    {
        mLastStatus = {};
    }

private:
    virtual auto
    do_consume(std::size_t binarySize,
               std::span<serialized_message_info const> messages) noexcept
            -> result<void>
            = 0;

    virtual auto do_sync() noexcept -> result<void>
    {
        return outcome::success();
    }
    virtual auto do_finalize() noexcept -> result<void>
    {
        return outcome::success();
    }
};

} // namespace dplx::dlog

template <dplx::dlog::sink_backend Backend>
struct dplx::make<dplx::dlog::basic_sink_frontend<Backend>>
{
    dlog::severity threshold;
    make<Backend> backend;

    auto operator()() const noexcept
            -> result<dlog::basic_sink_frontend<Backend>>;
};

namespace dplx::dlog
{

template <sink_backend Backend>
class basic_sink_frontend : public sink_frontend_base
{
    Backend mBackend;

public:
    using backend_type = Backend;

    basic_sink_frontend(basic_sink_frontend const &) = delete;
    auto operator=(basic_sink_frontend const &)
            -> basic_sink_frontend & = delete;

    basic_sink_frontend(basic_sink_frontend &&) noexcept = default;
    auto operator=(basic_sink_frontend &&) noexcept
            -> basic_sink_frontend & = default;

    basic_sink_frontend(severity threshold, backend_type backend)
        : sink_frontend_base(threshold)
        , mBackend(std::move(backend))
    {
    }

    auto backend() noexcept -> backend_type &
    {
        return mBackend;
    }
    auto backend() const noexcept -> backend_type const &
    {
        return mBackend;
    }

private:
    auto
    do_consume(std::size_t const binarySize,
               std::span<serialized_message_info const> const messages) noexcept
            -> result<void> override
    {
        (void)binarySize;
        DPLX_TRY(detail::concate_messages(mBackend, messages, mThreshold));
        return outcome::success();
    }
    auto do_sync() noexcept -> result<void> override
    {
        DPLX_TRY(mBackend.sync_output());
        return outcome::success();
    }
    auto do_finalize() noexcept -> result<void> override
    {
        if constexpr (requires {
                          { mBackend.finalize() } -> cncr::tryable;
                      })
        {
            DPLX_TRY(mBackend.finalize());
        }
        return outcome::success();
    }
};

} // namespace dplx::dlog

template <dplx::dlog::sink_backend Backend>
auto dplx::make<dplx::dlog::basic_sink_frontend<Backend>>::operator()()
        const noexcept -> result<dlog::basic_sink_frontend<Backend>>
{
    using namespace dplx::dlog;

    DPLX_TRY(auto &&backend_, backend());
    return result<basic_sink_frontend<Backend>>(
            std::in_place_type<basic_sink_frontend<Backend>>, threshold,
            std::move(backend_));
}
