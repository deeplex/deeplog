
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <string_view>
#include <type_traits>

#include <dplx/dp/codecs/core.hpp>
#include <dplx/dp/codecs/fixed_u8string.hpp>
#include <dplx/dp/codecs/std-string.hpp>
#include <dplx/dp/concepts.hpp>

#include <dplx/dlog/config.hpp>
#include <dplx/dlog/core/strong_types.hpp>
#include <dplx/dlog/detail/any_loggable_ref.hpp>
#include <dplx/dlog/loggable.hpp>

namespace dplx::dlog
{

template <resource_id Id, dp::fixed_u8string OtlpId, loggable T>
struct basic_attribute_ref
{
    static constexpr resource_id id{Id};
    static constexpr std::u8string_view otlp_id{OtlpId.data(), OtlpId.size()};

    using type = T;
    type const &value;
};
template <resource_id Id, dp::fixed_u8string OtlpId, typename T>
    requires(loggable<T> && (std::is_arithmetic_v<T> || std::is_enum_v<T>))
struct basic_attribute_ref<Id, OtlpId, T>
{
    static constexpr resource_id id{Id};
    static constexpr std::u8string_view otlp_id{OtlpId.data(), OtlpId.size()};

    using type = T;
    type value;
};
template <resource_id Id,
          dp::fixed_u8string OtlpId,
          typename CharT,
          typename CharTraits>
struct basic_attribute_ref<Id,
                           OtlpId,
                           std::basic_string_view<CharT, CharTraits>>
{
    static constexpr resource_id id{Id};
    static constexpr std::u8string_view otlp_id{OtlpId.data(), OtlpId.size()};

    using type = std::basic_string_view<CharT, CharTraits>;
    type value;
};
template <resource_id Id,
          dp::fixed_u8string OtlpId,
          typename CharT,
          typename CharTraits,
          typename Alloc>
struct basic_attribute_ref<Id,
                           OtlpId,
                           std::basic_string<CharT, CharTraits, Alloc>>
{
    static constexpr resource_id id{Id};
    static constexpr std::u8string_view otlp_id{OtlpId.data(), OtlpId.size()};

    using type = std::basic_string_view<CharT, CharTraits>;
    type value;
};

template <typename T>
concept attribute
        = requires {
              typename T;
              typename T::type;
              requires loggable<typename T::type>;
              T::id;
              requires std::is_same_v<resource_id const, decltype(T::id)>;
              typename std::integral_constant<resource_id, T::id>;
          };

namespace attr
{

using file = basic_attribute_ref<resource_id{2},
                                 u8"code.filepath",
                                 std::string_view>;
using line = basic_attribute_ref<resource_id{3}, u8"code.lineno", unsigned>;

using function = basic_attribute_ref<resource_id{4},
                                     u8"code.function",
                                     std::string_view>;

} // namespace attr

} // namespace dplx::dlog

namespace dplx::dlog::detail
{

#if DPLX_DLOG_USE_SOURCE_LOCATION
consteval auto make_attribute_function(std::source_location current
                                       = std::source_location::current())
        -> attr::function
{
    auto const function = current.function_name();
    auto const functionSize = std::char_traits<char>::length(function);
    return {
            {function, functionSize}
    };
}
#else
consteval auto make_attribute_function(char const *function) noexcept
        -> attr::function
{
    auto const functionSize = std::char_traits<char>::length(function);
    return {
            {function, functionSize}
    };
}
#endif

} // namespace dplx::dlog::detail

namespace dplx::dlog::detail
{

class attribute_args
{
public:
    detail::any_loggable_ref_storage const *attributes;
    detail::any_loggable_ref_storage_id const *attribute_types;
    resource_id const *ids;
    std::uint_least16_t num_attributes;
};

template <attribute... Args>
class stack_attribute_args : public attribute_args
{
    static_assert(sizeof...(Args) <= UINT_LEAST16_MAX);

public:
    detail::any_loggable_ref_storage const values[sizeof...(Args)];
    detail::any_loggable_ref_storage_id const types[sizeof...(Args)];
    resource_id const rids[sizeof...(Args)];

    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
    stack_attribute_args(Args... args)
        : attribute_args(attribute_args{
                values,
                types,
                rids,
                static_cast<std::uint_least16_t>(sizeof...(Args)),
        })
        , values{detail::any_loggable_ref_storage_type_of_t<
                  detail::any_loggable_ref_storage_tag<typename Args::type>>(
                  args.value)...}
        , types{detail::any_loggable_ref_storage_tag<typename Args::type>...}
        , rids{Args::id...}
    {
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)

    stack_attribute_args(stack_attribute_args const &) = delete;
    auto operator=(stack_attribute_args const &)
            -> stack_attribute_args & = delete;
};

template <>
class stack_attribute_args<> : public attribute_args
{
public:
    stack_attribute_args()
        : attribute_args(attribute_args{
                nullptr,
                nullptr,
                nullptr,
                0U,
        })
    {
    }
};

auto encoded_size_of_attributes(dp::emit_context &ctx,
                                attribute_args const &attrs) noexcept
        -> std::uint64_t;
auto encode_attributes(dp::emit_context &ctx,
                       attribute_args const &attrs) noexcept
        -> dp::result<void>;

} // namespace dplx::dlog::detail

template <>
class dplx::dp::codec<dplx::dlog::detail::attribute_args>
{
    using value_type = dplx::dlog::detail::attribute_args;

public:
    static auto size_of(emit_context &ctx, value_type const &attrs) noexcept
            -> std::uint64_t;
    static auto encode(emit_context &ctx, value_type const &attrs) noexcept
            -> result<void>;
};

namespace dplx::dlog
{

template <typename... Args>
    requires(... && attribute<std::remove_cvref_t<Args>>)
auto make_attributes(Args &&...args)
        -> detail::stack_attribute_args<std::remove_cvref_t<Args>...>
{
    return {args...};
}

} // namespace dplx::dlog
