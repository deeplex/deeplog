
// Copyright Henrik S. Gaßmann 2023.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/attributes.hpp"

#include <catch2/catch_test_macros.hpp>

#include "test_utils.hpp"

namespace dlog_tests
{

TEST_CASE("make_attributes() returns empty attributes")
{
    auto attrs = dlog::make_attributes();
    CHECK(attrs.num_attributes == 0);
    CHECK(attrs.attributes == nullptr);
    CHECK(attrs.attribute_types == nullptr);
    CHECK(attrs.ids == nullptr);
}

TEST_CASE("make_attributes(file, line) returns attribute references")
{
    constexpr std::uint_least16_t fakedLineNumber = 1337U;

    using namespace std::string_view_literals;
    auto attrs = dlog::make_attributes(dlog::attr::file{"serious-code.bf"sv},
                                       dlog::attr::line{fakedLineNumber});
    CHECK(attrs.num_attributes == 2);
    CHECK(attrs.attributes != nullptr);
    CHECK(attrs.attribute_types != nullptr);
    CHECK(attrs.ids != nullptr);
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    CHECK(attrs.attribute_types[0]
          == dlog::detail::any_loggable_ref_storage_id::string);
    CHECK(attrs.attribute_types[1]
          == dlog::detail::any_loggable_ref_storage_id::uint64);
    CHECK(attrs.ids[0] == dlog::attr::file::id);
    CHECK(attrs.ids[1] == dlog::attr::line::id);
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

} // namespace dlog_tests
