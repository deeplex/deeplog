
// Copyright Henrik Steffen Gaßmann 2021, 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstdint>

#include <fmt/core.h>

#include <dplx/dlog/arguments.hpp>
#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/disappointment.hpp>
#include <dplx/dlog/log_bus.hpp>
#include <dplx/dlog/loggable.hpp>

namespace dplx::dlog
{

namespace detail
{

auto vlog(bus_handle &messageBus, log_args const &args) noexcept
        -> result<void>;

} // namespace detail

class span
{
    span_id mId{};
    bus_handle *mBus{};

public:
    // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
    severity threshold{};

    span(bus_handle &targetBus,
         severity thresholdInit = default_threshold) noexcept
        : mId{}
        , mBus(&targetBus)
        , threshold(thresholdInit)
    {
    }

    template <typename... Args>
        requires(... && loggable<Args>)
    [[nodiscard]] auto
    operator()(severity sev,
               fmt::format_string<reification_type_of_t<Args>...> message,
               detail::log_location location,
               Args const &...args) const noexcept -> result<void>
    {
        static_assert(sizeof...(Args) <= UINT_LEAST16_MAX);
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        detail::any_loggable_ref_storage const
                values[sizeof...(Args) > 0U ? sizeof...(Args) : 1U]
                = {detail::any_loggable_ref_storage_type_of_t<
                        detail::any_loggable_ref_storage_tag<Args>>(args)...};
        // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        detail::any_loggable_ref_storage_id const
                types[sizeof...(Args) > 0U ? sizeof...(Args) : 1U]
                = {detail::any_loggable_ref_storage_tag<Args>...};

        return detail::vlog(
                *mBus,
                detail::log_args{
                        message,
                        static_cast<detail::any_loggable_ref_storage const *>(
                                values),
                        static_cast<detail::any_loggable_ref_storage_id const
                                            *>(types),
                        mId,
                        location,
                        static_cast<std::uint_least16_t>(sizeof...(Args)),
                        sev,
                });
    }
};

} // namespace dplx::dlog

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

#if _MSVC_TRADITIONAL
#define DLOG_GENERIC(log, severity, message, ...)                              \
    do                                                                         \
    { /* due to shadowing this name isn't required to be unique */             \
        if (auto &&_dlog_materialized_temporary_ = (log);                      \
            _dlog_materialized_temporary_.threshold >= (severity))             \
            (void)_dlog_materialized_temporary_(                               \
                    (severity), (message), DPLX_DLOG_LOCATION, __VA_ARGS__);   \
    } while (0)
#else // _MSVC_TRADITIONAL
#define DLOG_GENERIC(log, severity, message, ...)                              \
    do                                                                         \
    { /* due to shadowing this name isn't required to be unique */             \
        if (auto &&_dlog_materialized_temporary_ = (log);                      \
            _dlog_materialized_temporary_.threshold >= (severity))             \
            (void)_dlog_materialized_temporary_(                               \
                    (severity), (message),                                     \
                    DPLX_DLOG_LOCATION __VA_OPT__(, __VA_ARGS__));             \
    } while (0)
#endif // _MSVC_TRADITIONAL

#define DLOG_CRITICAL(log, message, ...)                                       \
    DLOG_GENERIC(log, ::dplx::dlog::severity::critical, message, __VA_ARGS__)
#define DLOG_ERROR(log, message, ...)                                          \
    DLOG_GENERIC(log, ::dplx::dlog::severity::error, message, __VA_ARGS__)
#define DLOG_WARN(log, message, ...)                                           \
    DLOG_GENERIC(log, ::dplx::dlog::severity::warn, message, __VA_ARGS__)
#define DLOG_INFO(log, message, ...)                                           \
    DLOG_GENERIC(log, ::dplx::dlog::severity::info, message, __VA_ARGS__)
#define DLOG_DEBUG(log, message, ...)                                          \
    DLOG_GENERIC(log, ::dplx::dlog::severity::debug, message, __VA_ARGS__)
#define DLOG_TRACE(log, message, ...)                                          \
    DLOG_GENERIC(log, ::dplx::dlog::severity::trace, message, __VA_ARGS__)

// NOLINTEND(cppcoreguidelines-macro-usage)
