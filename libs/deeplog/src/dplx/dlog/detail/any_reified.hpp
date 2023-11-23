
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <fmt/format.h>

#include <dplx/cncr/utils.hpp>
#include <dplx/dp/api.hpp>
#include <dplx/dp/codecs/core.hpp>

#include <dplx/dlog/disappointment.hpp>
#include <dplx/dlog/loggable.hpp>

namespace dplx::dlog::detail
{

template <typename Allocator, typename T>
using mp_bind_alloc =
        typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
template <typename Allocator, typename T>
using mp_bind_traits =
        typename std::allocator_traits<Allocator>::template rebind_traits<T>;

template <typename Allocator>
class reified_value_base
{
protected:
    ~reified_value_base() noexcept = default;

public:
    using allocator_type = Allocator;
    using string_type
            = std::basic_string<char,
                                std::char_traits<char>,
                                typename std::allocator_traits<Allocator>::
                                        template rebind_alloc<char>>;

    virtual void destroy_self(allocator_type &allocator) noexcept = 0;
    virtual auto move_to(allocator_type &targetAlloc,
                         allocator_type &sourceAlloc)
            -> reified_value_base * = 0;
    [[nodiscard]] virtual auto
    stringify(std::string_view formatSpec,
              allocator_type const &allocator) const noexcept
            -> result<string_type>
            = 0;
    [[nodiscard]] virtual auto
    encoded_size(dp::emit_context &ctx) const noexcept -> std::uint64_t
            = 0;
    [[nodiscard]] virtual auto encode(dp::emit_context &ctx) const noexcept
            -> result<void>
            = 0;
};

template <typename T, typename Allocator>
class basic_reified_value final : public reified_value_base<Allocator>
{
    using base = reified_value_base<Allocator>;

    T mValue;

public:
    ~basic_reified_value() = default;
    basic_reified_value() = default;

    basic_reified_value(basic_reified_value &&) noexcept(
            std::is_nothrow_move_constructible_v<T>)
            = default;
    auto operator=(basic_reified_value &&) noexcept(
            std::is_nothrow_move_assignable_v<T>)
            -> basic_reified_value & = default;

    explicit basic_reified_value(Allocator const &allocator)
        : mValue(std::make_obj_using_allocator<T>(allocator))
    {
    }
    explicit basic_reified_value(T &&value, Allocator const &allocator)
        : mValue(std::make_obj_using_allocator<T>(allocator,
                                                  static_cast<T &&>(value)))
    {
    }

    static auto reify(dp::parse_context &ctx,
                      typename base::allocator_type &allocator) noexcept
            -> result<base *>
    try
    {
        using allocator_t = mp_bind_alloc<Allocator, basic_reified_value>;
        using allocator_traits = mp_bind_traits<Allocator, basic_reified_value>;
        allocator_t reboundAlloc(allocator);

        auto *self = allocator_traits::allocate(reboundAlloc, 1U);
        allocator_traits::construct(reboundAlloc, self);

        if (auto decodeRx = dp::decode(ctx, self->mValue); decodeRx.has_error())
                [[unlikely]]
        {
            allocator_traits::destroy(reboundAlloc, self);
            allocator_traits::deallocate(reboundAlloc, self, 1U);

            return static_cast<decltype(decodeRx) &&>(decodeRx).assume_error();
        }
        return self;
    }
    catch (std::bad_alloc const &)
    {
        return dp::errc::not_enough_memory;
    }

    void
    destroy_self(typename base::allocator_type &allocator) noexcept override
    {
        using allocator_t = mp_bind_alloc<Allocator, basic_reified_value>;
        using allocator_traits = mp_bind_traits<Allocator, basic_reified_value>;
        allocator_t reboundAlloc(allocator);

        // referring to `this` after `destroy()` is UB,
        // therefore make a copy of `this` pointer beforehands
        typename allocator_traits::pointer memory = this;
        allocator_traits::destroy(reboundAlloc, this);
        allocator_traits::deallocate(reboundAlloc, memory, 1U);
    }
    auto move_to(typename base::allocator_type &targetAlloc,
                 typename base::allocator_type &sourceAlloc) -> base * override
    {
        using allocator_t = mp_bind_alloc<Allocator, basic_reified_value>;
        using allocator_traits = mp_bind_traits<Allocator, basic_reified_value>;
        allocator_t reboundTargetAlloc(targetAlloc);

        auto *moved = allocator_traits::allocate(reboundTargetAlloc, 1U);
        allocator_traits::construct(reboundTargetAlloc, moved,
                                    static_cast<T &&>(mValue));

        basic_reified_value::destroy_self(sourceAlloc);
        return moved;
    }
    [[nodiscard]] auto
    stringify(std::string_view formatSpec,
              typename base::allocator_type const &allocator) const noexcept
            -> result<typename base::string_type> override
    try
    {
        fmt::basic_memory_buffer<char, fmt::inline_buffer_size,
                                 typename base::string_type::allocator_type>
                buffer(allocator);
        fmt::format_to(std::back_inserter(buffer), fmt::runtime(formatSpec),
                       mValue);
        return typename base::string_type(buffer.data(), buffer.size(),
                                          allocator);
    }
    catch (std::bad_alloc const &)
    {
        return errc::not_enough_memory;
    }
    catch ([[maybe_unused]] fmt::format_error const &exc)
    {
        return errc::invalid_argument;
    }
    [[nodiscard]] auto encoded_size(dp::emit_context &ctx) const noexcept
            -> std::uint64_t override
    {
        return dp::encoded_size_of(ctx, mValue);
    }
    [[nodiscard]] auto encode(dp::emit_context &ctx) const noexcept
            -> result<void> override
    {
        return dp::encode(ctx, mValue);
    }
};

template <typename Allocator>
class any_reified
{
    friend class dp::codec<any_reified<Allocator>>;

public:
    using allocator_type = Allocator;
    using allocator_traits = std::allocator_traits<allocator_type>;

private:
    using value_container_base = reified_value_base<allocator_type>;

    value_container_base *mReified;
    DPLX_ATTR_NO_UNIQUE_ADDRESS allocator_type mAllocator;

public:
    ~any_reified() noexcept
    {
        if (mReified != nullptr)
        {
            mReified->destroy_self(mAllocator);
        }
    }
    any_reified() noexcept
        : mReified(nullptr)
        , mAllocator()
    {
    }

    any_reified(any_reified &&other) noexcept
        : mReified(std::exchange(other.mReified, nullptr))
        , mAllocator(std::move(other.mAllocator))
    {
    }
    auto operator=(any_reified &&other) noexcept(
            allocator_traits::is_always_equal::value
            // NOLINTNEXTLINE(performance-noexcept-move-constructor)
            || allocator_traits::propagate_on_container_move_assignment::value)
            -> any_reified &
    {
        if (this == &other)
        {
            // do nothing on self assignment
            return *this;
        }

        if constexpr (allocator_traits::is_always_equal::value
                      || allocator_traits::
                              propagate_on_container_move_assignment::value)
        {
            if (mReified != nullptr)
            {
                mReified->destroy_self(mAllocator);
            }
            mReified = std::exchange(other.mReified, nullptr);
            if constexpr (allocator_traits::
                                  propagate_on_container_move_assignment::value)
            {
                mAllocator = std::move(other.mAllocator);
            }
        }
        else
        {
            auto *const old = mReified;
            if (mAllocator == other.mAllocator || other.mReified == nullptr)
            {
                mReified = std::exchange(other.mReified, nullptr);
            }
            else
            {
                mReified
                        = other.mReified->move_to(mAllocator, other.mAllocator);
                other.mReified = nullptr;
            }

            if (old != nullptr)
            {
                old->destroy_self(mAllocator);
            }
        }
        return *this;
    }

    explicit any_reified(allocator_type const &allocator) noexcept
        : mReified(nullptr)
        , mAllocator(allocator)
    {
    }

    template <typename T>
        requires(reifiable<T> && std::default_initializable<T>
                 && std::movable<T>)
    static auto reify(dp::parse_context &ctx,
                      allocator_type const &allocator) noexcept
            -> result<any_reified>
    {
        result<any_reified> rx(std::in_place_type<any_reified>, allocator);
        any_reified &self = rx.assume_value();

        if (auto reifyRx
            = basic_reified_value<T, Allocator>::reify(ctx, self.mAllocator);
            reifyRx.has_value()) [[likely]]
        {
            self.mReified = reifyRx.assume_value();
        }
        else
        {
            rx = static_cast<decltype(reifyRx) &&>(reifyRx).assume_error();
        }
        return rx;
    }

    [[nodiscard]] auto stringify(std::string_view formatSpec) const
            -> result<typename value_container_base::string_type>
    {
        return mReified->stringify(formatSpec, mAllocator);
    }

    friend inline void swap(any_reified &left, any_reified &right) noexcept
    {
        using std::swap;
        swap(left.mReified, right.mReified);
        if constexpr (allocator_traits::propagate_on_container_swap::value)
        {
            swap(left.mAllocator, right.mAllocator);
        }
        else
        {
            assert(left.mAllocator == right.mAllocator);
        }
    }

    [[nodiscard]] auto get_allocator() const noexcept -> Allocator
    {
        return mAllocator;
    }
};

} // namespace dplx::dlog::detail

template <typename Allocator>
class dplx::dp::codec<dplx::dlog::detail::any_reified<Allocator>>
{
    using any_reified = dplx::dlog::detail::any_reified<Allocator>;

public:
    static auto size_of(emit_context &ctx, any_reified const &value) noexcept
            -> std::uint64_t
    {
        return value.mReified != nullptr
                       ? value.mReified->encoded_size(ctx)
                       : dp::encoded_size_of(ctx, dp::null_value);
    }
    static auto encode(emit_context &ctx, any_reified const &value) noexcept
            -> result<void>
    {
        return value.mReified != nullptr ? value.mReified->encode(ctx)
                                         : dp::encode(ctx, dp::null_value);
    }
};
