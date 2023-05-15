
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "status-code/status_code_domain.hpp"

SYSTEM_ERROR2_NAMESPACE_BEGIN

namespace detail
{
template <class StringView, template <class> class Formatter>
struct string_ref_formatter : private Formatter<StringView>
{
private:
    using base = Formatter<StringView>;

public:
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) -> typename ParseContext::iterator
    {
        return base::parse(ctx);
    }

    template <typename FormatContext>
    auto format(status_code_domain::string_ref const &str, FormatContext &ctx)
            -> typename FormatContext::iterator
    {
        return base::format(StringView(str.data(), str.size()), ctx);
    }
};
} // namespace detail

SYSTEM_ERROR2_NAMESPACE_END

#if __cpp_lib_format >= 202106L
#include <format>

SYSTEM_ERROR2_NAMESPACE_BEGIN
namespace detail
{
template <typename T>
using std_formatter = std::formatter<T, char>;
}
SYSTEM_ERROR2_NAMESPACE_END

template <>
struct std::formatter<SYSTEM_ERROR2_NAMESPACE::status_code_domain::string_ref,
                      char>
    : SYSTEM_ERROR2_NAMESPACE::detail::string_ref_formatter<
              std::string_view,
              SYSTEM_ERROR2_NAMESPACE::detail::std_formatter>
{
};
#endif

#include <fmt/core.h>

SYSTEM_ERROR2_NAMESPACE_BEGIN
namespace detail
{
template <typename T>
using fmt_formatter = fmt::formatter<T, char>;
}
SYSTEM_ERROR2_NAMESPACE_END

template <>
struct fmt::formatter<SYSTEM_ERROR2_NAMESPACE::status_code_domain::string_ref,
                      char>
    : SYSTEM_ERROR2_NAMESPACE::detail::string_ref_formatter<
              fmt::string_view,
              SYSTEM_ERROR2_NAMESPACE::detail::fmt_formatter>
{
};
