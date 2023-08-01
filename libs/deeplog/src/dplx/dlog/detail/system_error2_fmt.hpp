
// Copyright Henrik Steffen Gaßmann 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <status-code/status_code_domain.hpp>
#include <status-code/system_code.hpp>

SYSTEM_ERROR2_NAMESPACE_BEGIN

namespace detail
{
template <class StringView, template <class> class Formatter>
struct string_ref_formatter : private Formatter<StringView>
{
private:
    using base = Formatter<StringView>;

public:
    template <typename ParseContext>
    constexpr auto parse(ParseContext &ctx) -> typename ParseContext::iterator
    {
        return base::parse(ctx);
    }

    template <typename FormatContext>
    auto format(status_code_domain::string_ref const &str, FormatContext &ctx)
            -> typename FormatContext::iterator
    {
        return base::format(StringView(str.data(), str.size()), ctx);
    }
};
} // namespace detail

SYSTEM_ERROR2_NAMESPACE_END

#if __cpp_lib_format >= 202106L
#include <format>

SYSTEM_ERROR2_NAMESPACE_BEGIN
namespace detail
{
template <typename T>
using std_formatter = std::formatter<T, char>;
}
SYSTEM_ERROR2_NAMESPACE_END

template <>
struct std::formatter<SYSTEM_ERROR2_NAMESPACE::status_code_domain::string_ref,
                      char>
    : SYSTEM_ERROR2_NAMESPACE::detail::string_ref_formatter<
              std::string_view,
              SYSTEM_ERROR2_NAMESPACE::detail::std_formatter>
{
};
#endif

#include <fmt/core.h>

SYSTEM_ERROR2_NAMESPACE_BEGIN
namespace detail
{
template <typename T>
using fmt_formatter = fmt::formatter<T, char>;
}
SYSTEM_ERROR2_NAMESPACE_END

template <>
struct fmt::formatter<SYSTEM_ERROR2_NAMESPACE::status_code_domain::string_ref,
                      char>
    : SYSTEM_ERROR2_NAMESPACE::detail::string_ref_formatter<
              fmt::string_view,
              SYSTEM_ERROR2_NAMESPACE::detail::fmt_formatter>
{
};

#include <memory_resource>

#include <dplx/cncr/utils.hpp>
#include <dplx/dp/fwd.hpp>

#include <dplx/dlog/fwd.hpp>

namespace dplx::dlog::detail
{

struct trivial_status_code_view
{
    system_error2::status_code<void> const *code;

    trivial_status_code_view() noexcept = default;

    DPLX_ATTR_FORCE_INLINE
    trivial_status_code_view(system_error2::status_code<void> const &sc)
        : code(&sc)
    {
    }
};
struct reified_status_code
{
    std::uint64_t mDomainId{};
    std::pmr::string mDomainName;
    std::pmr::string mMessage;

    reified_status_code() noexcept = default;
};

struct trivial_system_code_view
{
    system_error2::system_code const *code;

    trivial_system_code_view() noexcept = default;

    DPLX_ATTR_FORCE_INLINE
    trivial_system_code_view(system_error2::system_code const &sc)
        : code(&sc)
    {
    }
};
struct reified_system_code
{
    std::uint64_t mDomainId{};
    std::uint64_t mRawValue{};
    std::pmr::string mDomainName;
    std::pmr::string mMessage;

    reified_system_code() noexcept = default;
};

} // namespace dplx::dlog::detail

template <>
struct fmt::formatter<dplx::dlog::detail::reified_status_code>
{
    static constexpr auto parse(format_parse_context &ctx)
            -> decltype(ctx.begin())
    {
        auto &&it = ctx.begin();
        if (it != ctx.end() && *it != '}')
        {
            dplx::dlog::detail::throw_fmt_format_error("invalid format");
        }
        return it;
    }

    template <typename FormatContext>
    static auto format(dplx::dlog::detail::reified_status_code code,
                       FormatContext &ctx) -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "{{{}: {}}}", code.mDomainName,
                              code.mMessage);
    }
};
template <>
struct fmt::formatter<dplx::dlog::detail::reified_system_code>
{
    static constexpr auto parse(format_parse_context &ctx)
            -> decltype(ctx.begin())
    {
        auto &&it = ctx.begin();
        if (it != ctx.end() && *it != '}')
        {
            dplx::dlog::detail::throw_fmt_format_error("invalid format");
        }
        return it;
    }

    template <typename FormatContext>
    static auto format(dplx::dlog::detail::reified_system_code code,
                       FormatContext &ctx) -> decltype(ctx.out())
    {
        return fmt::format_to(ctx.out(), "{{{}: {}}}", code.mDomainName,
                              code.mMessage);
    }
};

template <>
class dplx::dp::codec<dplx::dlog::detail::reified_status_code>
{
public:
    static auto decode(dp::parse_context &ctx,
                       dlog::detail::reified_status_code &code) noexcept
            -> result<void>;
};
template <>
class dplx::dp::codec<dplx::dlog::detail::trivial_status_code_view>
{
public:
    static auto size_of(dp::emit_context &ctx,
                        dlog::detail::trivial_status_code_view code) noexcept
            -> std::uint64_t;
    static auto encode(dp::emit_context &ctx,
                       dlog::detail::trivial_status_code_view code) noexcept
            -> result<void>;
};

template <>
class dplx::dp::codec<dplx::dlog::detail::reified_system_code>
{
public:
    static auto decode(dp::parse_context &ctx,
                       dlog::detail::reified_system_code &code) noexcept
            -> result<void>;
};
template <>
class dplx::dp::codec<dplx::dlog::detail::trivial_system_code_view>
{
public:
    static auto size_of(dp::emit_context &ctx,
                        dlog::detail::trivial_system_code_view code) noexcept
            -> std::uint64_t;
    static auto encode(dp::emit_context &ctx,
                       dlog::detail::trivial_system_code_view code) noexcept
            -> result<void>;
};
