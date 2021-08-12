
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "boost-test.hpp"
#include "test-utils.hpp"

#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/file_database.hpp>
#include <dplx/dlog/sink.hpp>
#include <dplx/dlog/source.hpp>

namespace dlog_tests
{

BOOST_AUTO_TEST_SUITE(api)

BOOST_AUTO_TEST_CASE(tmp)
{
    using sink_type = dlog::basic_sink_frontend<dlog::file_sink_backend>;

    auto dbOpenRx = dlog::file_database_handle::file_database(
            test_dir, "log-test.drot", "log-test.{iso8601}.dlog");
    DPLX_REQUIRE_RESULT(dbOpenRx);
    auto &&db = std::move(dbOpenRx).assume_value();

    auto sinkBackendOpenRx = dlog::file_sink_backend::file_sink(
            db.create_record_container(dlog::file_sink_id::default_,
                                       dlog::file_sink_backend::file_mode)
                    .value(),
            64 * 1024, {});
    DPLX_REQUIRE_RESULT(sinkBackendOpenRx);

    auto sinkOwner = std::make_unique<sink_type>(
            dlog::severity::info, std::move(sinkBackendOpenRx).assume_value());
    auto sink = sinkOwner.get();

    dlog::core core{dlog::ringbus(test_dir, "tmp", 1 << 10).value()};
    core.attach_sink(std::move(sinkOwner));

    dlog::logger xlog{core.connector()};
    DLOG_GENERIC(xlog, dlog::severity::warn,
                 u8"important msg with arg {} and {}", 1, dlog::arg("argx", 2));

    auto retireRx = core.retire_log_records();
    DPLX_REQUIRE_RESULT(retireRx);
    BOOST_TEST(retireRx.assume_value() == 0);

    core.release_sink(sink);
    sinkOwner.reset(sink);
    auto finRx = sinkOwner->backend().finalize();
    DPLX_REQUIRE_RESULT(finRx);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace dlog_tests
