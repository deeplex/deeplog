
// Copyright Henrik S. Gaßmann 2021-2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <dplx/dlog/llfio.hpp>

#include "test_utils.hpp"

namespace dlog_tests
{

namespace llfio = dlog::llfio;

// we don't want to throw from within an initializer
// initialized in dlog.test.cpp
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern llfio::directory_handle test_dir;

} // namespace dlog_tests
