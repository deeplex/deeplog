
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>
#include <cstdint>

#include <memory_resource>
#include <string_view>

#include <fmt/format.h>

#include <parallel_hashmap/phmap.h>

#include <dplx/dp/decoder/api.hpp>
#include <dplx/dp/decoder/core.hpp>
#include <dplx/dp/decoder/object_utils.hpp>
#include <dplx/dp/decoder/std_string.hpp>
#include <dplx/dp/decoder/tuple_utils.hpp>

#include <dplx/dlog/argument_transmorpher_fmt.hpp>
#include <dplx/dlog/attributes.hpp>
#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/sink.hpp>

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

template <detail::integer ReType>
class basic_record_attribute<ReType> final : record_attribute_base
{
public:
    using value_type = ReType;

private:
    value_type mValue;
    std::pmr::string mStringified;

public:
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
                return std::errc::not_enough_memory;
            }
        }
        return mStringified;
    }
};

class polymorphic_attribute_deleter
{
    using delete_fn = auto (*)(std::pmr::polymorphic_allocator<> &alloc,
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

template <dp::input_stream Stream>
class record_attribute_reviver;

class record_attribute_container
{
    template <dp::input_stream Stream>
    friend class record_attribute_reviver;

    using attribute_ptr = std::unique_ptr<record_attribute_base,
                                          polymorphic_attribute_deleter>;
    using map_type = pmr_flat_hash_map<resource_id, attribute_ptr>;

    map_type mAttributes;
};

template <dp::input_stream Stream>
class record_attribute_reviver
{
    using key_type = resource_id;
    using attribute_ptr = typename record_attribute_container::attribute_ptr;
    using attribute_allocator =
            typename record_attribute_container::map_type::allocator_type;
    using revive_fn = auto (*)(Stream &stream) noexcept -> result<void>;

    using reviver_map_type = phmap::flat_hash_map<key_type, revive_fn>;

    reviver_map_type mKnownTypes;

    using parse = dp::item_parser<Stream>;

public:
    auto operator()(Stream &inStream, record_attribute_container &store)
            -> result<void>
    {
        store.mAttributes.clear();

        return parse::map_finite(
                inStream, store.mAttributes,
                [this](Stream &inStream,
                       record_attribute_container::map_type &store,
                       std::size_t const)
                { return decode_attr(inStream, store); });
    }

    template <resource_id Id, typename T, typename ReType>
    auto register_attribute()
    {
        try
        {
            mKnownTypes.insert(argument<T>::type_id, &revive<T>);
        }
        catch (std::bad_alloc const &)
        {
            return std::errc::not_enough_memory;
        }
    }

private:
    auto decode_attr(Stream &inStream,
                     record_attribute_container::map_type &store)
            -> result<void>
    {
        DPLX_TRY(auto key, dp::decode(dp::as_value<key_type>, inStream));

        if (auto it = mKnownTypes.find(key); it != mKnownTypes.end())
        {
            revive_fn revive = it->second;
            DPLX_TRY(auto attr, revive(inStream));

            try
            {
                if (!store.try_emplace(std::move(key), std::move(attr)))
                {
                    // ignore duplicate keys
                    return oc::success();
                }
            }
            catch (std::bad_alloc const &)
            {
                return std::errc::not_enough_memory;
            }

            return oc::success();
        }

        return errc::bad;
    }

    template <typename ReType>
    static auto revive(Stream &inStream, attribute_allocator &allocator)
            -> result<attribute_ptr>
    {
        try
        {
            result<attribute_ptr> rx(
                    std::in_place_type<attribute_ptr>,
                    allocator.new_object<ReType>(),
                    polymorphic_attribute_deleter::bind<ReType>(allocator));

            if (auto &&decodeRx = dp::decode(inStream, rx.assume_value());
                !oc::try_operation_has_value(decodeRx))
            {
                rx = oc::try_operation_return_as(std::move(decodeRx));
            }
            return rx;
        }
        catch (std::bad_alloc const &)
        {
            return std::errc::not_enough_memory;
        }
    }
};

class record
{
public:
    dlog::severity severity;
    std::uint64_t timestamp; // FIXME: correct timestamp type
    std::string message;
    record_attribute_container attributes;
    dynamic_format_arg_store<fmt::format_context> format_arguments;
};

} // namespace dplx::dlog

namespace dplx::dp
{

template <input_stream Stream>
class basic_decoder<dlog::record, Stream>
{
    using parse = item_parser<Stream>;

public:
    using value_type = dlog::record;

    dlog::argument_transmorpher<Stream> &parse_arguments;
    dlog::record_attribute_reviver<Stream> &parse_attributes;

    auto operator()(Stream &inStream, value_type &value) -> result<void>
    {
        DPLX_TRY(parse::expect(inStream, type_code::array, 5U));

        DPLX_TRY(dp::decode(inStream, value.severity));
        DPLX_TRY(dp::decode(inStream, value.timestamp));
        DPLX_TRY(dp::decode(inStream, value.message));

        DPLX_TRY(parse_attributes(inStream, value.attributes));

        DPLX_TRY(parse_arguments(inStream, value.format_arguments));
        return oc::success();
    }
};

} // namespace dplx::dp

namespace dplx::dlog
{

class record_container
{
public:
    std::unique_ptr<std::pmr::memory_resource> memory_resource;
    file_info info;
    std::pmr::vector<record> records;

};

} // namespace dplx::dlog
