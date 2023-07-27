
// Copyright 2021-2023 Henrik Steffen Gaßmann
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <string>

#include <catch2/catch_tostring.hpp>
#include <fmt/core.h>
#include <fmt/format.h>

#include <dplx/predef/compiler.h>

#include <dplx/dlog/disappointment.hpp>

#ifdef DPLX_COMP_GNUC_AVAILABLE
#pragma GCC diagnostic ignored "-Wmissing-declarations"
#endif

namespace dplx
{
}

namespace dlog_tests
{

using namespace dplx;

namespace detail
{

template <typename T>
concept is_fmt_formattable = fmt::is_formattable<T>::value;

inline auto format_error(dlog::system_error::status_code<void> const &esc)
        -> std::string
{
    auto const domainNameRef = esc.domain().name();
    auto const messageRef = esc.message();
    return fmt::format(
            "{{fail[{}]: {}}}",
            std::string_view(domainNameRef.data(), domainNameRef.size()),
            std::string_view(messageRef.data(), messageRef.size()));
}

} // namespace detail

} // namespace dlog_tests

template <typename T>
    requires dlog_tests::detail::is_fmt_formattable<T const &>
struct Catch::StringMaker<dplx::dlog::result<T>>
{
    static auto convert(dplx::dlog::result<T> const &rx) -> std::string
    {
        if (rx.has_value())
        {
            return fmt::format("{{rx: {}}}", rx.assume_value());
        }

        return dlog_tests::detail::format_error(rx.assume_error());
    }
};

template <>
struct Catch::StringMaker<dplx::dlog::result<void>>
{
    static auto convert(dplx::dlog::result<void> const &rx) -> std::string
    {
        using namespace std::string_literals;
        if (rx.has_value())
        {
            return "{rx: success<void>}"s;
        }

        return dlog_tests::detail::format_error(rx.assume_error());
    }
};
