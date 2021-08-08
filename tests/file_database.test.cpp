
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include <dplx/dlog/file_database.hpp>

#include "boost-test.hpp"
#include "test-utils.hpp"

namespace dlog_tests
{

BOOST_AUTO_TEST_SUITE(file_db)

BOOST_AUTO_TEST_CASE(api)
{
    auto const dbName = llfio::utils::random_string(4u);
    auto const dbFullName
            = std::string{dbName}.append(dlog::file_database_handle::extension);
    auto const sinkFilePattern = dbName + ".{ctr}_{iso8601}.blog";

    auto createRx = dlog::file_database_handle::file_database(
            test_dir, dbFullName, sinkFilePattern);
    DPLX_REQUIRE_RESULT(createRx);
    auto &&db = std::move(createRx).assume_value();

    auto create2Rx = db.create_record_container();
    DPLX_REQUIRE_RESULT(create2Rx);

    // cleanup
    (void)create2Rx.assume_value().close();
    auto cleanupRx = db.unlink_all();
    DPLX_TEST_RESULT(cleanupRx);
}

BOOST_AUTO_TEST_CASE(reopen)
{
    auto const dbName = llfio::utils::random_string(4u);
    auto const dbFullName
            = std::string{dbName}.append(dlog::file_database_handle::extension);
    auto const sinkFilePattern = dbName + ".{ctr}_{iso8601}.blog";

    {
        auto createRx = dlog::file_database_handle::file_database(
                {}, dbFullName, sinkFilePattern);
        DPLX_REQUIRE_RESULT(createRx);
        auto &&db = std::move(createRx).assume_value();

        auto create2Rx = db.create_record_container();
        DPLX_REQUIRE_RESULT(create2Rx);
    }

    {
        auto createRx = dlog::file_database_handle::file_database(
                {}, dbFullName, sinkFilePattern);
        DPLX_REQUIRE_RESULT(createRx);
        auto &&db = std::move(createRx).assume_value();

        // cleanup
        auto cleanupRx = db.unlink_all();
        DPLX_TEST_RESULT(cleanupRx);
    }
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace dlog_tests
