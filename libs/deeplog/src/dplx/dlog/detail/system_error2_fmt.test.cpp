
// Copyright Henrik S. Gaßmann 2025.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/detail/system_error2_fmt.hpp"

#include <catch2/catch_test_macros.hpp>

#include "test_dir.hpp"
#include "test_utils.hpp"

namespace dlog_tests
{

TEST_CASE("formats a static_code_domain::string_ref using fmt")
{
    using namespace dplx::dlog::detail;
    constexpr char const helloWorld[] = "Hello, World!";

    SYSTEM_ERROR2_NAMESPACE::status_code_domain::string_ref strRef{
            static_cast<char const *>(helloWorld), sizeof(helloWorld) - 1};

    auto const formatted = fmt::format("{}", strRef);
    REQUIRE(formatted
            == std::string_view{static_cast<char const *>(helloWorld),
                                sizeof(helloWorld) - 1});
}

TEST_CASE("formats a reified_status_code using fmt")
{
    using namespace dplx::dlog::detail;

    reified_status_code code{
            .mDomainId = system_error2::generic_code::domain_type::get().id(),
            .mDomainName = "example_domain",
            .mMessage = "an example message",
    };

    auto const formatted = fmt::format("{}", code);
    REQUIRE(formatted == "{example_domain: an example message}");
}

TEST_CASE("formats a reified_system_code using fmt")
{
    using namespace dplx::dlog::detail;

    reified_system_code code{
            .mDomainId = system_error2::generic_code::domain_type::get().id(),
            .mRawValue = 0,
            .mDomainName = "example_system_domain",
            .mMessage = "a system error message",
    };

    auto const formatted = fmt::format("{}", code);
    REQUIRE(formatted == "{example_system_domain: a system error message}");
}

} // namespace dlog_tests
