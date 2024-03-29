
// Copyright Henrik Steffen Gaßmann 2021, 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <dplx/dlog/disappointment.hpp>

namespace dplx::dlog
{

enum class severity : unsigned;
struct span_id;
struct trace_id;
struct span_context;

class span_scope;

class log_context;
class log_record_port;

namespace detail
{

class attribute_args;

}

class record_output_buffer;
struct record_output_buffer_storage;

class file_database_handle;

class sink_frontend_base;
class file_sink_backend;
class db_file_sink_backend;

class mpsc_bus_handle;
class db_mpsc_bus_handle;

namespace detail
{

[[noreturn]] void throw_fmt_format_error(char const *message);

}

} // namespace dplx::dlog
