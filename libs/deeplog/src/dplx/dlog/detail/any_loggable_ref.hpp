
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <string_view>

#include <fmt/core.h>

#include <dplx/cncr/utils.hpp>
#include <dplx/dp/api.hpp>
#include <dplx/dp/fwd.hpp>

#include <dplx/dlog/disappointment.hpp>
#include <dplx/dlog/loggable.hpp>

namespace dplx::dlog::detail
{

template <typename Char>
struct basic_trivial_string_view // NOLINT(cppcoreguidelines-pro-type-member-init)
{
    Char const *data;
    std::size_t size;

    constexpr basic_trivial_string_view() noexcept = default;
    DPLX_ATTR_FORCE_INLINE explicit constexpr basic_trivial_string_view(
            Char const *str, std::size_t strSize) noexcept
        : data(str)
        , size(strSize)
    {
    }

    DPLX_ATTR_FORCE_INLINE constexpr basic_trivial_string_view(
            Char const *str) noexcept
        : data(str)
        , size(std::char_traits<Char>::length(str))
    {
    }
    template <typename Traits, typename Alloc>
    DPLX_ATTR_FORCE_INLINE constexpr basic_trivial_string_view(
            std::basic_string<Char, Traits, Alloc> const &str) noexcept
        : data(str.data())
        , size(str.size())
    {
    }
    template <typename Traits>
    DPLX_ATTR_FORCE_INLINE constexpr basic_trivial_string_view(
            std::basic_string_view<Char, Traits> const &strView) noexcept
        : data(strView.data())
        , size(strView.size())
    {
    }
    DPLX_ATTR_FORCE_INLINE constexpr basic_trivial_string_view(
            fmt::basic_string_view<Char> const &strView) noexcept
        : data(strView.data())
        , size(strView.size())
    {
    }
    DPLX_ATTR_FORCE_INLINE constexpr basic_trivial_string_view(
            system_error::status_code_domain::string_ref const &strRef) noexcept
        requires std::is_same_v<Char, char>
        : data(strRef.data())
        , size(strRef.size())
    {
    }
};
using trivial_string_view = basic_trivial_string_view<char>;

enum class erased_loggable_thunk_mode
{
    size_of,
    size_of_raw,
    encode,
    encode_raw,
};
using erased_loggable_thunk_ptr
        = auto(*)(void const *self,
                  erased_loggable_thunk_mode mode,
                  dp::emit_context &ctx) noexcept -> result<std::uint64_t>;

struct erased_loggable_ref
{
    void const *self;
    erased_loggable_thunk_ptr func;

    constexpr erased_loggable_ref() noexcept = default;

    template <typename T>
    DPLX_ATTR_FORCE_INLINE constexpr erased_loggable_ref(T const &ref)
        : self(static_cast<void const *>(&ref))
        , func(&thunk_impl<T>)
    {
    }

private:
    // this has been implemented in source.cpp in order to avoid including
    // dp/items/emit_core.hpp
    static auto emit_reification_prefix(dp::emit_context &ctx,
                                        reification_type_id id) noexcept
            -> result<std::uint64_t>;

    template <typename T>
    static auto thunk_impl(void const *self,
                           erased_loggable_thunk_mode mode,
                           dp::emit_context &ctx) noexcept
            -> result<std::uint64_t>
    {
        using enum erased_loggable_thunk_mode;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto const &typedSelf = *reinterpret_cast<T const *>(self);
        switch (mode)
        {
        case size_of:
            return 1U + dp::encoded_size_of(ctx, effective_reification_tag_v<T>)
                 + dp::encoded_size_of(ctx, typedSelf);

        case size_of_raw:
            return dp::encoded_size_of(ctx, typedSelf);

        case encode:
            if (result<std::uint64_t> emitRx
                = emit_reification_prefix(ctx, effective_reification_tag_v<T>);
                emitRx.has_error()) [[unlikely]]
            {
                return emitRx;
            }
            [[fallthrough]];
        case encode_raw:
            if (dp::result<void> encodeRx = dp::encode(ctx, typedSelf);
                encodeRx.has_error()) [[unlikely]]
            {
                return static_cast<decltype(encodeRx) &&>(encodeRx)
                        .assume_error();
            }
            return oc::success_type<std::uint64_t>(std::uint64_t{});

        default:
            cncr::unreachable();
        }
    }
};

} // namespace dplx::dlog::detail

template <>
class dplx::dp::codec<dplx::dlog::detail::trivial_string_view>
{
public:
    static auto size_of(dp::emit_context &ctx,
                        dlog::detail::trivial_string_view const &str) noexcept
            -> std::uint64_t;
    static auto encode(dp::emit_context &ctx,
                       dlog::detail::trivial_string_view const &str) noexcept
            -> dp::result<void>;
};

namespace dplx::dlog::detail
{

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define DPLX_X_STRING trivial_string_view

enum class any_loggable_ref_storage_id : unsigned char
{
    null,
#define DPLX_X(name, type, var) name,
#include <dplx/dlog/detail/x_poly_types.inl>
#undef DPLX_X
    thunk,
    LIMIT,
};

consteval auto as_any_loggable_storage_id(reification_type_id id) noexcept
        -> any_loggable_ref_storage_id
{
    switch (id)
    {
#define DPLX_X(name, type, var)                                                \
    case reification_type_id::name:                                            \
        return any_loggable_ref_storage_id::name;
#include <dplx/dlog/detail/x_poly_types.inl>
#undef DPLX_X
    default:
        return any_loggable_ref_storage_id::thunk;
    }
}

consteval auto as_reification_id(any_loggable_ref_storage_id id) noexcept
        -> reification_type_id
{
    switch (id)
    {
#define DPLX_X(name, type, var)                                                \
    case any_loggable_ref_storage_id::name:                                    \
        return reification_type_id::name;
#include <dplx/dlog/detail/x_poly_types.inl>
#undef DPLX_X
    default:
        cncr::unreachable();
    }
}

#define DPLX_X_WITH_THUNK 1

struct any_loggable_ref_storage
{
    union
    {
        dp::null_type null;
#define DPLX_X(name, type, var) type var;
#include <dplx/dlog/detail/x_poly_types.inl>
#undef DPLX_X
    };

public:
    DPLX_ATTR_FORCE_INLINE constexpr any_loggable_ref_storage() noexcept
            = default;
#define DPLX_X(name, type, var)                                                \
    DPLX_ATTR_FORCE_INLINE constexpr any_loggable_ref_storage(type value)      \
        : var(value)                                                           \
    {                                                                          \
    }
#include <dplx/dlog/detail/x_poly_types.inl>
#undef DPLX_X
};

template <any_loggable_ref_storage_id Id>
struct any_loggable_ref_storage_type_of
{
};

template <any_loggable_ref_storage_id Id>
using any_loggable_ref_storage_type_of_t =
        typename any_loggable_ref_storage_type_of<Id>::type;

#define DPLX_X(name, _type, var)                                               \
    template <>                                                                \
    struct any_loggable_ref_storage_type_of<any_loggable_ref_storage_id::name> \
    {                                                                          \
        using type = _type;                                                    \
    };
#include <dplx/dlog/detail/x_poly_types.inl>
#undef DPLX_X

#undef DPLX_X_WITH_THUNK

template <typename T>
inline constexpr any_loggable_ref_storage_id any_loggable_ref_storage_tag
        = any_loggable_ref_storage_id::thunk;

#define DPLX_X(name, type, var)                                                \
    template <>                                                                \
    inline constexpr any_loggable_ref_storage_id                               \
            any_loggable_ref_storage_tag<type>                                 \
            = any_loggable_ref_storage_id::name;
#include <dplx/dlog/detail/x_poly_types.inl>
#undef DPLX_X

template <cncr::integer T>
    requires(sizeof(std::uint64_t) >= sizeof(T))
inline constexpr any_loggable_ref_storage_id any_loggable_ref_storage_tag<T>
        = std::is_unsigned_v<T> ? any_loggable_ref_storage_id::uint64
                                : any_loggable_ref_storage_id::int64;

template <typename T>
    requires(effective_reification_tag_v<T> == reification_type_id::string
             && std::convertible_to<T const &, trivial_string_view>)
inline constexpr any_loggable_ref_storage_id any_loggable_ref_storage_tag<T>
        = any_loggable_ref_storage_id::string;

#define DPLX_X_WITH_THUNK 1

class any_loggable_ref
{
    any_loggable_ref_storage mRef;
    any_loggable_ref_storage_id mRefType;

public:
    constexpr any_loggable_ref() noexcept
        : mRef()
        , mRefType(any_loggable_ref_storage_id::null)
    {
    }

    template <loggable T>
    explicit constexpr any_loggable_ref(T const &v)
        : mRef(typename any_loggable_ref_storage_type_of<
                any_loggable_ref_storage_tag<T>>::type(v))
        , mRefType(any_loggable_ref_storage_tag<T>)
    {
    }

    [[nodiscard]] constexpr auto type() const noexcept
            -> any_loggable_ref_storage_id
    {
        return mRefType;
    }
    [[nodiscard]] constexpr auto storage() const noexcept
            -> any_loggable_ref_storage
    {
        return mRef;
    }

    template <typename Fn>
    constexpr auto visit(Fn &&visitor) const noexcept -> decltype(auto)
    {
        using enum any_loggable_ref_storage_id;
        switch (mRefType)
        {
#define DPLX_X(name, type, var)                                                \
    case name:                                                                 \
        return static_cast<Fn &&>(visitor)(mRef.var);
#include <dplx/dlog/detail/x_poly_types.inl>
#undef DPLX_X

        default:
            cncr::unreachable();
        }
    }
};

#undef DPLX_X_WITH_THUNK
#undef DPLX_X_STRING
// NOLINTEND(cppcoreguidelines-macro-usage)

} // namespace dplx::dlog::detail
