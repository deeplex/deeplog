
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <chrono>
#include <string>
#include <string_view>

#include <fmt/format.h>

// tis a polyfill until c++20 changes to chrono and std::format get implemented
namespace dplx::dlog::detail
{

inline constexpr int iso8601_datetime_size = 13;
inline auto iso8601_datetime(std::chrono::system_clock::time_point timePoint)
        -> std::string
{
    using namespace std::string_view_literals;

    auto const date = std::chrono::floor<std::chrono::days>(timePoint);
    std::chrono::hh_mm_ss const timeOfDay{timePoint - date};
    std::chrono::year_month_day const calendar{date};

    return fmt::format("{:04}{:02}{:02}T{:02}{:02}{:02}"sv,
                       static_cast<int>(calendar.year()),
                       static_cast<unsigned>(calendar.month()),
                       static_cast<unsigned>(calendar.day()),
                       timeOfDay.hours().count(), timeOfDay.minutes().count(),
                       timeOfDay.seconds().count());
}

inline constexpr int iso8601_datetime_long_size = 26;
inline auto
iso8601_datetime_long(std::chrono::system_clock::time_point timePoint)
        -> std::string
{
    using namespace std::string_view_literals;

    auto const date = std::chrono::floor<std::chrono::days>(timePoint);
    std::chrono::hh_mm_ss const timeOfDay{timePoint - date};
    std::chrono::year_month_day const calendar{date};
    auto const frac = std::chrono::duration_cast<std::chrono::microseconds>(
            timeOfDay.subseconds());

    return fmt::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:06}"sv,
                       static_cast<int>(calendar.year()),
                       static_cast<unsigned>(calendar.month()),
                       static_cast<unsigned>(calendar.day()),
                       timeOfDay.hours().count(), timeOfDay.minutes().count(),
                       timeOfDay.seconds().count(), frac.count());
}

} // namespace dplx::dlog::detail
