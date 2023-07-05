
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
    dlog::llfio::path_handle baseDir = {};

    DPLX_TRY(auto &&db, dlog::file_database_handle::file_database(
                                baseDir, "log-test.drot"));

    constexpr auto regionSize = 1 << 14;
    DPLX_TRY(auto &&mpscBus,
             dlog::mpsc_bus(baseDir, "log-test.dmsb", 4U, regionSize));
    dlog::log_fabric core{std::move(mpscBus)};

    constexpr auto bufferSize = 64 * 1024;
    DPLX_TRY(auto *sink,
             core.create_sink<dlog::file_sink_db>({
                    .threshold = dlog::severity::debug,
                    .backend = {
                            .max_file_size = UINT64_MAX,
                            .database = db,
                            .file_name_pattern = "log-test.{now:%FT%H-%M-%S}.dlog",
                            .target_buffer_size = bufferSize,
                            .sink_id = dlog::file_sink_id::default_,
                    },
    }));
    dlog::set_thread_context(dlog::log_context{core});

    {
        auto mainScope = dlog::span_scope::open("main/exec");
        do_output();
    }

    DPLX_TRY(core.retire_log_records());

    DPLX_TRY(core.destroy_sink(sink));

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
