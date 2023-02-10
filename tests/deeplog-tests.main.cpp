
// Copyright Henrik Steffen Gaßmann 2019
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE deeplog tests
#include "boost-test.hpp"
#include "test-utils.hpp"

// we don't want to throw from within an initializer
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
constinit dlog_tests::llfio::directory_handle dlog_tests::test_dir{};
struct dlog_test_dir_fixtures
{
    dlog_test_dir_fixtures()
    {
        using namespace dlog_tests;

        BOOST_TEST_CHECKPOINT("creating/opening test-files directory");
        auto testFilesDir
                = llfio::directory({}, "_test-files",
                                   llfio::directory_handle::mode::write,
                                   llfio::directory_handle::creation::if_needed)
                          .value();

        BOOST_TEST_CHECKPOINT("creating unique directory for test files");
        test_dir = llfio::uniquely_named_directory(
                           testFilesDir, llfio::directory_handle::mode::write,
                           llfio::directory_handle::caching::all)
                           .value();

        fmt::print(
                "created unique test directory at '{}'\n",
                llfio::to_win32_path(test_dir, llfio::win32_path_namespace::dos)
                        .value()
                        .string());
    }

    static void teardown()
    {
        using namespace dlog_tests;

        BOOST_TEST_CHECKPOINT("removing the unique directory for test files");
        if (auto reduceRx = llfio::algorithm::reduce(std::move(test_dir));
            reduceRx.has_error())
        {
            reduceRx.assume_error().throw_exception();
        }
    }
};
BOOST_TEST_GLOBAL_FIXTURE(dlog_test_dir_fixtures);
