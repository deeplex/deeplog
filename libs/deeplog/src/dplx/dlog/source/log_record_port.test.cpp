
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/source/log_record_port.hpp"

#include <catch2/catch_test_macros.hpp>

#include "test_utils.hpp"

namespace dlog_tests
{

static_assert(!std::destructible<dlog::log_record_port>);
static_assert(!std::default_initializable<dlog::log_record_port>);
static_assert(!std::copyable<dlog::log_record_port>);
static_assert(!std::movable<dlog::log_record_port>);

} // namespace dlog_tests
