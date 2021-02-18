
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <system_error>
#include <type_traits>

#include <outcome/std_result.hpp>
#include <outcome/try.hpp>

namespace dplx::dlog
{

namespace oc = OUTCOME_V2_NAMESPACE;

template <typename R, typename EC = std::error_code>
using result = oc::basic_result<R, EC, oc::policy::default_policy<R, EC, void>>;

enum class errc
{
    nothing = 0, // to be removed
    bad = 1,
};
auto error_category() noexcept -> std::error_category const &;

inline auto make_error_code(errc value) -> std::error_code
{
    return std::error_code(static_cast<int>(value), error_category());
}

} // namespace dplx::dlog


template <>
struct std::is_error_code_enum<dplx::dlog::errc> : std::true_type
{
};

#ifndef DPLX_TRY
#define DPLX_TRY(...) OUTCOME_TRY(__VA_ARGS__)
#endif
