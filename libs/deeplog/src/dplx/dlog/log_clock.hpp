
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <chrono>
#include <cstdint>

#include <dplx/dp.hpp>

namespace dplx::dlog
{

class log_clock
{
public:
    using internal_clock
            = std::conditional_t<std::chrono::high_resolution_clock::is_steady,
                                 std::chrono::high_resolution_clock,
                                 std::chrono::steady_clock>;

    struct epoch_info
    {
        std::chrono::system_clock::time_point system_reference;
        internal_clock::time_point steady_reference;

        epoch_info() = default;
        explicit epoch_info(
                std::chrono::system_clock::time_point systemReference,
                internal_clock::time_point steadyReference) noexcept
            : system_reference(systemReference)
            , steady_reference(steadyReference)
        {
        }

        template <typename Duration>
        [[nodiscard]] auto to_sys(std::uint64_t nanoseconds) const noexcept
                -> std::chrono::time_point<
                        std::chrono::system_clock,
                        std::common_type_t<
                                Duration,
                                std::chrono::duration<std::int64_t, std::nano>>>
        {
            std::chrono::duration<std::uint64_t, std::nano> sinceEpoch(
                    nanoseconds);

            auto const internalSinceEpoch
                    = sinceEpoch - steady_reference.time_since_epoch();
            auto const systemTime = system_reference + internalSinceEpoch;

            using requested_duration = std::common_type_t<
                    Duration, std::chrono::duration<std::int64_t, std::nano>>;
            return time_point_cast<requested_duration>(systemTime);
        }
    };

private:
    static epoch_info const epoch_;

public:
    using rep = std::uint64_t;
    using period = std::nano;
    using duration = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<log_clock, duration>;

    static constexpr bool is_steady = internal_clock::is_steady;

    static auto now() noexcept -> time_point
    {
        return time_point(duration_cast<duration>(
                internal_clock::now().time_since_epoch()));
    }

    static auto epoch() noexcept -> epoch_info
    {
        return epoch_;
    }

    template <typename Duration>
    static auto
    to_sys(std::chrono::time_point<log_clock, Duration> const &t) noexcept
            -> std::chrono::time_point<std::chrono::system_clock,
                                       std::common_type_t<Duration, duration>>
    {
        internal_clock::time_point internalTime(
                duration_cast<internal_clock::duration>(t.time_since_epoch()));

        auto const internalSinceEpoch = internalTime - epoch_.steady_reference;
        auto const systemTime = epoch_.system_reference + internalSinceEpoch;

        using requested_duration = std::common_type_t<Duration, duration>;
        return time_point_cast<requested_duration>(systemTime);
    }
    template <typename Duration>
    static auto from_sys(std::chrono::time_point<std::chrono::system_clock,
                                                 Duration> const &t) noexcept
            -> std::chrono::time_point<log_clock,
                                       std::common_type_t<Duration, duration>>
    {
        auto const systemSinceEpoch = t - epoch_.system_reference;
        auto const internalTime = epoch_.steady_reference + systemSinceEpoch;

        using requested_duration = std::common_type_t<Duration, duration>;

        return std::chrono::time_point<log_clock, requested_duration>(
                duration_cast<requested_duration>(
                        internalTime.time_since_epoch()));
    }
};

} // namespace dplx::dlog

template <>
class dplx::dp::codec<dplx::dlog::log_clock::time_point>
{
public:
    static auto size_of(emit_context &ctx,
                        dplx::dlog::log_clock::time_point value) noexcept
            -> std::uint64_t;
    static auto encode(emit_context &ctx,
                       dplx::dlog::log_clock::time_point value) noexcept
            -> result<void>;
    static auto decode(parse_context &ctx,
                       dplx::dlog::log_clock::time_point &outValue) noexcept
            -> result<void>;
};

template <>
class dplx::dp::codec<dplx::dlog::log_clock::epoch_info>
{
public:
    static auto size_of(emit_context &ctx,
                        dplx::dlog::log_clock::epoch_info value) noexcept
            -> std::uint64_t;
    static auto encode(emit_context &ctx,
                       dplx::dlog::log_clock::epoch_info value) noexcept
            -> result<void>;
    static auto decode(parse_context &ctx,
                       dplx::dlog::log_clock::epoch_info &outValue) noexcept
            -> result<void>;
};
