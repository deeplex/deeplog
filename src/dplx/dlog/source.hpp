
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

#include <boost/config.hpp>

#include <dplx/dp/encoder/api.hpp>
#include <dplx/dp/encoder/core.hpp>
#include <dplx/dp/encoder/std_path.hpp>

#include <dplx/dlog/arguments.hpp>
#include <dplx/dlog/attributes.hpp>
#include <dplx/dlog/concepts.hpp>
#include <dplx/dlog/core.hpp>
#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/log_bus.hpp>
#include <dplx/dlog/log_clock.hpp>

namespace dplx::dlog
{

namespace detail
{

template <typename T, typename Stream>
struct log_arg_dispatch
{
    using xtype = argument<T>;
    static constexpr unsigned is_attribute = 0U;

    using encoded_size_of_t
            = dp::tag_invoke_result_t<dp::encoded_size_of_fn, xtype &&>;
    static constexpr auto encoded_size_of(T const &value) noexcept
    {
        return dp::encoded_size_of(xtype{value});
    }

    static BOOST_FORCEINLINE auto
    arg_encode(dp::result<void> &rx, Stream &outStream, T const &value) -> bool
    {
        rx = dp::encode(outStream, argument<T>{value});
        return rx.has_value();
    }
    static BOOST_FORCEINLINE auto
    attr_encode(dp::result<void> &, Stream &, T const &) noexcept -> bool
    {
        return true;
    }
};
template <resource_id Id, typename T, typename ReType, typename Stream>
struct log_arg_dispatch<basic_attribute<Id, T, ReType>, Stream>
{
    using xtype = basic_attribute<Id, T, ReType>;
    static constexpr unsigned is_attribute = 1U;

    using encoded_size_of_t
            = dp::tag_invoke_result_t<dp::encoded_size_of_fn, T const &>;
    static constexpr auto encoded_size_of(xtype const &attr) noexcept
    {
        return dp::encoded_size_of(attr.id) + dp::encoded_size_of(attr.value);
    }

    static BOOST_FORCEINLINE auto
    arg_encode(dp::result<void> &, Stream &, xtype const &) noexcept -> bool
    {
        return true;
    }
    static BOOST_FORCEINLINE auto attr_encode(dp::result<void> &rx,
                                              Stream &outStream,
                                              xtype const &attr) -> bool
    {
        // there is no/we don't use an encoder for basic_attribute, because we
        // encode id value pairs inside a map
        rx = dp::encode(outStream, attr.id);
        if (rx.has_failure())
        {
            return false;
        }
        rx = dp::encode(outStream, attr.value);
        return rx.has_value();
    }
};

} // namespace detail

template <bus T>
class logger
{
    T *mBus;
    /*[[no_unique_address]]*/ typename T::logger_token mToken;

public:
    severity threshold;

    logger() noexcept = default;

    logger(T &targetBus, severity thresholdInit = default_threshold) noexcept
        : mBus(&targetBus)
        , mToken{}
        , threshold(thresholdInit)
    {
    }

private:
    using output_stream = typename T::output_stream;
    using emit = dp::item_emitter<output_stream>;
    template <typename Arg>
    using dispatch = detail::log_arg_dispatch<Arg, output_stream>;

public:
    template <typename... Args>
        requires(...
                 && (loggable_attribute<Args, output_stream>
                     || loggable_argument<Args, output_stream>))
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

        using encoded_size_type = decltype((
                0U + ... + typename dispatch<Args>::encoded_size_of_t{}));
        constexpr auto numAttrs = (0U + ... + dispatch<Args>::is_attribute);
        constexpr auto hasAttrs = sizeof...(Args) > 0U;
        constexpr auto numFmtArgs = sizeof...(Args) - numAttrs;
        constexpr auto hasFmtArgs = numFmtArgs > 0U;

        auto const timeStamp = log_clock::now();

        constexpr encoded_size_type encodedArraySize
                = dp::additional_information_size(5U);
        encoded_size_type const encodedPrefixSize
                = dp::encoded_size_of(sev) + dp::encoded_size_of(timeStamp)
                + dp::encoded_size_of(message);

        constexpr encoded_size_type encodedAttrMapSize
                = hasAttrs ? dp::additional_information_size(numAttrs) : 0U;
        constexpr encoded_size_type encodedFmtArgsArraySize
                = hasFmtArgs ? dp::additional_information_size(numFmtArgs) : 0U;

        encoded_size_type const encodedArgsSize
                = (encoded_size_type{} + ...
                   + dispatch<Args>::encoded_size_of(args));

        encoded_size_type const encodedSize
                = encodedArraySize + encodedPrefixSize + encodedAttrMapSize
                + encodedFmtArgsArraySize + encodedArgsSize;

        DPLX_TRY(auto &&outStream,
                 mBus->write(mToken, static_cast<unsigned>(encodedSize)));
        bus_write_lock<T> busLock(*mBus, mToken);

        DPLX_TRY(emit::array(outStream, 3U + (hasAttrs ? 1U : 0U)
                                                + (hasFmtArgs ? 1U : 0U)));
        DPLX_TRY(dp::encode(outStream, sev));
        DPLX_TRY(dp::encode(outStream, timeStamp));
        DPLX_TRY(dp::encode(outStream, message));

        if constexpr (hasAttrs)
        {
            DPLX_TRY(emit::map(outStream, numAttrs));

            if constexpr (numAttrs > 0U)
            {
                dp::result<void> rx = dp::oc::success();
                bool succeeded
                        = (...
                           && dispatch<Args>::attr_encode(rx, outStream, args));

                if (!succeeded)
                {
                    return std::move(rx).as_failure();
                }
            }
        }
        if constexpr (hasFmtArgs)
        {
            DPLX_TRY(emit::array(outStream, numFmtArgs));

            dp::result<void> rx = dp::oc::success();
            bool succeeded
                    = (... && dispatch<Args>::arg_encode(rx, outStream, args));

            if (!succeeded)
            {
                return std::move(rx).as_failure();
            }
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
            (void)_dlog_materialized_temporary_(                               \
                    severity, message, ::dplx::dlog::attr::file{__FILE__},     \
                    ::dplx::dlog::attr::line{__LINE__}, __VA_ARGS__);          \
    } while (0)

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
