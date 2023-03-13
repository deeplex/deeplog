
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

static_assert(!std::is_move_constructible_v<dlog::span_scope>);
static_assert(!std::is_copy_constructible_v<dlog::span_scope>);

TEST_CASE("can open a span scope")
{
    constexpr auto regionSize = 1 << 14;
    dlog::core core{
            dlog::mpsc_bus(test_dir, "ts1.dmsb", 4U, regionSize).value()};

    {
        dlog::span_scope log
                = DLOG_SPAN_START_ROOT_EX(core.connector(), "sample");
        DLOG_WARN_EX(log, "this should be attached to the root span");

        {
            dlog::span_scope innerLog = DLOG_SPAN_START_EX(log, "inner");
            DLOG_WARN_EX(innerLog,
                         "this should be attached to the inner span.");
        }
    }
}

#if !DPLX_DLOG_DISABLE_IMPLICIT_CONTEXT
TEST_CASE("implicit spans are respected")
{
    constexpr auto regionSize = 1 << 14;
    dlog::core core{
            dlog::mpsc_bus(test_dir, "ts2.dmsb", 4U, regionSize).value()};

    {
        dlog::span_scope log
                = dlog::span_scope::none(core.connector(), dlog::attach::yes);
        DLOG_WARN("this should be attached to the root span");

        {
            dlog::span_scope innerLog = DLOG_SPAN_ATTACH("inner");
            DLOG_WARN("this should be attached to the inner span.");
        }
    }
}
#endif

} // namespace dlog_tests
