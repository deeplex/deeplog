
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

#include <dplx/dp/api.hpp>
#include <dplx/dp/codecs/core.hpp>
#include <dplx/dp/fwd.hpp>
#include <dplx/dp/items/emit_core.hpp>

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

template <typename T>
struct log_arg_dispatch
{
    using xtype = argument<T>;
    static constexpr unsigned is_attribute = 0U;

    static constexpr auto encoded_size_of(T const &value) noexcept
            -> std::uint64_t
    {
        return dp::encoded_size_of(xtype{value});
    }

    static BOOST_FORCEINLINE auto arg_encode(dp::result<void> &rx,
                                             dp::emit_context &ctx,
                                             T const &value) -> bool
    {
        rx = dp::encode(ctx, argument<T>{value});
        return rx.has_value();
    }
    static BOOST_FORCEINLINE auto attr_encode(dp::result<void> &,
                                              dp::emit_context &,
                                              T const &) noexcept -> bool
    {
        return true;
    }
};
template <resource_id Id, typename T, typename ReType>
struct log_arg_dispatch<basic_attribute<Id, T, ReType>>
{
    using xtype = basic_attribute<Id, T, ReType>;
    static constexpr unsigned is_attribute = 1U;

    static constexpr auto encoded_size_of(xtype const &attr) noexcept
            -> std::uint64_t
    {
        return dp::encoded_size_of(attr.id) + dp::encoded_size_of(attr.value);
    }

    static BOOST_FORCEINLINE auto arg_encode(dp::result<void> &,
                                             dp::emit_context &,
                                             xtype const &) noexcept -> bool
    {
        return true;
    }
    static BOOST_FORCEINLINE auto attr_encode(dp::result<void> &rx,
                                              dp::emit_context &ctx,
                                              xtype const &attr) -> bool
    {
        // there is no/we don't use an encoder for basic_attribute, because we
        // encode id value pairs inside a map
        rx = dp::encode(ctx, attr.id);
        if (rx.has_failure())
        {
            return false;
        }
        rx = dp::encode(ctx, attr.value);
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
    template <typename Arg>
    using dispatch = detail::log_arg_dispatch<Arg>;
    using encoded_size_type = std::uint64_t;

public:
    template <typename... Args>
        requires(... && (loggable_attribute<Args> || loggable_argument<Args>))
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
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

        constexpr auto numAttrs = (0U + ... + dispatch<Args>::is_attribute);
        constexpr auto hasAttrs = sizeof...(Args) > 0U;
        constexpr auto numFmtArgs = sizeof...(Args) - numAttrs;
        constexpr auto hasFmtArgs = numFmtArgs > 0U;

        auto const timeStamp = log_clock::now();

        constexpr encoded_size_type encodedArraySize
                = dp::encoded_item_head_size<dp::type_code::posint>(5U);
        encoded_size_type const encodedPrefixSize
                = dp::encoded_size_of(sev) + dp::encoded_size_of(timeStamp)
                + dp::encoded_size_of(message);

        constexpr encoded_size_type encodedAttrMapSize
                = hasAttrs ? dp::encoded_item_head_size<dp::type_code::posint>(
                          numAttrs)
                           : 0U;
        constexpr encoded_size_type encodedFmtArgsArraySize
                = hasFmtArgs
                        ? dp::encoded_item_head_size<dp::type_code::posint>(
                                numFmtArgs)
                        : 0U;

        auto const encodedArgsSize = (encoded_size_type{} + ...
                                      + dispatch<Args>::encoded_size_of(args));

        encoded_size_type const encodedSize
                = encodedArraySize + encodedPrefixSize + encodedAttrMapSize
                + encodedFmtArgsArraySize + encodedArgsSize;

        DPLX_TRY(auto &&outBuffer,
                 mBus->write(mToken, static_cast<unsigned>(encodedSize)));
        bus_write_lock<T> busLock(*mBus, mToken);

        auto &&outBufferable = dp::get_output_buffer(outBuffer);
        dp::emit_context ctx{outBufferable};
        DPLX_TRY(dp::emit_array(ctx, 3U + (hasAttrs ? 1U : 0U)
                                             + (hasFmtArgs ? 1U : 0U)));
        DPLX_TRY(dp::encode(ctx, sev));
        DPLX_TRY(dp::encode(ctx, timeStamp));
        DPLX_TRY(dp::encode(ctx, message));

        if constexpr (hasAttrs)
        {
            DPLX_TRY(dp::emit_map(ctx, numAttrs));

            if constexpr (numAttrs > 0U)
            {
                dp::result<void> rx = dp::oc::success();
                bool succeeded
                        = (... && dispatch<Args>::attr_encode(rx, ctx, args));

                if (!succeeded)
                {
                    return std::move(rx).as_failure();
                }
            }
        }
        if constexpr (hasFmtArgs)
        {
            DPLX_TRY(dp::emit_array(ctx, numFmtArgs));

            dp::result<void> rx = dp::oc::success();
            bool succeeded = (... && dispatch<Args>::arg_encode(rx, ctx, args));

            if (!succeeded)
            {
                return std::move(rx).as_failure();
            }
        }

        return oc::success();
    }
};

} // namespace dplx::dlog

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

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

// NOLINTEND(cppcoreguidelines-macro-usage)
