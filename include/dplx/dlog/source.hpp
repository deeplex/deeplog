
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <chrono>
#include <compare>
#include <concepts>
#include <string>
#include <type_traits>

#include <dplx/dp/encoder/api.hpp>
#include <dplx/dp/encoder/core.hpp>

#include <dplx/dlog/arguments.hpp>
#include <dplx/dlog/concepts.hpp>
#include <dplx/dlog/core.hpp>
#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/log_bus.hpp>
#include <dplx/dlog/log_clock.hpp>

namespace dplx::dlog
{

template <bus T>
class logger
{
    T *mBus;
    /*[[no_unique_address]]*/ typename T::logger_token mToken;

public:
    severity threshold;

    logger() noexcept = default;

    logger(T &targetBus, severity threshold = default_threshold) noexcept
        : mBus(&targetBus)
        , mToken{}
        , threshold(threshold)
    {
    }

private:
    using output_stream = typename T::output_stream;
    using emit = dp::item_emitter<output_stream>;

public:
    template <typename... Args>
        requires(... &&loggable_argument<Args, output_stream>)
    auto operator()(severity sev,
                    std::u8string_view message,
                    Args const &...args) -> result<void>
    {
        // layout:
        // array 5
        // +  ui    severity
        // +  ui64  timestamp
        // +  str   message
        // +  map   attributes
        // +  array format args

        constexpr auto hasFmtArgs = sizeof...(Args) > 0U;
        auto const timeStamp = log_clock::now();

        using encoded_size_type = decltype((0u + ... + dp::tag_invoke_result_t<
                dp::encoded_size_of_fn, argument<Args> &&>{}));

        constexpr encoded_size_type encodedArraySize
                = dp::additional_information_size(5U);
        constexpr encoded_size_type encodedAttrMapSize
                = dp::additional_information_size(0U);
        encoded_size_type const encodedPrefixSize
                = dp::encoded_size_of(sev)
                                     + dp::encoded_size_of(timeStamp)
                                     + dp::encoded_size_of(message);

        auto const encodedArgsSize
                = hasFmtArgs ? (
                          dp::additional_information_size(sizeof...(Args)) + ...
                          + static_cast<encoded_size_type>(
                                  dp::encoded_size_of(argument<Args>{args})))
                             : 0U;

        auto const encodedSize = encodedArraySize + encodedPrefixSize
                               + encodedAttrMapSize + encodedArgsSize;

        DPLX_TRY(auto &&outStream,
                 mBus->write(mToken, static_cast<unsigned>(encodedSize)));

        DPLX_TRY(emit::array(outStream, 3U + (hasFmtArgs ? 2U : 0U)));
        DPLX_TRY(dp::encode(outStream, sev));
        DPLX_TRY(dp::encode(outStream, timeStamp));
        DPLX_TRY(dp::encode(outStream, message));

        if constexpr (hasFmtArgs)
        {
            DPLX_TRY(emit::map(outStream, 0U));
            DPLX_TRY(emit::array(outStream, sizeof...(Args)));

            dp::result<void> rx = dp::oc::success();
            bool failed = (...
                           || dp::detail::try_extract_failure(
                                   dp::basic_encoder<argument<Args>,
                                                     output_stream>()(
                                           outStream, argument<Args>{args}),
                                   rx));

            mBus->commit(mToken);
            if (failed)
            {
                return std::move(rx).as_failure();
            }
        }
        else
        {
            mBus->commit(mToken);
        }

        return oc::success();
    }
};

} // namespace dplx::dlog

#define DLOG_GENERIC(log, severity, message, ...)                              \
    do                                                                         \
    { /* due to shadowing this name isn't required to be unique */             \
        if (auto &&_dlog_materialized_temporary_ = log;                        \
            _dlog_materialized_temporary_.threshold >= severity)               \
            (void)_dlog_materialized_temporary_(severity, message,             \
                                                __VA_ARGS__);                  \
    } while (0)

#define DLOG_CRITICAL(log, message, ...)                                       \
    DLOG_GENERIC(log, ::dplx::dlog::severity::critical, __VA_ARGS__)
#define DLOG_ERROR(log, message, ...)                                          \
    DLOG_GENERIC(log, ::dplx::dlog::severity::error, __VA_ARGS__)
#define DLOG_WARN(log, message, ...)                                           \
    DLOG_GENERIC(log, ::dplx::dlog::severity::warn, __VA_ARGS__)
#define DLOG_INFO(log, message, ...)                                           \
    DLOG_GENERIC(log, ::dplx::dlog::severity::info, __VA_ARGS__)
#define DLOG_DEBUG(log, message, ...)                                          \
    DLOG_GENERIC(log, ::dplx::dlog::severity::debug, __VA_ARGS__)
#define DLOG_TRACE(log, message, ...)                                          \
    DLOG_GENERIC(log, ::dplx::dlog::severity::trace, __VA_ARGS__)
