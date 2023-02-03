
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
    not_enough_space,
    missing_data,
    invalid_file_database_header,
    invalid_record_container_header,
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

namespace dplx::dlog::detail
{

template <typename T>
concept tryable = requires(T &&t) {
                      oc::try_operation_has_value(t);
                      {
                          oc::try_operation_return_as(static_cast<T &&>(t))
                          } -> std::convertible_to<result<void>>;
                      oc::try_operation_extract_value(static_cast<T &&>(t));
                  };

template <tryable T>
using result_value_t
        = std::remove_cvref_t<decltype(oc::try_operation_extract_value(
                std::declval<T &&>()))>;

template <typename T, typename R>
concept tryable_result
        = tryable<T> && std::convertible_to<result_value_t<T>, R>;

} // namespace dplx::dlog::detail
