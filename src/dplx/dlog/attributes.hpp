
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <string_view>
#include <type_traits>

#include <dplx/dp/concepts.hpp>

#include <dplx/dlog/arguments.hpp>
#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/detail/utils.hpp>

namespace dplx::dlog
{

template <resource_id Id, typename T = void, typename ReType = T>
struct basic_attribute
{
    static_assert(!std::is_void_v<T>,
                  "you cannot use the default attribute definition without "
                  "specifying the type arguments");

    static constexpr resource_id id{Id};

    using type = T;
    using retype = ReType;

    type const &value;
};

template <typename T>
concept loggable_attribute = dp::encodable<typename T::type>
                          && requires {
                                 typename T::type;
                                 typename T::retype;
                                 {
                                     T::id
                                     } -> std::convertible_to<resource_id>;
                             };

template <resource_id Id, detail::integer T, detail::integer ReType>
struct basic_attribute<Id, T, ReType>
{
    static constexpr resource_id id{Id};

    using type = T;
    using retype = ReType;

    type value;
};

namespace attr
{

using file = basic_attribute<resource_id{2}, std::string_view, std::string>;
using line = basic_attribute<resource_id{3}, unsigned>;

} // namespace attr

template <resource_id Id, typename T, typename ReType>
struct argument<basic_attribute<Id, T, ReType>>
{
};

} // namespace dplx::dlog
