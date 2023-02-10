
// Copyright Henrik S. Gaßmann 2021-2023.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <fmt/format.h>

#include <dplx/dlog/llfio.hpp>

#include "test_dir.hpp"
#include "test_utils.hpp"

namespace dlog_tests
{

// we don't want to throw from within an initializer
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
constinit llfio::directory_handle test_dir{};

class TempDirectoryManager : public Catch::EventListenerBase
{
public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting(Catch::TestRunInfo const &) override
    {
        auto testFilesDir
                = llfio::directory({}, "_test-files",
                                   llfio::directory_handle::mode::write,
                                   llfio::directory_handle::creation::if_needed)
                          .value();

        test_dir = llfio::uniquely_named_directory(
                           testFilesDir, llfio::directory_handle::mode::write,
                           llfio::directory_handle::caching::all)
                           .value();

        fmt::print(
                "created unique test directory at '{}'\n",
                llfio::to_win32_path(test_dir, llfio::win32_path_namespace::dos)
                        .value()
                        .generic_string());
    }
    void testRunEnded(Catch::TestRunStats const &testRunStats) override
    {
        if (testRunStats.aborting)
        {
            return;
        }

        if (auto reduceRx = llfio::algorithm::reduce(std::move(test_dir));
            reduceRx.has_error())
        {
            reduceRx.assume_error().throw_exception();
        }
    }
};

CATCH_REGISTER_LISTENER(TempDirectoryManager)

} // namespace dlog_tests
