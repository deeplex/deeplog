
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
#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/file_database.hpp>
#include <dplx/dlog/llfio.hpp>
#include <dplx/dlog/macros.hpp>
#include <dplx/dlog/sink.hpp>
#include <dplx/dlog/source.hpp>

namespace dlog_ex
{

using namespace dplx;

class jsf_engine
{
    std::uint32_t a;
    std::uint32_t b;
    std::uint32_t c;
    std::uint32_t d;

public:
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
    jsf_engine(std::uint32_t seed) noexcept
        : a(0xf1ea'5eedU)
        , b(seed)
        , c(seed)
        , d(seed)
    {
        for (int i = 0; i < 20; ++i)
        {
            (void)operator()();
        }
    }
    auto operator()() noexcept -> std::uint32_t
    {
        std::uint32_t e = a - std::rotl(b, 27);
        a = b ^ std::rotl(c, 17);
        b = c + d;
        c = d + e;
        d = e + a;
        return d;
    }
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
};

inline void do_output()
{
    DLOG_(warn, "important msg with arg {}", 1);
    DLOG_(info, "here happens something else");
    DLOG_(error, "oh no something bad happened");

    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
    jsf_engine gen(0xdead'beefU);
    constexpr int limit = 0x2a * 2;
    for (int i = 0; i < limit; ++i)
    {
        if (auto v = gen(); (v & 0x800'0000U) != 0)
        {
            DLOG_(warn, "{} is a pretty big number", v);
        }
        if (auto v = gen(); (v & 1U) != 0)
        {
            DLOG_(info, "{} is a real oddity", v);
        }
        DLOG_(debug, "I'm still alive");
        if (auto v = gen(); (v & 0x7) == 0x7)
        {
            DLOG_(error, "this is not good");
        }
    }
    DLOG_(fatal, "this is the end");
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
}

inline auto main() -> dlog::result<void>
{
    using sink_type = dlog::basic_sink_frontend<dlog::file_sink_backend>;
    dlog::llfio::path_handle baseDir = {};

    DPLX_TRY(auto &&db, dlog::file_database_handle::file_database(
                                baseDir, "log-test.drot",
                                "log-test.{now:%FT%H-%M-%S}.dlog"));
    DPLX_TRY(auto &&sinkFile,
             db.create_record_container(dlog::file_sink_id::default_,
                                        dlog::file_sink_backend::file_mode));

    constexpr auto bufferSize = 64 * 1024;
    DPLX_TRY(auto &&sinkBackend, dlog::file_sink_backend::file_sink(
                                         std::move(sinkFile), bufferSize, {}));

    constexpr auto regionSize = 1 << 14;
    DPLX_TRY(auto &&mpscBus,
             dlog::mpsc_bus(baseDir, "log-test.dmsb", 4U, regionSize));
    dlog::core core{std::move(mpscBus)};
    auto *sink = core.attach_sink(std::make_unique<sink_type>(
            dlog::severity::debug, std::move(sinkBackend)));
    core.connector().threshold = dlog::severity::debug;
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

    DPLX_TRY(core.connector().unlink());
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
