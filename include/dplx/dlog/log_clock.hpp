
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstdint>

#include <chrono>

#include <dplx/dp/concepts.hpp>
#include <dplx/dp/encoder/api.hpp>
#include <dplx/dp/fwd.hpp>
#include <dplx/dp/type_code.hpp>

namespace dplx::dlog
{

class log_clock
{
    using internal_clock
            = std::conditional_t<std::chrono::high_resolution_clock::is_steady,
                                 std::chrono::high_resolution_clock,
                                 std::chrono::steady_clock>;

    struct epoch_info
    {
        std::chrono::system_clock::time_point system_reference;
        internal_clock::time_point steady_reference;

        explicit epoch_info()
            : system_reference(std::chrono::system_clock::now())
            , steady_reference(internal_clock::now())
        {
        }
    };

    static epoch_info epoch;

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

    template <typename Duration>
    static auto
    to_sys(std::chrono::time_point<log_clock, Duration> const &t) noexcept
            -> std::chrono::time_point<std::chrono::system_clock,
                                       std::common_type_t<Duration, duration>>
    {
        internal_clock::time_point internalTime(
                duration_cast<internal_clock::duration>(t.time_since_epoch()));

        auto const internalSinceEpoch = internalTime - epoch.steady_reference;
        auto const systemTime = epoch.system_reference + internalSinceEpoch;

        using requested_duration = std::common_type_t<Duration, duration>;
        return time_point_cast<requested_duration>(systemTime);
    }
    template <typename Duration>
    static auto from_sys(std::chrono::time_point<std::chrono::system_clock,
                                                 Duration> const &t) noexcept
            -> std::chrono::time_point<log_clock,
                                       std::common_type_t<Duration, duration>>
    {
        auto const systemSinceEpoch = t - epoch.system_reference;
        auto const internalTime = epoch.steady_reference + systemSinceEpoch;

        using requested_duration = std::common_type_t<Duration, duration>;

        return std::chrono::time_point<log_clock, requested_duration>(
                duration_cast<requested_duration>(
                        internalTime.time_since_epoch()));
    }
};

} // namespace dplx::dlog

namespace dplx::dp
{

template <output_stream Stream>
class basic_encoder<dlog::log_clock::time_point, Stream>
{
public:
    using value_type = dlog::log_clock::time_point;

    auto operator()(Stream &outStream, value_type value) -> result<void>
    {
        DPLX_TRY(auto writeProxy, write(outStream, 9u));

        auto const data = std::ranges::data(writeProxy);
        *data = to_byte(type_code::posint) | std::byte{27u};
        detail::store(data + 1, value.time_since_epoch().count());

        if constexpr (lazy_output_stream<Stream>)
        {
            DPLX_TRY(commit(outStream, writeProxy));
        }
        return oc::success();
    }
};

template <input_stream Stream>
class basic_decoder<dlog::log_clock::time_point, Stream>
{
public:
    using value_type = dlog::log_clock::time_point;

    auto operator()(Stream &inStream, value_type &value) -> result<void>
    {
        DPLX_TRY(auto readProxy, read(inStream, 9u));

        auto const data = std::ranges::data(readProxy);
        if (*data != to_byte(type_code::posint) | std::byte{27u})
        {
            return errc::item_type_mismatch;
        }
        auto const bits = detail::load<dlog::log_clock::rep>(data + 1);
        value = dlog::log_clock::time_point(dlog::log_clock::duration(bits));

        if constexpr (lazy_output_stream<Stream>)
        {
            DPLX_TRY(consume(inStream, readProxy));
        }
        return oc::success();
    }
};

constexpr auto tag_invoke(encoded_size_of_fn,
                          dlog::log_clock::time_point) noexcept -> unsigned
{
    return 9U;
}

} // namespace dplx::dp
