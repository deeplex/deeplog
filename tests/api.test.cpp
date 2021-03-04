
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "boost-test.hpp"

namespace dlog_tests
{

BOOST_AUTO_TEST_SUITE(api)

BOOST_AUTO_TEST_CASE(tmp)
{
    DLOG_GENERIC_LOG(logger, severity, "important msg with arg {} and {}", 1, 2, dlog::attrs());
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace dlog_tests
