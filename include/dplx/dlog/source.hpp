
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

#include <dplx/dp/decoder/api.hpp>
#include <dplx/dp/encoder/api.hpp>
#include <dplx/dp/encoder/core.hpp>

#include <dplx/dlog/arguments.hpp>
#include <dplx/dlog/concepts.hpp>
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

    template <typename Arg>
    auto encoded_arg_size(Arg const &arg) -> int
    {
        return dp::encoded_size_of(
                       argument<detail::remove_cref_t<Arg>>::type_id)
             + dp::encoded_size_of(arg);
    }

public:
    template <typename... Args>
    requires(... &&loggable_argument<Args, output_stream>) auto
    operator()(severity sev, std::u8string_view message, Args const &...args)
            -> result<void>
    {
        // layout:
        // array 5
        // +  ui   severity
        // +  ui64 timestamp
        // +  str  message
        // +  map  format args
        // +  map  attributes

        constexpr auto hasFmtArgs = sizeof...(Args) > 0;
        auto const timeStamp = log_clock::now();

        auto const encodedArraySize = 1u;
        auto const encodedPrefixSize = dp::encoded_size_of(sev)
                                     + dp::encoded_size_of(timeStamp)
                                     + dp::encoded_size_of(message);
        auto const encodedArgsSize
                = hasFmtArgs ? (dp::encoded_size_of(sizeof...(Args)) + ...
                                + encoded_arg_size(args))
                             : 0u;

        auto const encodedSize
                = encodedArraySize + encodedPrefixSize + encodedArgsSize;

        DPLX_TRY(auto &&outStream, mBus->write(mToken, encodedSize));

        DPLX_TRY(emit::array(outStream, 3u + hasFmtArgs));
        DPLX_TRY(dp::encode(outStream, sev));
        DPLX_TRY(dp::encode(outStream, timeStamp));
        DPLX_TRY(dp::encode(outStream, message));

        if constexpr (hasFmtArgs)
        {
            DPLX_TRY(emit::map(outStream, sizeof...(Args)));

            dp::result<void> rx = dp::oc::success();
            bool failed
                    = (...
                       || (dp::detail::try_extract_failure(
                                   dp::basic_encoder<resource_id,
                                                     output_stream>()(
                                           outStream, argument<Args>::type_id),
                                   rx)
                           || dp::detail::try_extract_failure(
                                   dp::basic_encoder<Args, output_stream>()(
                                           outStream, args),
                                   rx)));

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
