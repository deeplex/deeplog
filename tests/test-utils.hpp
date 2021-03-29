
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "boost-test.hpp"

#include <fmt/core.h>

#include <dplx/dlog/llfio.hpp>
#include <dplx/dlog/disappointment.hpp>

namespace dlog_tests
{

using namespace dplx;
namespace llfio = dlog::llfio;

template <typename R>
inline auto check_result(dlog::result<R> const &rx)
        -> boost::test_tools::predicate_result
{
    bool const succeeded = !rx.has_failure();
    boost::test_tools::predicate_result prx{succeeded};
    if (!succeeded)
    {
        auto error = rx.assume_error();
        auto const &cat = error.category();

        fmt::print(prx.message().stream(),
                   "[category: {}; value: {}; message: {}]", cat.name(),
                   error.value(), error.message());
    }
    return prx;
}

#define DPLX_TEST_RESULT(...)                                                  \
    BOOST_TEST((::dlog_tests::check_result((__VA_ARGS__))))
#define DPLX_REQUIRE_RESULT(...)                                               \
    BOOST_TEST_REQUIRE((::dlog_tests::check_result((__VA_ARGS__))))

} // namespace dlog_tests
