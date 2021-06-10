
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <concepts>
#include <type_traits>

#include <dplx/dp/memory_buffer.hpp>

namespace dplx::dlog
{

// clang-format off
template <typename T>
concept sink_backend = requires(T &sinkBackend, unsigned int msgSize)
{
    { sinkBackend.write(msgSize) } -> std::same_as<dp::byte_buffer_view>;
};
// clang-format on

} // namespace dplx::dlog
