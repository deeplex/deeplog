
// Copyright 2021, 2023 Henrik Steffen Gaßmann
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <memory>
#include <memory_resource>

#include <parallel_hashmap/phmap.h>

#include <dplx/dp/fwd.hpp>
#include <dplx/dp/state.hpp>

#include <dplx/dlog/any_attribute.hpp>
#include <dplx/dlog/attributes.hpp>
#include <dplx/dlog/disappointment.hpp>

namespace dplx::dlog
{

class attribute_container
{
    friend class dp::codec<attribute_container>;

public:
    using allocator_type = std::pmr::polymorphic_allocator<
            std::pair<resource_id const, any_attribute>>;

private:
    using map_type = phmap::flat_hash_map<resource_id,
                                          any_attribute,
                                          std::hash<resource_id>,
                                          std::equal_to<>,
                                          allocator_type>;

    map_type mAttributes;

public:
    attribute_container() = default;
    explicit attribute_container(allocator_type const &allocator)
        : mAttributes(allocator)
    {
    }
};

class attribute_type_registry
{
    using key_type = resource_id;
    using revive_fn = auto(*)(dp::parse_context &ctx) noexcept
                      -> dp::result<any_attribute>;

public:
    using allocator_type = std::pmr::polymorphic_allocator<
            std::pair<key_type const, revive_fn>>;

private:
    using reviver_map_type = phmap::flat_hash_map<key_type,
                                                  revive_fn,
                                                  std::hash<key_type>,
                                                  std::equal_to<>,
                                                  allocator_type>;

    reviver_map_type mKnownTypes;

public:
    attribute_type_registry() noexcept = default;
    explicit attribute_type_registry(allocator_type const &alloc)
        : mKnownTypes(alloc)
    {
    }

    auto decode(dp::parse_context &ctx) const noexcept
            -> dp::result<any_attribute>;

    template <attribute T>
    auto insert() -> dlog::result<void>
    try
    {
        revive_fn fn = &any_attribute::reify<T>;
        mKnownTypes.insert({T::id, fn});
        return dp::oc::success();
    }
    catch (std::bad_alloc const &)
    {
        return errc::not_enough_memory;
    }
};

inline constexpr dp::state_key<attribute_type_registry>
        attribute_type_registry_state{
                cncr::uuid("{ac33a72e-67fa-4fe7-8693-3645a4cd8a66}")};

} // namespace dplx::dlog

template <>
class dplx::dp::codec<dplx::dlog::attribute_container>
{
public:
    static auto size_of(emit_context &ctx,
                        dplx::dlog::attribute_container const &attrs) noexcept
            -> std::uint64_t;
    static auto encode(emit_context &ctx,
                       dplx::dlog::attribute_container const &attrs) noexcept
            -> result<void>;
    static auto decode(parse_context &ctx,
                       dplx::dlog::attribute_container &outValue) noexcept
            -> result<void>;
};
