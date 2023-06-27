
// Copyright Henrik S. Gaßmann 2021-2022.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/core/file_database.hpp"

#include <catch2/catch_test_macros.hpp>

#include "test_dir.hpp"
#include "test_utils.hpp"

namespace dlog_tests
{

TEST_CASE("file database API integration test")
{
    auto const dbName = llfio::utils::random_string(4U);
    auto const dbFullName
            = std::string{dbName}.append(dlog::file_database_handle::extension);
    auto const sinkFilePattern = dbName + ".{ctr}_{now:%FT%H-%M-%S}.blog";

    auto createRx = dlog::file_database_handle::file_database(
            test_dir, dbFullName, sinkFilePattern);
    REQUIRE(createRx);
    auto &&db = std::move(createRx).assume_value();

    auto create2Rx = db.create_record_container();
    REQUIRE(create2Rx);

    // cleanup
    (void)create2Rx.assume_value().close();
    auto cleanupRx = db.unlink_all();
    CHECK(cleanupRx);
}

TEST_CASE("file database reopen round trip")
{
    auto const dbName = llfio::utils::random_string(4U);
    auto const dbFullName
            = std::string{dbName}.append(dlog::file_database_handle::extension);
    auto const sinkFilePattern = dbName + ".{ctr}_{now:%FT%H-%M-%S}.blog";

    {
        auto createRx = dlog::file_database_handle::file_database(
                {}, dbFullName, sinkFilePattern);
        REQUIRE(createRx);
        auto &&db = std::move(createRx).assume_value();

        auto create2Rx = db.create_record_container();
        REQUIRE(create2Rx);
    }

    {
        auto createRx = dlog::file_database_handle::file_database(
                {}, dbFullName, sinkFilePattern);
        REQUIRE(createRx);
        auto &&db = std::move(createRx).assume_value();

        // cleanup
        auto cleanupRx = db.unlink_all();
        CHECK(cleanupRx);
    }
}

} // namespace dlog_tests
