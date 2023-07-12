
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
    auto const sinkFilePattern = dbName + ".{ctr}_{now:%FT%H-%M-%S}.dlog";
    auto const busNamePattern
            = dbName + ".{id}.{ctr}_{pid}_{now:%FT%H-%M-%S}.dmpscb";

    auto createRx
            = dlog::file_database_handle::file_database(test_dir, dbFullName);
    REQUIRE(createRx);
    auto &&db = std::move(createRx).assume_value();

    auto create2Rx = db.create_record_container(sinkFilePattern);
    REQUIRE(create2Rx);
    create2Rx.assume_value().unlock_file();
    (void)create2Rx.assume_value().close();

    auto create3Rx = db.create_message_bus(busNamePattern, "std", {});
    REQUIRE(create3Rx);
    create3Rx.assume_value().handle.unlock_file();
    (void)create3Rx.assume_value().handle.close();

    // cleanup
    auto cleanupRx = db.unlink_all();
    CHECK(cleanupRx);
}

TEST_CASE("file database reopen round trip")
{
    auto const dbName = llfio::utils::random_string(4U);
    auto const dbFullName
            = std::string{dbName}.append(dlog::file_database_handle::extension);
    auto const sinkFilePattern = dbName + ".{ctr}_{now:%FT%H-%M-%S}.dlog";
    auto const busNamePattern
            = dbName + ".{id}.{ctr}_{pid}_{now:%FT%H-%M-%S}.dmpscb";

    {
        auto createRx
                = dlog::file_database_handle::file_database({}, dbFullName);
        REQUIRE(createRx);
        auto &&db = std::move(createRx).assume_value();

        auto create2Rx = db.create_record_container(sinkFilePattern);
        REQUIRE(create2Rx);
        create2Rx.assume_value().unlock_file();

        auto create3Rx = db.create_message_bus(busNamePattern, "std", {});
        REQUIRE(create3Rx);
        create3Rx.assume_value().handle.unlock_file();
    }

    {
        auto createRx
                = dlog::file_database_handle::file_database({}, dbFullName);
        REQUIRE(createRx);
        auto &&db = std::move(createRx).assume_value();

        // cleanup
        auto cleanupRx = db.unlink_all();
        CHECK(cleanupRx);
    }
}

} // namespace dlog_tests
