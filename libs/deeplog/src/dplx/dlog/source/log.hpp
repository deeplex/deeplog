
// Copyright Henrik Steffen Gaßmann 2021, 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstdint>
#include <cstring>

#if __cpp_lib_source_location >= 201'907L
#include <source_location>
#endif

#include <fmt/core.h>

#include <dplx/dlog/config.hpp>
#include <dplx/dlog/core/strong_types.hpp>
#include <dplx/dlog/detail/any_loggable_ref.hpp>
#include <dplx/dlog/fwd.hpp>
#include <dplx/dlog/loggable.hpp>
#include <dplx/dlog/source/log_context.hpp>

namespace dplx::dlog::detail
{

struct log_location
{
    char const *filename;
    std::int_least32_t line;
    std::int_least16_t filenameSize;
};

#if DPLX_DLOG_USE_SOURCE_LOCATION
constexpr auto from_source_location(std::source_location const &current)
        -> log_location
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
    log_location location;
    std::uint_least16_t num_arguments;
    severity sev;
};

template <typename... Args>
class stack_log_args : public log_args
{
    static_assert(sizeof...(Args) <= UINT_LEAST16_MAX);

public:
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    detail::any_loggable_ref_storage const values[sizeof...(Args)];
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    detail::any_loggable_ref_storage_id const types[sizeof...(Args)];

    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
#if DPLX_DLOG_USE_SOURCE_LOCATION
    stack_log_args(fmt::string_view msg,
                   severity xsev,
                   std::source_location const &loc,
                   Args const &...args)
        : log_args(log_args{
                  msg,
                  values,
                  types,
                  detail::from_source_location(loc),
                  static_cast<std::uint_least16_t>(sizeof...(Args)),
                  xsev,
          })
        , values{detail::any_loggable_ref_storage_type_of_t<
                  detail::any_loggable_ref_storage_tag<Args>>(args)...}
        , types{detail::any_loggable_ref_storage_tag<Args>...}
    {
    }
#else
    stack_log_args(fmt::string_view msg,
                   severity xsev,
                   detail::log_location loc,
                   Args const &...args)
        : log_args(log_args{
                  msg,
                  values,
                  types,
                  loc,
                  static_cast<std::uint_least16_t>(sizeof...(Args)),
                  xsev,
          })
        , values{detail::any_loggable_ref_storage_type_of_t<
                  detail::any_loggable_ref_storage_tag<Args>>(args)...}
        , types{detail::any_loggable_ref_storage_tag<Args>...}
    {
    }
#endif
    // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)

    stack_log_args(stack_log_args const &) = delete;
    auto operator=(stack_log_args const &) -> stack_log_args & = delete;
};

template <>
class stack_log_args<> : public log_args
{
public:
#if DPLX_DLOG_USE_SOURCE_LOCATION
    stack_log_args(fmt::string_view msg,
                   severity xsev,
                   std::source_location const &loc)
        : log_args(log_args{
                  msg,
                  nullptr,
                  nullptr,
                  detail::from_source_location(loc),
                  0U,
                  xsev,
          })
    {
    }
#else
    stack_log_args(fmt::string_view msg,
                   severity xsev,
                   detail::log_location loc)
        : log_args(log_args{
                  msg,
                  nullptr,
                  nullptr,
                  loc,
                  0U,
                  xsev,
          })
    {
    }
#endif
};

} // namespace dplx::dlog::detail

namespace dplx::dlog
{

namespace detail
{

auto vlog(log_context const &logCtx, log_args const &args) noexcept
        -> result<void>;

} // namespace detail

template <typename... Args>
    requires(... && loggable<Args>)
[[nodiscard]] inline auto
log(log_context const &ctx,
    severity sev,
    fmt::format_string<reification_type_of_t<Args>...> message,
#if DPLX_DLOG_USE_SOURCE_LOCATION
    std::source_location const &location,
#else
    detail::log_location const &location,
#endif
    Args const &...args) noexcept -> result<void>
{
    if (sev < ctx.threshold()) [[unlikely]]
    {
        return outcome::success();
    }

    return detail::vlog(ctx, detail::stack_log_args<Args...>{
                                     message, sev, location, args...});
}

template <typename... Args>
    requires(... && loggable<Args>)
[[nodiscard]] inline auto
log(log_record_port &port,
    severity sev,
    fmt::format_string<reification_type_of_t<Args>...> message,
#if DPLX_DLOG_USE_SOURCE_LOCATION
    std::source_location const &location,
#else
    detail::log_location const &location,
#endif
    Args const &...args) noexcept -> result<void>
{
    log_context ctx(port);
    if (sev < ctx.threshold()) [[unlikely]]
    {
        return outcome::success();
    }

    return detail::vlog(ctx, detail::stack_log_args<Args...>{
                                     message, sev, location, args...});
}

} // namespace dplx::dlog
