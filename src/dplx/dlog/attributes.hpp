
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <string_view>
#include <type_traits>

#include <dplx/dp/codecs/core.hpp>
#include <dplx/dp/codecs/std-string.hpp>
#include <dplx/dp/concepts.hpp>

#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/loggable.hpp>

namespace dplx::dlog
{

template <resource_id Id, typename T = void>
    requires(loggable<T> || std::is_void_v<T>)
struct basic_attribute
{
    static_assert(!std::is_void_v<T>,
                  "you cannot use the default attribute definition without "
                  "specifying the type arguments");

    static constexpr resource_id id{Id};

    using type = T;

    type value;
};

template <typename T>
concept attribute = requires { typename T::type; };

namespace attr
{

using file = basic_attribute<resource_id{2}, std::string_view>;
using line = basic_attribute<resource_id{3}, unsigned>;

} // namespace attr

} // namespace dplx::dlog
