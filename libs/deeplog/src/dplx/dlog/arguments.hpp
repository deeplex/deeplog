
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstdint>
#include <cstring>
#include <source_location>

#include <fmt/core.h>

#include <dplx/dp/disappointment.hpp>
#include <dplx/dp/fwd.hpp>

#include <dplx/dlog/config.hpp>
#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/detail/any_loggable_ref.hpp>
#include <dplx/dlog/loggable.hpp>

#if DPLX_DLOG_USE_SOURCE_LOCATION
#include <source_location>
#endif

namespace dplx::dlog::detail
{

struct log_location
{
    char const *filename;
    std::int_least32_t line;
    std::int_least16_t filenameSize;
};

#if DPLX_DLOG_USE_SOURCE_LOCATION
consteval auto make_location(std::source_location current
                             = std::source_location::current()) -> log_location
{
    auto const filenameSize
            = std::char_traits<char>::length(current.file_name());
    assert(filenameSize <= INT_LEAST16_MAX);
    return {current.file_name(),
            static_cast<std::int_least32_t>(current.line()),
            static_cast<std::int_least16_t>(filenameSize)};
}
#else
consteval auto make_location(char const *filename,
                             std::uint_least32_t line) noexcept -> log_location
{
    auto const filenameSize = std::char_traits<char>::length(filename);
    assert(filenameSize <= INT_LEAST16_MAX);
    return {filename, static_cast<std::int_least32_t>(line),
            static_cast<std::int_least16_t>(filenameSize)};
}
#endif

} // namespace dplx::dlog::detail

namespace dplx::dlog::detail
{

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
class log_args
{
public:
    fmt::string_view message;
    detail::any_loggable_ref_storage const *message_parts;
    detail::any_loggable_ref_storage_id const *part_types;
    span_context owner;
    log_location location;
    std::uint_least16_t num_arguments;
    severity sev;
};

template <typename... Args>
class stack_log_args : public log_args
{
    static_assert(sizeof...(Args) <= UINT_LEAST16_MAX);

public:
    detail::any_loggable_ref_storage const values[sizeof...(Args)];
    detail::any_loggable_ref_storage_id const types[sizeof...(Args)];

    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    stack_log_args(fmt::string_view msg,
                   severity xsev,
                   span_context ctxId,
                   detail::log_location loc,
                   Args const &...args)
        : log_args(log_args{
                msg,
                values,
                types,
                ctxId,
                loc,
                static_cast<std::uint_least16_t>(sizeof...(Args)),
                xsev,
        })
        , values{detail::any_loggable_ref_storage_type_of_t<
                  detail::any_loggable_ref_storage_tag<Args>>(args)...}
        , types{detail::any_loggable_ref_storage_tag<Args>...}
    {
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)

    stack_log_args(stack_log_args const &) = delete;
    auto operator=(stack_log_args const &) -> stack_log_args & = delete;
};

template <>
class stack_log_args<> : public log_args
{
public:
    stack_log_args(fmt::string_view msg,
                   severity xsev,
                   span_context ctxId,
                   detail::log_location loc)
        : log_args(log_args{
                msg,
                nullptr,
                nullptr,
                ctxId,
                loc,
                0U,
                xsev,
        })
    {
    }
};

} // namespace dplx::dlog::detail
