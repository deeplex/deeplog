
// Copyright Henrik Steffen Gaßmann 2021-2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

#include <dplx/cncr/data_defined_status_domain.hpp>
#include <dplx/cncr/disappointment.hpp>
#include <dplx/predef/compiler.h>

namespace dplx::dlog
{

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
    unknown_message_bus,
    sink_finalization_failed,

    LIMIT,
};

template <typename R>
using pure_result = outcome::experimental::status_result<R, errc>;

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
        { code::unknown_message_bus, generic_errc::unknown,
            "The given message bus (id, rotation) isn't registered with this database." },
        { code::sink_finalization_failed, generic_errc::unknown,
            "Failed to finalize the sink, the failure code is attached to the sink." },
  // clang-format on
    };
};
