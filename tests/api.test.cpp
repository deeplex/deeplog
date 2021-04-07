
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "boost-test.hpp"
#include "test-utils.hpp"

#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/source.hpp>

namespace dlog_tests
{

BOOST_AUTO_TEST_SUITE(api)

BOOST_AUTO_TEST_CASE(tmp)
{
    auto bus = dlog::ringbus({}, "./tmp", 1 << 10).value();
    dlog::logger xlog{bus};
    DLOG_GENERIC(xlog, dlog::severity::warn,
                 u8"important msg with arg {} and {}", 1, 2);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace dlog_tests
