
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

#include <dplx/dlog/bus/mpsc_bus.hpp>
#include <dplx/dlog/core/file_database.hpp>
#include <dplx/dlog/llfio.hpp>
#include <dplx/dlog/log_fabric.hpp>
#include <dplx/dlog/sink.hpp>

#include "test_dir.hpp"
#include "test_utils.hpp"

namespace dlog_tests
{

TEST_CASE("The library can create a new database, as new file_sink and write a "
          "record into it")
{
    using sink_type = dlog::basic_sink_frontend<dlog::file_sink_backend>;

    auto dbOpenRx = dlog::file_database_handle::file_database(
            test_dir, "log-test.drot", "log-test.{now:%FT%H-%M-%S}.dlog");
    REQUIRE(dbOpenRx);
    auto &&db = std::move(dbOpenRx).assume_value();

    constexpr auto bufferSize = 64 * 1024;
    auto sinkBackendOpenRx = dlog::file_sink_backend::file_sink(
            db.create_record_container(dlog::file_sink_id::default_,
                                       dlog::file_sink_backend::file_mode)
                    .value(),
            bufferSize, {});
    REQUIRE(sinkBackendOpenRx);

    constexpr auto regionSize = 1 << 14;
    dlog::log_fabric core{
            dlog::mpsc_bus(test_dir, "tmp", 4U, regionSize).value()};
    auto *sink = core.attach_sink(std::make_unique<sink_type>(
            dlog::severity::info, std::move(sinkBackendOpenRx).assume_value()));

    dlog::log_context ctx(core);
    DLOG_TO(ctx, dlog::severity::warn, "important msg with arg {}", 1);
    // DLOG_GENERIC(xlog, dlog::severity::warn,
    //              "important msg with arg {} and {}", 1, dlog::arg("argx",
    //              2));

    auto retireRx = core.retire_log_records();
    REQUIRE(retireRx);
    CHECK(retireRx.assume_value() == 0);

    core.release_sink(sink);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
    std::unique_ptr<sink_type> sinkOwner(static_cast<sink_type *>(sink));
    auto finRx = sinkOwner->backend().finalize();
    REQUIRE(finRx);
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
