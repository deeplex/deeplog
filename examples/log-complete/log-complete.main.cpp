
// Copyright Henrik S. Gaßmann 2023.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include <cstdlib>
#include <utility>

#include <fmt/format.h>

#include <dplx/dlog.hpp>
#include <dplx/dlog/bus/mpsc_bus.hpp>
#include <dplx/dlog/core/file_database.hpp>
#include <dplx/dlog/llfio.hpp>
#include <dplx/dlog/log_fabric.hpp>
#include <dplx/dlog/sinks/file_sink.hpp>

#include "ecomponent.hpp"

namespace dlog_ex
{

using namespace dplx;

inline auto main() -> dlog::result<void>
{
    using sink_type = dlog::basic_sink_frontend<dlog::file_sink_backend>;
    dlog::llfio::path_handle baseDir = {};

    DPLX_TRY(auto &&db, dlog::file_database_handle::file_database(
                                baseDir, "log-test.drot"));
    DPLX_TRY(auto &&sinkFile,
             db.create_record_container("log-test.{now:%FT%H-%M-%S}.dlog",
                                        dlog::file_sink_id::default_,
                                        dlog::file_sink_backend::file_mode));

    constexpr auto bufferSize = 64 * 1024;
    DPLX_TRY(auto &&sinkBackend, dlog::file_sink_backend::file_sink(
                                         std::move(sinkFile), bufferSize, {}));

    constexpr auto regionSize = 1 << 14;
    DPLX_TRY(auto &&mpscBus,
             dlog::mpsc_bus(baseDir, "log-test.dmsb", 4U, regionSize));
    dlog::log_fabric core{std::move(mpscBus)};
    auto *sink = core.attach_sink(std::make_unique<sink_type>(
            dlog::severity::debug, std::move(sinkBackend)));
    dlog::set_thread_context(dlog::log_context{core});

    {
        auto mainScope = dlog::span_scope::open("main/exec");
        do_output();
    }

    DPLX_TRY(core.retire_log_records());

    core.release_sink(sink);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
    std::unique_ptr<sink_type> sinkOwner(static_cast<sink_type *>(sink));
    DPLX_TRY(sinkOwner->backend().finalize());

    DPLX_TRY(core.message_bus().unlink());
    return dlog::oc::success();
}

} // namespace dlog_ex

auto main() -> int
{
    auto execRx = dlog_ex::main();
    if (execRx.has_value())
    {
        return EXIT_SUCCESS;
    }

    fmt::print(stdout, "example 'log-complete' failed:\n{}\n",
               execRx.assume_error().message().c_str());
    return EXIT_FAILURE;
}
