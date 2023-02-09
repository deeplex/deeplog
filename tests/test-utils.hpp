
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <fmt/core.h>
#include <fmt/ostream.h>

#include <dplx/dlog/disappointment.hpp>
#include <dplx/dlog/llfio.hpp>

#include "boost-test.hpp"

namespace dlog_tests
{

using namespace dplx;
namespace llfio = dlog::llfio;

// we don't want to throw from within an initializer
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern llfio::directory_handle test_dir;

template <typename R>
inline auto check_result(dlog::result<R> const &rx)
        -> boost::test_tools::predicate_result
{
    bool const succeeded = !rx.has_failure();
    boost::test_tools::predicate_result prx{succeeded};
    if (!succeeded)
    {
        auto &&code = rx.assume_error();
        auto const &cat = code.domain();

        fmt::print(prx.message().stream(),
                   "[domain: {}; value: {}; message: {}]", cat.name().c_str(),
                   static_cast<std::intptr_t>(code.value()),
                   code.message().c_str());
    }
    return prx;
}

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

#define DPLX_TEST_RESULT(...)                                                  \
    BOOST_TEST((::dlog_tests::check_result((__VA_ARGS__))))
#define DPLX_REQUIRE_RESULT(...)                                               \
    BOOST_TEST_REQUIRE((::dlog_tests::check_result((__VA_ARGS__))))

// NOLINTEND(cppcoreguidelines-macro-usage)

} // namespace dlog_tests
