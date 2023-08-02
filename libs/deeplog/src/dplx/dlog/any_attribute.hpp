
// Copyright Henrik S. Gaßmann 2023.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <memory>
#include <memory_resource>
#include <string>
#include <utility>

#include <dplx/dlog/attributes.hpp>
#include <dplx/dlog/core/strong_types.hpp>
#include <dplx/dlog/detail/any_reified.hpp>
#include <dplx/dlog/detail/workaround.hpp>

namespace dplx::dlog
{

class any_attribute
{
    friend class dp::codec<any_attribute>;

public:
    using allocator_type = std::pmr::polymorphic_allocator<>;
    using allocator_traits = std::allocator_traits<allocator_type>;

private:
    using any_reified = detail::any_reified<allocator_type>;
    using string_type = std::pmr::string;

    resource_id mId;
    any_reified mValue;
    string_type mOtlpId;
    std::string_view mFormatSpec;
    mutable string_type mStringified;

public:
    ~any_attribute() noexcept = default;
    any_attribute() noexcept
        : mId()
        , mValue()
        , mOtlpId()
        , mFormatSpec()
        , mStringified()
    {
    }

    any_attribute(any_attribute &&other) noexcept
        : mId(std::exchange(other.mId, resource_id{}))
        , mValue(std::move(other.mValue))
        , mOtlpId(std::move(other.mOtlpId))
        , mFormatSpec(other.mFormatSpec)
        , mStringified(std::move(other.mStringified))
    {
    }
    auto operator=(any_attribute &&other) noexcept(
            // NOLINTNEXTLINE(performance-noexcept-move-constructor)
            allocator_traits::is_always_equal::value) -> any_attribute &
    {
        if (&other == this)
        {
            return *this;
        }

        if (allocator_traits::is_always_equal::value
            || mOtlpId.get_allocator() == other.mOtlpId.get_allocator())
        {
            mValue = std::move(other.mValue);
            mOtlpId = std::move(other.mOtlpId);
            mStringified = std::move(other.mStringified);
        }
        else
        {
            // make allocator compatible string copies first which then can be
            // nothrow moved into this. This way we maintain a strong exception
            // guarantee while side-stepping the necessity of an allocator
            // converting any_reified move constructor.
            string_type otlpId(other.mOtlpId, mOtlpId.get_allocator());
            string_type stringified(other.mStringified,
                                    other.mStringified.get_allocator());
            mValue = std::move(other.mValue);
            mOtlpId = std::move(otlpId);
            mStringified = std::move(stringified);
            other.mOtlpId.clear();
            other.mStringified.clear();
        }
        mId = std::exchange(other.mId, resource_id{});
        mFormatSpec = std::exchange(other.mFormatSpec, std::string_view{});

        return *this;
    }

    any_attribute(allocator_type const &allocator)
        : mId{}
        , mValue(allocator)
        , mOtlpId(allocator)
        , mFormatSpec{}
        , mStringified(allocator)
    {
    }
    any_attribute(any_attribute &&other, allocator_type const &allocator)
        : any_attribute(other.mOtlpId.get_allocator() == allocator
                                ? static_cast<any_attribute &&>(other)
                                : any_attribute{allocator})
    {
        if (other.mOtlpId.get_allocator() != allocator)
        {
            operator=(static_cast<any_attribute &&>(other));
        }
    }
#if DPLX_DLOG_WORKAROUND_ISSUE_LIBSTDCPP_108952
    any_attribute(any_attribute &other, allocator_type const &allocator)
        : any_attribute(other.mOtlpId.get_allocator() == allocator
                                ? static_cast<any_attribute &&>(other)
                                : any_attribute{allocator})
    {
        if (other.mOtlpId.get_allocator() != allocator)
        {
            operator=(static_cast<any_attribute &&>(other));
        }
    }
#endif

private:
    any_attribute(resource_id id,
                  any_reified &&value,
                  string_type &&otlpId,
                  std::string_view formatSpec,
                  allocator_type const &allocator) noexcept
        : mId(id)
        , mValue(static_cast<any_reified &&>(value))
        , mOtlpId(static_cast<string_type &&>(otlpId))
        , mFormatSpec(formatSpec)
        , mStringified(allocator)
    {
    }

public:
    template <attribute T>
    static auto reify(dp::parse_context &ctx) noexcept -> result<any_attribute>
    {
        using namespace std::string_view_literals;
        allocator_type allocator = ctx.get_allocator();
        string_type otlpId(allocator);
        try
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            otlpId.assign(reinterpret_cast<char const *>(T::otlp_id.data()),
                          T::otlp_id.size());
        }
        catch (std::bad_alloc const &)
        {
            return dp::errc::not_enough_memory;
        }
        DPLX_TRY(auto &&value,
                 any_reified::reify<reification_type_of_t<typename T::type>>(
                         ctx, allocator));
        return any_attribute(T::id, static_cast<any_reified &&>(value),
                             static_cast<string_type &&>(otlpId), "{}"sv,
                             allocator);
    }

    [[nodiscard]] auto id() const noexcept -> resource_id
    {
        return mId;
    }

    [[nodiscard]] auto stringify() const -> result<std::string_view>
    {
        if (mStringified.empty())
        {
            DPLX_TRY(mStringified, mValue.stringify(mFormatSpec));
        }
        return mStringified;
    }
};

} // namespace dplx::dlog

template <>
class dplx::dp::codec<dplx::dlog::any_attribute>
{
    using any_attribute = dplx::dlog::any_attribute;

public:
    static auto size_of(emit_context &ctx, any_attribute const &value) noexcept
            -> std::uint64_t
    {
        return dp::encoded_size_of(ctx, value.mId)
             + dp::encoded_size_of(ctx, value.mValue);
    }
    static auto encode(emit_context &ctx, any_attribute const &value) noexcept
            -> result<void>
    {
        DPLX_TRY(dp::encode(ctx, value.mId));
        return dp::encode(ctx, value.mValue);
    }
};
