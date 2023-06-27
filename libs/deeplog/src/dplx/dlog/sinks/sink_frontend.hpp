
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

#include <dplx/dlog/core/serialized_messages.hpp>
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

template <typename T>
concept sink_backend
        = std::movable<T> && std::derived_from<T, dp::output_buffer>;

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
    auto do_try_consume(
            std::size_t const binarySize,
            std::span<serialized_message_info const> const messages) noexcept
            -> bool override
    {
        (void)binarySize;
        return detail::concate_messages(mBackend, messages, mThreshold)
                .has_value();
    }
    auto do_try_sync() noexcept -> bool override
    {
        return oc::try_operation_has_value(mBackend.sync_output());
    }
};

} // namespace dplx::dlog
