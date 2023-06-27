
// Copyright Henrik S. Gaßmann 2021-2023.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog.hpp"

#include <fmt/format.h>

namespace dplx::dlog::detail
{

[[noreturn]] void throw_fmt_format_error(char const *message)
{
    throw fmt::format_error(message);
}

} // namespace dplx::dlog::detail
