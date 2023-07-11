
// Copyright Henrik Steffen Gaßmann 2021-2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

#include <dplx/predef/compiler.h>

#if defined(DPLX_COMP_GNUC_AVAILABLE)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

#include <outcome/experimental/status_result.hpp>
#include <outcome/try.hpp>
#include <status-code/system_code.hpp>

#if defined(DPLX_COMP_GNUC_AVAILABLE)
#pragma GCC diagnostic pop
#endif

#include <dplx/cncr/data_defined_status_domain.hpp>

namespace dplx::dlog
{

namespace system_error = SYSTEM_ERROR2_NAMESPACE;
namespace oc = OUTCOME_V2_NAMESPACE;

enum class [[nodiscard]] errc
{
    success = 0,
    bad = 1,
    invalid_argument,
    not_enough_memory,
    not_enough_space,
    missing_data,
    invalid_file_database_header,
    invalid_record_container_header,
    container_unlink_failed,
    container_could_not_be_locked,
    message_bus_unlink_failed,
    unknown_argument_type_id,
    unknown_attribute_type_id,
    unknown_sink,
    sink_finalization_failed,

    LIMIT,
};

// we use a distinct policy type in order to shorten the symbol length
template <typename T>
struct status_throw_policy
    : oc::experimental::policy::status_code_throw<
              T,
              system_error::errored_status_code<system_error::erased<
                      typename system_error::system_code::value_type>>,
              void>
{
};

template <typename R>
using result = oc::experimental::status_result<
        R,
        system_error::errored_status_code<system_error::erased<
                typename system_error::system_code::value_type>>,
        status_throw_policy<R>>;

template <typename R>
using pure_result = oc::experimental::status_result<R, errc>;

} // namespace dplx::dlog

template <>
struct dplx::cncr::status_enum_definition<::dplx::dlog::errc>
    : status_enum_definition_defaults<::dplx::dlog::errc>
{
    static constexpr char domain_id[] = "BED2C33E-C001-4CDB-9BB3-2A2638D85E08";
    static constexpr char domain_name[] = "dplx::dlog error domain";

    static constexpr value_descriptor values[] = {
  // clang-format off
        { code::success, generic_errc::success,
            "No Error/Success" },
        { code::bad, generic_errc::unknown,
            "an external API did not meet its operation contract"},
        { code::invalid_argument, generic_errc::invalid_argument,
            "Invalid Argument" },
        { code::not_enough_memory, generic_errc::not_enough_memory,
            "The operation did not succeed due to a memory allocation failure" },
        { code::not_enough_space, generic_errc::no_buffer_space,
            "The operation failed to allocate a write buffer of sufficient size" },
        { code::missing_data, generic_errc::unknown,
            "The file/message is missing data at its end" },
        { code::invalid_file_database_header, generic_errc::unknown,
            "The .drot file doesn't start with a valid header" },
        { code::invalid_record_container_header, generic_errc::unknown,
            "The .dlog file doesn't start with a valid header" },
        { code::container_unlink_failed, generic_errc::unknown,
            "Failed to unlink one or more of the referenced record container(s)" },
        { code::container_could_not_be_locked, generic_errc::timed_out,
            "Failed to obtain an exclusive lock for the record container file" },    
        { code::message_bus_unlink_failed, generic_errc::unknown,
            "Failed to unlink one or more of the referenced message bus(es)" },
        { code::unknown_argument_type_id, generic_errc::unknown,
            "Could not decode the serialized argument due to an unknown type_id" },
        { code::unknown_attribute_type_id, generic_errc::unknown,
            "Could not decode the serialized attribute due to an unknown type_id" },
        { code::unknown_sink, generic_errc::unknown,
            "The given sink isn't attached to this log fabric." },
        { code::sink_finalization_failed, generic_errc::unknown,
            "Failed to finalize the sink, the failure code is attached to the sink." },
  // clang-format on
    };
};

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

#ifndef DPLX_TRY
#define DPLX_TRY(...) OUTCOME_TRY(__VA_ARGS__)
#endif

// NOLINTEND(cppcoreguidelines-macro-usage)

namespace dplx::dlog::detail
{

template <typename B>
concept boolean_testable_impl = std::convertible_to<B, bool>;
// clang-format off
template <typename B>
concept boolean_testable
        = boolean_testable_impl<B>
        && requires(B &&b)
        {
            { !static_cast<B &&>(b) }
                -> boolean_testable_impl;
        };
// clang-format on

// clang-format off
template <typename T>
concept tryable
    = requires(T t)
{
    { oc::try_operation_has_value(t) }
        -> boolean_testable;
    { oc::try_operation_return_as(static_cast<T &&>(t)) }
        -> std::convertible_to<result<void>>;
    oc::try_operation_extract_value(static_cast<T &&>(t));
};
// clang-format on

template <tryable T>
using result_value_t
        = std::remove_cvref_t<decltype(oc::try_operation_extract_value(
                std::declval<T &&>()))>;

// clang-format off
template <typename T, typename R>
concept tryable_result
    = tryable<T>
    && requires(T rx)
    {
        { oc::try_operation_extract_value(static_cast<T &&>(rx)) }
            -> std::convertible_to<R>;
    };
// clang-format on

} // namespace dplx::dlog::detail
