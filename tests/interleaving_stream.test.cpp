
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include <dplx/dlog/detail/interleaving_stream.hpp>

#include "boost-test.hpp"
#include "test-utils.hpp"

namespace dlog_tests
{

BOOST_AUTO_TEST_SUITE(interleaving_stream)

static_assert(dp::input_stream<dlog::interleaving_input_stream_handle>);
static_assert(!dp::lazy_input_stream<dlog::interleaving_input_stream_handle>);

static_assert(dp::output_stream<dlog::interleaving_output_stream_handle>);
static_assert(dp::lazy_output_stream<dlog::interleaving_output_stream_handle>);

BOOST_AUTO_TEST_SUITE_END()

}
