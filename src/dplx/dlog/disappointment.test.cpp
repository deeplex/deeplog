
// Copyright Henrik S. Gaßmann 2021-2022.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/disappointment.hpp"

#include "test_utils.hpp"

namespace dlog_tests
{

static_assert(std::size(dplx::cncr::status_enum_definition<dlog::errc>::values)
              == static_cast<std::size_t>(dlog::errc::LIMIT));
static_assert(dplx::cncr::validate_status_enum_definition_data<dlog::errc>());

} // namespace dlog_tests
