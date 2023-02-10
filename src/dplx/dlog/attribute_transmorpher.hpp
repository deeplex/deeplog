
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <memory>
#include <memory_resource>
#include <string>
#include <string_view>

#include <parallel_hashmap/phmap.h>

#include <dplx/dp.hpp>
#include <dplx/dp/api.hpp>
#include <dplx/dp/codecs/core.hpp>
#include <dplx/dp/items/parse_ranges.hpp>

#include <dplx/dlog/attributes.hpp>
#include <dplx/dlog/detail/utils.hpp>

namespace dplx::dlog
{

class record_attribute_base
{
public:
    virtual ~record_attribute_base() = default;

    virtual auto stringify() noexcept -> result<std::string_view> = 0;
};

template <typename ReType>
class basic_record_attribute;

class record_attribute_reviver;

template <detail::integer ReType>
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
class basic_record_attribute<ReType> final : public record_attribute_base
{
public:
    using value_type = ReType;

private:
    value_type mValue;
    std::pmr::string mStringified;

public:
    auto value() noexcept -> value_type &
    {
        return mValue;
    }

    auto stringify() noexcept -> result<std::string_view> override
    {
        if (mStringified.empty())
        {
            try
            {
                mStringified = fmt::to_string(mValue);
            }
            catch (std::bad_alloc const &)
            {
                return system_error::errc::not_enough_memory;
            }
        }
        return mStringified;
    }
};

template <>
class basic_record_attribute<std::string> final : public record_attribute_base
{
public:
    using value_type = std::u8string;

private:
    value_type mValue;

public:
    auto value() noexcept -> value_type &
    {
        return mValue;
    }

    auto stringify() noexcept -> result<std::string_view> override
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return std::string_view(reinterpret_cast<char const *>(mValue.data()),
                                mValue.size());
    }
};

} // namespace dplx::dlog

namespace dplx::dlog
{

class polymorphic_attribute_deleter
{
    using delete_fn = auto(*)(std::pmr::polymorphic_allocator<> &alloc,
                              record_attribute_base *obj) noexcept -> void;

    std::pmr::polymorphic_allocator<> mAllocator{};
    delete_fn mDelete{};

public:
    polymorphic_attribute_deleter() noexcept = default;

private:
    polymorphic_attribute_deleter(
            std::pmr::polymorphic_allocator<> const &allocator,
            delete_fn del) noexcept
        : mAllocator(allocator)
        , mDelete(del)
    {
    }

public:
    void operator()(record_attribute_base *obj)
    {
        mDelete(mAllocator, obj);
    }

    template <typename T>
        requires std::is_base_of_v<record_attribute_base, T>
    static auto
    bind(std::pmr::polymorphic_allocator<> const &allocator) noexcept
            -> polymorphic_attribute_deleter
    {
        return {allocator, &delete_attribute<T>};
    }

private:
    template <typename T>
    static void delete_attribute(std::pmr::polymorphic_allocator<> &alloc,
                                 record_attribute_base *obj) noexcept
    {
        alloc.delete_object(static_cast<T *>(obj));
    }
};

template <typename K,
          typename V,
          typename Hash = std::hash<K>,
          typename EqualTo = std::equal_to<>>
using pmr_flat_hash_map = phmap::flat_hash_map<
        K,
        V,
        Hash,
        EqualTo,
        std::pmr::polymorphic_allocator<std::pair<K const, V>>>;

class record_attribute_reviver;

class record_attribute_container
{
    friend class record_attribute_reviver;

    using attribute_ptr = std::unique_ptr<record_attribute_base,
                                          polymorphic_attribute_deleter>;
    using map_type = pmr_flat_hash_map<resource_id, attribute_ptr>;

    map_type mAttributes;

public:
    record_attribute_container() = default;
    explicit record_attribute_container(
            map_type::allocator_type const &allocator)
        : mAttributes(allocator)
    {
    }
};

class record_attribute_reviver
{
    using key_type = resource_id;
    using attribute_ptr = typename record_attribute_container::attribute_ptr;
    using attribute_allocator =
            typename record_attribute_container::map_type::allocator_type;
    using revive_fn
            = auto(*)(dp::parse_context &ctx, attribute_allocator &) noexcept
              -> dp::result<attribute_ptr>;

    using reviver_map_type = phmap::flat_hash_map<key_type, revive_fn>;

    reviver_map_type mKnownTypes;

public:
    auto operator()(dp::parse_context &ctx, record_attribute_container &store)
            -> dp::result<void>
    {
        store.mAttributes.clear();
        auto alloc = store.mAttributes.get_allocator();

        if (auto parseRx = dp::parse_map_finite(
                    ctx, store.mAttributes,
                    [this, &alloc](dp::parse_context &lctx,
                                   record_attribute_container::map_type &lstore,
                                   std::size_t const) noexcept
                    { return decode_attr(lctx, lstore, alloc); });
            parseRx.has_failure())
        {
            return std::move(parseRx).as_failure();
        }
        return dp::oc::success();
    }

    template <typename Attribute>
    auto register_attribute() -> dp::result<void>
    {
        return register_attribute<Attribute::id, typename Attribute::retype>();
    }
    template <resource_id Id, typename ReType>
    auto register_attribute() -> dp::result<void>
    {
        try
        {
            revive_fn fn = &revive<basic_record_attribute<ReType>>;
            mKnownTypes.insert({Id, fn});
            return dp::oc::success();
        }
        catch (std::bad_alloc const &)
        {
            return system_error::errc::not_enough_memory;
        }
    }

private:
    auto decode_attr(dp::parse_context &ctx,
                     record_attribute_container::map_type &store,
                     record_attribute_container::map_type::allocator_type
                             &alloc) noexcept -> dp::result<void>
    {
        DPLX_TRY(auto key, dp::decode(dp::as_value<key_type>, ctx));

        if (auto it = mKnownTypes.find(key); it != mKnownTypes.end())
        {
            revive_fn reviveAttribute = it->second;
            DPLX_TRY(auto &&attr, reviveAttribute(ctx, alloc));

            try
            {
                if (!store.try_emplace(key, std::move(attr)).second)
                {
                    // ignore duplicate keys
                    return dp::oc::success();
                }
            }
            catch (std::bad_alloc const &)
            {
                return system_error::errc::not_enough_memory;
            }

            return dp::oc::success();
        }

        return dp::errc::bad;
    }

    template <typename ReType>
    static auto revive(dp::parse_context &ctx,
                       attribute_allocator &allocator) noexcept
            -> dp::result<attribute_ptr>
    {
        try
        {
            auto rawAttr = allocator.new_object<ReType>();
            dp::result<attribute_ptr> rx(
                    std::in_place_type<attribute_ptr>, rawAttr,
                    polymorphic_attribute_deleter::bind<ReType>(allocator));

            if (auto &&decodeRx = dp::decode(ctx, rawAttr->value());
                !oc::try_operation_has_value(decodeRx))
            {
                return oc::try_operation_return_as(std::move(decodeRx));
            }
            return rx;
        }
        catch (std::bad_alloc const &)
        {
            return system_error::errc::not_enough_memory;
        }
    }
};

} // namespace dplx::dlog
