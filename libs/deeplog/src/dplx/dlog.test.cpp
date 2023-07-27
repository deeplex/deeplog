
// Copyright 2021-2023 Henrik Steffen Gaßmann
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <fmt/format.h>

#include <dplx/dlog/bus/mpsc_bus.hpp>
#include <dplx/dlog/core/file_database.hpp>
#include <dplx/dlog/llfio.hpp>
#include <dplx/dlog/log_fabric.hpp>
#include <dplx/dlog/sinks/file_sink.hpp>

#include "test_dir.hpp"
#include "test_utils.hpp"

namespace dlog_tests
{

TEST_CASE("The library can create a new database, as new file_sink and write a "
          "record into it")
{
    auto dbOpenRx = dlog::file_database_handle::file_database(test_dir,
                                                              "log-test.drot");
    REQUIRE(dbOpenRx);
    auto &&db = std::move(dbOpenRx).assume_value();

    constexpr auto regionSize = 1 << 14;
    dlog::log_fabric core{
            dlog::mpsc_bus(test_dir, "tmp", 4U, regionSize).value()};

    constexpr auto bufferSize = 64 * 1024;
    auto createSinkRx = core.create_sink<dlog::file_sink_db>({
        .threshold = dlog::severity::info,
        .backend = {
            .max_file_size = UINT64_MAX,
            .database = db,
            .file_name_pattern = "log-test.{now:%FT%H-%M-%S}.dlog",
            .target_buffer_size = bufferSize,
            .sink_id = dlog::file_sink_id::default_,
        },
    });
    REQUIRE(createSinkRx);

    dlog::log_context ctx(core);
    DLOG_TO(ctx, dlog::severity::warn, "important msg with arg {}", 1);
    DLOG_TO(ctx, dlog::severity::error, "oh no {}",
            dlog::system_error::system_code(
                    dlog::system_error::errc::not_enough_memory));
    // DLOG_GENERIC(xlog, dlog::severity::warn,
    //              "important msg with arg {} and {}", 1, dlog::arg("argx",
    //              2));

    auto retireRx = core.retire_log_records();
    REQUIRE(retireRx);
    CHECK(retireRx.assume_value() == 0);

    REQUIRE(core.destroy_sink(createSinkRx.assume_value()));
}

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
