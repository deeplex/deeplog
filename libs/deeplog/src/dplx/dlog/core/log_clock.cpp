
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/core/log_clock.hpp"

#include <dplx/dp/api.hpp>
#include <dplx/dp/codecs/auto_tuple.hpp>
#include <dplx/dp/codecs/core.hpp>
#include <dplx/dp/codecs/std-chrono.hpp>

namespace dplx::dlog
{

log_clock::epoch_info const log_clock::epoch_(std::chrono::system_clock::now(),
                                              internal_clock::now());

// namespace
//{
//
// dp::tuple_def<dp::tuple_member_def<&log_clock::epoch_info::system_reference>{},
//               dp::tuple_member_def<&log_clock::epoch_info::steady_reference>{}>
//         log_epoch_info_descriptor{};
//
// }

} // namespace dplx::dlog

auto dplx::dp::codec<dplx::dlog::log_clock::time_point>::size_of(
        emit_context &ctx,
        dplx::dlog::log_clock::time_point const &value) noexcept
        -> std::uint64_t
{
    return dp::encoded_size_of(ctx, value.time_since_epoch());
}

auto dplx::dp::codec<dplx::dlog::log_clock::time_point>::encode(
        emit_context &ctx,
        dplx::dlog::log_clock::time_point const &value) noexcept -> result<void>
{
    return dp::encode(ctx, value.time_since_epoch());
}

auto dplx::dp::codec<dplx::dlog::log_clock::time_point>::decode(
        parse_context &ctx,
        dplx::dlog::log_clock::time_point &outValue) noexcept -> result<void>
{
    DPLX_TRY(auto timeSinceEpoch,
             dp::decode(as_value<dplx::dlog::log_clock::time_point::duration>,
                        ctx));
    outValue = dplx::dlog::log_clock::time_point{timeSinceEpoch};
    return outcome::success();
}

auto dplx::dp::codec<dplx::dlog::log_clock::epoch_info>::size_of(
        emit_context &ctx,
        dplx::dlog::log_clock::epoch_info const &value) noexcept
        -> std::uint64_t
{
    return dp::encoded_item_head_size<type_code::array>(2U)
         + dp::encoded_size_of(ctx, value.system_reference.time_since_epoch())
         + dp::encoded_size_of(ctx, value.steady_reference.time_since_epoch());

    // system_clock::time_point is missing a codec<>
    // return dp::size_of_tuple<dlog::log_epoch_info_descriptor>(ctx, value);
}

auto dplx::dp::codec<dplx::dlog::log_clock::epoch_info>::encode(
        emit_context &ctx,
        dplx::dlog::log_clock::epoch_info const &value) noexcept -> result<void>
{
    DPLX_TRY(dp::emit_array(ctx, 2U));

    DPLX_TRY(dp::encode(ctx, value.system_reference.time_since_epoch()));
    DPLX_TRY(dp::encode(ctx, value.steady_reference.time_since_epoch()));
    return outcome::success();

    // system_clock::time_point is missing a codec<>
    // return dp::encode_tuple<dlog::log_epoch_info_descriptor>(ctx, value);
}

auto dplx::dp::codec<dplx::dlog::log_clock::epoch_info>::decode(
        parse_context &ctx,
        dplx::dlog::log_clock::epoch_info &outValue) noexcept -> result<void>
{
    DPLX_TRY(dp::expect_item_head(ctx, type_code::array, 2U));

    using system_clock = std::chrono::system_clock;
    using internal_clock = dlog::log_clock::internal_clock;

    DPLX_TRY(auto sysRef, dp::decode(as_value<system_clock::duration>, ctx));
    DPLX_TRY(auto steadyRef,
             dp::decode(as_value<internal_clock::duration>, ctx));

    outValue = dlog::log_clock::epoch_info{
            system_clock::time_point{sysRef},
            internal_clock::time_point{steadyRef},
    };

    return outcome::success();

    // system_clock::time_point is missing a codec<>
    // DPLX_TRY(tuple_head_info headInfo,
    //          dp::decode_tuple_head(ctx, std::false_type{}));
    // return dp::decode_tuple_properties<dlog::log_epoch_info_descriptor>(
    //        ctx, outValue, headInfo.num_properties);
}
