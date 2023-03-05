
// Copyright Henrik S. Gaßmann 2023.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/span_scope.hpp"

#include <catch2/catch_test_macros.hpp>

#include <dplx/dlog/bus/mpsc_bus.hpp>
#include <dplx/dlog/core.hpp>
#include <dplx/dlog/macros.hpp>

#include "test_dir.hpp"
#include "test_utils.hpp"

namespace dlog_tests
{

TEST_CASE("can open a span scope")
{
    constexpr auto regionSize = 1 << 14;
    dlog::core core{
            dlog::mpsc_bus(test_dir, "ts1.dmsb", 4U, regionSize).value()};

    {
        dlog::span_scope log = DLOG_OPEN_SPAN_EX(core.connector(), "sample");
        DLOG_WARN_EX(log, "this should be attached to the root span");

        {
            dlog::span_scope innerLog = DLOG_OPEN_SPAN_EX(log, "inner");
            DLOG_WARN_EX(innerLog,
                         "this should be attached to the inner span.");
        }
    }
}

} // namespace dlog_tests
