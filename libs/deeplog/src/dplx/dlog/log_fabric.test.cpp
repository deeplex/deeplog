
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/log_fabric.hpp"

#include <dplx/dlog/bus/mpsc_bus.hpp>

#include "test_utils.hpp"

namespace dlog_tests
{

static_assert(makable<dlog::log_fabric<dlog::mpsc_bus_handle>>);

}
