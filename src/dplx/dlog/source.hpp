
// Copyright Henrik Steffen Gaßmann 2021, 2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <concepts>
#include <cstddef>
#include <string>
#include <type_traits>

#include <fmt/core.h>

#include <dplx/cncr/math_supplement.hpp>
#include <dplx/cncr/utils.hpp>
#include <dplx/dp/fwd.hpp>

#include <dplx/dlog/arguments.hpp>
#include <dplx/dlog/attributes.hpp>
#include <dplx/dlog/concepts.hpp>
#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/disappointment.hpp>
#include <dplx/dlog/log_bus.hpp>

namespace dplx::dlog
{

namespace detail
{

template <typename T>
struct is_attribute
{
    using type = std::false_type;
};
template <resource_id Id, typename T, typename ReType>
struct is_attribute<basic_attribute<Id, T, ReType>>
{
    using type = std::true_type;
};

////////////////////////////////////////////////////////////////////////////////

struct poly_string_value
{
    char const *data;
    std::size_t size;

    constexpr poly_string_value() noexcept = default;

    DPLX_ATTR_FORCE_INLINE constexpr poly_string_value(char const *str) noexcept
        : data(str)
        , size(std::char_traits<char>::length(str))
    {
    }
    template <typename Traits, typename Alloc>
    DPLX_ATTR_FORCE_INLINE constexpr poly_string_value(
            std::basic_string<char, Traits, Alloc> const &str) noexcept
        : data(str.data())
        , size(str.size())
    {
    }
    template <typename Traits>
    DPLX_ATTR_FORCE_INLINE constexpr poly_string_value(
            std::basic_string_view<char, Traits> const &strView) noexcept
        : data(strView.data())
        , size(strView.size())
    {
    }
    DPLX_ATTR_FORCE_INLINE constexpr poly_string_value(
            fmt::basic_string_view<char> const &strView) noexcept
        : data(strView.data())
        , size(strView.size())
    {
    }
};

enum class poly_log_thunk_mode
{
    size_of,
    encode,
};
using poly_log_thunk_ptr
        = auto(*)(void const *self,
                  poly_log_thunk_mode mode,
                  dp::emit_context &ctx) noexcept -> result<std::uint64_t>;

struct poly_thunk_value
{
    void const *self;
    poly_log_thunk_ptr func;

    constexpr poly_thunk_value() noexcept = default;

    template <typename T>
    DPLX_ATTR_FORCE_INLINE constexpr poly_thunk_value(T const &ref)
        : self(static_cast<void const *>(&ref))
        , func(&thunk_impl<T>)
    {
    }

private:
    template <typename T>
    static auto thunk_impl(void const *self,
                           poly_log_thunk_mode mode,
                           dp::emit_context &ctx) noexcept
            -> result<std::uint64_t>
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto const &typedSelf = *reinterpret_cast<T const *>(self);
        switch (mode)
        {
        case poly_log_thunk_mode::size_of:
            return dp::encoded_size_of(ctx, typedSelf);

        case poly_log_thunk_mode::encode:
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

////////////////////////////////////////////////////////////////////////////////

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define DPLX_X_WITH_THUNK 1

enum class poly_type_id : unsigned char
{
    null,
#define DPLX_X(name, type, var) name,
#include <dplx/dlog/detail/x_poly_types.inl>
#undef DPLX_X
    LIMIT,
};

template <typename T>
inline constexpr poly_type_id poly_type_id_of = poly_type_id::thunk;

#define DPLX_X(name, type, var)                                                \
    template <>                                                                \
    inline constexpr poly_type_id poly_type_id_of<type> = poly_type_id::name;
#include <dplx/dlog/detail/x_poly_types.inl>
#undef DPLX_X

template <poly_type_id Id>
struct poly_type_map;

#define DPLX_X(name, _type, var)                                               \
    template <>                                                                \
    struct poly_type_map<poly_type_id::name>                                   \
    {                                                                          \
        using type = _type;                                                    \
    };
#include <dplx/dlog/detail/x_poly_types.inl>
#undef DPLX_X

template <poly_type_id Id>
using poly_type_for = typename poly_type_map<Id>::type;

struct poly_value
{
    union
    {
        dp::null_type mNullValue;
#define DPLX_X(name, type, var) type var;
#include <dplx/dlog/detail/x_poly_types.inl>
#undef DPLX_X
    };

public:
    DPLX_ATTR_FORCE_INLINE constexpr poly_value() noexcept = default;
#define DPLX_X(name, type, var)                                                \
    DPLX_ATTR_FORCE_INLINE constexpr poly_value(type value)                    \
        : var(value)                                                           \
    {                                                                          \
    }
#include <dplx/dlog/detail/x_poly_types.inl>
#undef DPLX_X
};

#undef DPLX_X_WITH_THUNK
// NOLINTEND(cppcoreguidelines-macro-usage)

////////////////////////////////////////////////////////////////////////////////

template <cncr::integer T>
    requires(sizeof(std::uint64_t) >= sizeof(T))
inline constexpr poly_type_id poly_type_id_of<T>
        = std::is_unsigned_v<T> ? poly_type_id::uint64 : poly_type_id::int64;

template <std::convertible_to<poly_string_value> T>
inline constexpr poly_type_id poly_type_id_of<T> = poly_type_id::string;

////////////////////////////////////////////////////////////////////////////////

struct log_location
{
    char const *filename;
    std::int_least32_t line;
    std::int_least16_t filenameSize;
};

#if 0 && DPLX_DLOG_USE_SOURCE_LOCATION
// TODO: implement `make_location()` with `std::source_location`
#else
consteval auto make_location(char const *filename,
                             std::uint_least32_t line) noexcept -> log_location
{
    auto const filenameSize = std::char_traits<char>::length(filename);
    assert(filenameSize <= INT_LEAST16_MAX);
    return {filename, static_cast<std::int_least32_t>(line),
            static_cast<std::int_least16_t>(filenameSize)};
}
#define DPLX_DLOG_LOCATION                                                     \
    ::dplx::dlog::detail::make_location(__FILE__, __LINE__)
#endif

////////////////////////////////////////////////////////////////////////////////

// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
class log_args
{
public:
    fmt::string_view message;
    poly_value const *message_parts;
    poly_type_id const *part_types;
    log_location location;
    std::uint_least16_t num_arguments;
    severity sev;
};

class vlogger
{
protected:
    constexpr ~vlogger() noexcept = default;
    constexpr vlogger() noexcept = default;

public:
    auto vlog(bus_output_buffer &out, log_args const &args) const noexcept
            -> result<void>;

private:
    virtual auto do_allocate(bus_output_buffer &out,
                             std::size_t messageSize) const noexcept
            -> cncr::data_defined_status_code<errc>
            = 0;
};

} // namespace detail

template <bus T>
class logger final : private detail::vlogger
{
    T *mBus{};
    DPLX_ATTR_NO_UNIQUE_ADDRESS typename T::logger_token mToken{};

public:
    // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes)
    severity threshold{};

    constexpr logger() noexcept = default;

    logger(T &targetBus, severity thresholdInit = default_threshold) noexcept
        : mBus(&targetBus)
        , mToken{}
        , threshold(thresholdInit)
    {
    }

private:
    using output_buffer = typename T::output_buffer;

public:
    template <typename... Args>
        requires(... && loggable_argument<Args>)
    [[nodiscard]] auto operator()(severity sev,
                                  fmt::format_string<Args...> message,
                                  detail::log_location location,
                                  Args const &...args) const noexcept
            -> result<void>
    {
        static_assert(sizeof...(Args) <= UINT_LEAST16_MAX);
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        detail::poly_value const
                values[sizeof...(Args) > 0U ? sizeof...(Args) : 1U]
                = {detail::poly_type_for<detail::poly_type_id_of<Args>>(
                        args)...};
        // NOLINTEND(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
        detail::poly_type_id const
                types[sizeof...(Args) > 0U ? sizeof...(Args) : 1U]
                = {detail::poly_type_id_of<Args>...};

        output_buffer buffer;
        return vlogger::vlog(
                buffer,
                detail::log_args{
                        message,
                        static_cast<detail::poly_value const *>(values),
                        static_cast<detail::poly_type_id const *>(types),
                        location,
                        static_cast<std::uint_least16_t>(sizeof...(Args)),
                        sev,
                });
    }

private:
    auto do_allocate(bus_output_buffer &out,
                     std::size_t messageSize) const noexcept
            -> cncr::data_defined_status_code<errc> final
    {
        return mBus->allocate(static_cast<output_buffer &>(out), messageSize,
                              mToken);
    }
};

} // namespace dplx::dlog

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

#if _MSVC_TRADITIONAL
#define DLOG_GENERIC(log, severity, message, ...)                              \
    do                                                                         \
    { /* due to shadowing this name isn't required to be unique */             \
        if (auto &&_dlog_materialized_temporary_ = (log);                      \
            _dlog_materialized_temporary_.threshold >= (severity))             \
            (void)_dlog_materialized_temporary_(                               \
                    (severity), (message), DPLX_DLOG_LOCATION, __VA_ARGS__);   \
    } while (0)
#else // _MSVC_TRADITIONAL
#define DLOG_GENERIC(log, severity, message, ...)                              \
    do                                                                         \
    { /* due to shadowing this name isn't required to be unique */             \
        if (auto &&_dlog_materialized_temporary_ = (log);                      \
            _dlog_materialized_temporary_.threshold >= (severity))             \
            (void)_dlog_materialized_temporary_(                               \
                    (severity), (message),                                     \
                    DPLX_DLOG_LOCATION __VA_OPT__(, __VA_ARGS__));             \
    } while (0)
#endif // _MSVC_TRADITIONAL

#define DLOG_CRITICAL(log, message, ...)                                       \
    DLOG_GENERIC(log, ::dplx::dlog::severity::critical, message, __VA_ARGS__)
#define DLOG_ERROR(log, message, ...)                                          \
    DLOG_GENERIC(log, ::dplx::dlog::severity::error, message, __VA_ARGS__)
#define DLOG_WARN(log, message, ...)                                           \
    DLOG_GENERIC(log, ::dplx::dlog::severity::warn, message, __VA_ARGS__)
#define DLOG_INFO(log, message, ...)                                           \
    DLOG_GENERIC(log, ::dplx::dlog::severity::info, message, __VA_ARGS__)
#define DLOG_DEBUG(log, message, ...)                                          \
    DLOG_GENERIC(log, ::dplx::dlog::severity::debug, message, __VA_ARGS__)
#define DLOG_TRACE(log, message, ...)                                          \
    DLOG_GENERIC(log, ::dplx::dlog::severity::trace, message, __VA_ARGS__)

// NOLINTEND(cppcoreguidelines-macro-usage)
