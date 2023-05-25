
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>
#include <span>

#include <dplx/dlog/core/strong_types.hpp>

// span aliasse
namespace dplx::dlog
{

using bytes = std::span<std::byte const>;
using writable_bytes = std::span<std::byte>;

} // namespace dplx::dlog
