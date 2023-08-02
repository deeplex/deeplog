
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/loggable.hpp"

#include <catch2/catch_test_macros.hpp>

#include <dplx/dp/api.hpp>
#include <dplx/dp/codecs/core.hpp>

#include "test_utils.hpp"

namespace dlog_tests
{

static_assert(dlog::loggable<int>);
static_assert(dlog::loggable<std::int64_t>);
static_assert(dlog::loggable<system_error::status_code_domain::string_ref>);
static_assert(dlog::loggable<system_error::system_code>);
static_assert(std::same_as<dlog::reification_type_of_t<int>, std::int64_t>);

static_assert(dlog::reification_tag_v<std::int64_t>
              == dlog::reification_type_id::int64);
static_assert(dlog::detail::effective_reification_tag_v<std::int64_t>
              == dlog::reification_type_id::int64);
static_assert(dlog::detail::effective_reification_tag_v<int>
              == dlog::reification_type_id::int64);

static_assert(dlog::reifiable<std::int64_t>);
static_assert(!dlog::reifiable<int>);
static_assert(!dlog::reifiable<system_error::status_code_domain::string_ref>);

} // namespace dlog_tests
