
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstdint>
#include <string_view>
#include <type_traits>

#include <status-code/status_code_domain.hpp>

#include <dplx/cncr/math_supplement.hpp>
#include <dplx/dp/codecs/core.hpp>
#include <dplx/dp/codecs/std-string.hpp>
#include <dplx/dp/concepts.hpp>
#include <dplx/dp/fwd.hpp>

#include <dplx/dlog/detail/system_error2_fmt.hpp>

namespace dplx::dlog
{

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DPLX_X_WITH_SYSTEM_ERROR2 1

enum class reification_type_id : std::uint64_t
{
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DPLX_X(name, type, var) name,
#include <dplx/dlog/detail/x_poly_types.inl>
#undef DPLX_X
};

#undef DPLX_X_WITH_SYSTEM_ERROR2

} // namespace dplx::dlog

template <>
class dplx::dp::codec<dplx::dlog::reification_type_id>
{
    using underlying_type = std::underlying_type_t<dlog::reification_type_id>;

public:
    static auto size_of(emit_context &,
                        dplx::dlog::reification_type_id value) noexcept
            -> std::uint64_t;
    static auto encode(emit_context &ctx,
                       dplx::dlog::reification_type_id value) noexcept
            -> result<void>;
    static auto decode(parse_context &ctx,
                       dplx::dlog::reification_type_id &outValue) noexcept
            -> result<void>;
};

namespace dplx::dlog
{

template <reification_type_id Id>
using reification_type_constant
        = std::integral_constant<reification_type_id, Id>;

inline constexpr reification_type_id user_defined_reification_flag{1U << 7};

consteval auto make_user_reification_type_id(std::uint64_t id) noexcept
        -> reification_type_id
{
    return static_cast<reification_type_id>(
            id | static_cast<std::uint64_t>(user_defined_reification_flag));
}

template <std::uint64_t Id>
using user_reification_type_constant
        = reification_type_constant<dlog::make_user_reification_type_id(Id)>;

template <typename T>
struct reification_tag
{
};

template <typename T>
inline constexpr reification_type_id reification_tag_v
        = reification_tag<T>::value;

template <typename T>
concept reifiable = requires {
    typename T;
    typename reification_tag<T>;
    reification_tag<T>::value;
    requires std::is_same_v<reification_type_id const,
                            decltype(reification_tag<T>::value)>;
    typename reification_type_constant<reification_tag<T>::value>;
} && dp::value_decodable<T>;

// core reification tag specializations

#define DPLX_X_STRING std::u8string
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define DPLX_X(name, type, var)                                                \
    template <>                                                                \
    struct reification_tag<type>                                               \
        : reification_type_constant<reification_type_id::name>                 \
    {                                                                          \
    };
#include <dplx/dlog/detail/x_poly_types.inl>
#undef DPLX_X
#undef DPLX_X_STRING

template <>
struct reification_tag<std::string>
    : reification_type_constant<reification_type_id::string>
{
};

template <>
struct reification_tag<detail::reified_status_code>
    : reification_type_constant<reification_type_id::status_code>
{
};
template <>
struct reification_tag<detail::reified_system_code>
    : reification_type_constant<reification_type_id::system_code>
{
};

} // namespace dplx::dlog

namespace dplx::dlog::detail
{

// C-strings get a bit of special treatment (i.e. remapping) as they
// (rightfully) aren't dp::codable, but we want make them loggable anyways. In
// the general case there shouldn't be any types which aren't encodable
// themselves, but can be converted to and encoded by any_loggable_ref.
template <typename T>
struct remap_c_string
{
    using type = T;
};
template <std::size_t N>
struct remap_c_string<char[N]>
{
    using type = std::string;
};
template <>
struct remap_c_string<char *>
{
    using type = std::string;
};
template <>
struct remap_c_string<char const *>
{
    using type = std::string;
};
template <std::size_t N>
struct remap_c_string<char8_t[N]>
{
    using type = std::u8string;
};
template <>
struct remap_c_string<char8_t *>
{
    using type = std::u8string;
};
template <>
struct remap_c_string<char8_t const *>
{
    using type = std::u8string;
};
template <>
struct remap_c_string<SYSTEM_ERROR2_NAMESPACE::status_code_domain::string_ref>
{
    using type = std::string;
};

template <std::derived_from<system_error::status_code<void>> T>
struct remap_c_string<T>
{
    using type = detail::trivial_status_code_view;
};

template <typename T>
using remap_c_string_t = typename remap_c_string<T>::type;

} // namespace dplx::dlog::detail

namespace dplx::dlog
{

template <typename T>
struct reification_type_of : detail::remap_c_string<T>
{
};

template <typename T>
using reification_type_of_t = typename reification_type_of<T>::type;

template <typename T>
concept loggable
        = requires { typename T; } && dp::encodable<detail::remap_c_string_t<T>>
          && requires { typename reification_type_of<T>::type; }
          && reifiable<typename reification_type_of<T>::type>;

// core reification type maps

template <cncr::integer T>
    requires(sizeof(std::uint64_t) >= sizeof(T))
struct reification_type_of<T>
    : std::conditional<std::is_unsigned_v<T>, std::uint64_t, std::int64_t>
{
};

template <>
struct reification_type_of<std::string_view>
{
    using type = std::string;
};
template <>
struct reification_type_of<std::u8string_view>
{
    using type = std::u8string;
};

template <std::derived_from<system_error::status_code<void>> T>
struct reification_type_of<T>
{
    using type = detail::reified_status_code;
};
template <>
struct reification_type_of<system_error::system_code>
{
    using type = detail::reified_system_code;
};
template <>
struct reification_type_of<system_error::erased_errored_status_code<
        typename system_error::system_code::value_type>>
{
    using type = detail::reified_system_code;
};

} // namespace dplx::dlog

namespace dplx::dlog::detail
{

template <typename T>
inline constexpr reification_type_id effective_reification_tag_v
        = reification_tag_v<reification_type_of_t<T>>;

} // namespace dplx::dlog::detail
