
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstdint>
#include <cstring>

#include <fmt/core.h>

#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/detail/any_loggable_ref.hpp>
#include <dplx/dlog/loggable.hpp>

namespace dplx::dlog::detail
{

struct log_location
{
    char const *filename;
    std::int_least32_t line;
    std::int_least16_t filenameSize;
};

#if 0 && DPLX_DLOG_USE_SOURCE_LOCATION
// TODO: implement `make_location()` with `std::source_location`
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
    span_id owner;
    log_location location;
    std::uint_least16_t num_arguments;
    severity sev;
};

} // namespace dplx::dlog::detail
