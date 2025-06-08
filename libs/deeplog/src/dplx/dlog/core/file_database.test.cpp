
// Copyright Henrik S. Gaßmann 2021-2022.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/core/file_database.hpp"

#include <catch2/catch_test_macros.hpp>

#include <dplx/dlog.hpp>
#include <dplx/dlog/bus/mpsc_bus.hpp>

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
    create2Rx.assume_value().handle.unlock_file();
    (void)create2Rx.assume_value().handle.close();

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
        create2Rx.assume_value().handle.unlock_file();

        auto create3Rx = db.create_message_bus(busNamePattern, "std", {});
        REQUIRE(create3Rx);
        create3Rx.assume_value().handle.unlock_file();
    }

    {
        auto createRx
                = dlog::file_database_handle::file_database({}, dbFullName);
        REQUIRE(createRx);
    }
}

namespace
{

void fill_mpsc_bus_orphan(
        dplx::make<dlog::db_mpsc_bus_handle> const &makeMpscBus)
{
    dlog::log_fabric<dlog::db_mpsc_bus_handle> fabric(makeMpscBus().value());
    dlog::set_thread_context(dlog::log_context(fabric));

    DLOG_(warn, "hello from no scope");
    {
        auto fillScope = dlog::span_scope::open("test/db/mpsc/fill");
        DLOG_(warn, "hello from outer scope");

        {
            auto innerScope = dlog::span_scope::open("test/db/mpsc/fill/inner");
            DLOG_(warn, "hello from inner scope");
        }
    }
}

} // namespace

TEST_CASE("file database message bus recovery test")
{
    constexpr auto region_size = 8 * 4096;

    auto const dbName
            = fmt::format("bus_recovery-{}", llfio::utils::random_string(4U));
    auto const dbFullName
            = std::string{dbName}.append(dlog::file_database_handle::extension);
    auto const busNamePattern = dbName + ".{id}.{ctr}_{pid}.dmpscb";

    auto createRx
            = dlog::file_database_handle::file_database(test_dir, dbFullName);
    REQUIRE(createRx);
    auto &&db = std::move(createRx).assume_value();

    fill_mpsc_bus_orphan({
            .database = db,
            .bus_id = "orphan",
            .file_name_pattern = busNamePattern,
            .num_regions = 3,
            .region_size = region_size,
    });

    auto recoverRx = db.prune_message_buses({});
    REQUIRE(recoverRx);
    CHECK(db.message_buses().empty());
    auto containers = db.record_containers();
    CHECK(!containers.empty());
    CHECK(std::ranges::count(containers, dlog::file_sink_id::recovered,
                             [](auto const &c) { return c.sink_id; }));
}

} // namespace dlog_tests
