
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <concepts>
#include <deque>
#include <string>

#include <parallel_hashmap/phmap.h>

#include <dplx/dp/concepts.hpp>
#include <dplx/dp/decoder/api.hpp>
#include <dplx/dp/decoder/core.hpp>
#include <dplx/dp/item_parser.hpp>

#include <dplx/dlog/arguments.hpp>
#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/detail/utils.hpp>
#include <dplx/dlog/disappointment.hpp>

namespace dplx::dlog
{

template <typename Context>
class dynamic_format_arg_store : public fmt::dynamic_format_arg_store<Context>
{
    using base_type = fmt::dynamic_format_arg_store<Context>;
    using char_type = typename Context::char_type;

    std::deque<std::basic_string<char_type>> mArgNames;

public:
    auto persist_arg_name(std::basic_string<char_type> name)
            -> char_type const *
    {
        auto &inserted = mArgNames.push_back(std::move(name));
        return inserted.c_str();
    }

    friend inline auto tag_invoke(dp::container_reserve_fn,
                                  dynamic_format_arg_store &self,
                                  std::size_t const reservationSize)
            -> result<void>
    {
        try
        {
            self.reserve(reservationSize, reservationSize);
            return oc::success();
        }
        catch (std::bad_alloc const &)
        {
            return std::errc::not_enough_memory;
        }
    }
};

template <dp::input_stream Stream>
class argument_transmorpher
{
    using key_type = resource_id;
    using format_context = fmt::format_context;
    using char_type = typename fmt::format_context::char_type;
    using dynamic_arg_store = dynamic_format_arg_store<format_context>;
    using revive_fn = result<void> (*)(Stream &str,
                                       dynamic_arg_store &store,
                                       char const *name) noexcept;

    phmap::flat_hash_map<key_type, revive_fn> mKnownTypes;

    using parse = dp::item_parser<Stream>;

public:
    auto operator()(Stream &inStream, dynamic_arg_store &store) -> result<void>
    {
        store.clear();

        return parse::array_finite(
                inStream, store,
                [this](Stream &inStream, dynamic_arg_store &store,
                       std::size_t const)
                { return decode_arg(inStream, store, nullptr); });
    }

    template <typename T>
        requires loggable_argument<T, Stream> && dp::decodable<T, Stream>
    auto register_type() -> result<void>
    {
        try
        {
            mKnownTypes.insert(argument<T>::type_id, &revive_template<T>);
        }
        catch (std::bad_alloc const &)
        {
            return std::errc::not_enough_memory;
        }
    }

private:
    auto decode_arg(Stream &inStream,
                    dynamic_arg_store &store,
                    char const *name) noexcept -> result<void>
    {
        DPLX_TRY(auto key, dp::decode(dp::as_value<key_type>, inStream));

        if (key == argument<std::uint64_t>::type_id)
        {
            return revive_template<std::uint64_t>(inStream, store, name);
        }
        if (key == argument<std::int64_t>::type_id)
        {
            return revive_template<std::int64_t>(inStream, store, name);
        }
        if (key == resource_id{23u}) // named arg
        {
            if (name == nullptr)
            {
                return revive_named_arg(inStream, store);
            }
            else
            {
                return errc::bad; // TODO: proper error code for nested named
                                  // args
            }
        }
        if (auto it = mKnownTypes.find(key); it != mKnownTypes.end())
        {
            revive_fn revive = it->second;
            return revive(inStream, store, name);
        }

        return errc::bad; // TODO: proper error code for unknown arg type id
    }

    auto revive_named_arg(Stream &inStream, dynamic_arg_store &store)
            -> result<void>
    {
        {
            DPLX_TRY(dp::detail::item_info tupleInfo,
                     dp::detail::parse_item_info(inStream));
            if (std::byte{tupleInfo.type} != dp::type_code::array)
            {
                return dp::errc::item_type_mismatch;
            }
            if (tupleInfo.encoded_length != 1)
            {
                return dp::errc::invalid_additional_information;
            }
            if (tupleInfo.value != 3)
            {
                return dp::errc::tuple_size_mismatch;
            }
        }

        char_type const *name;
        try
        {
            DPLX_TRY(dp::detail::item_info nameInfo,
                     dp::detail::parse_item_info(inStream));
            if (std::byte{nameInfo.type} != dp::type_code::text)
            {
                return dp::errc::item_type_mismatch;
            }
            DPLX_TRY(auto &&availableBytes, dp::available_input_size(inStream));
            if (availableBytes < nameInfo.value)
            {
                return dp::errc::missing_data;
            }
            if (!std::in_range<std::size_t>(nameInfo.value))
            {
                return std::errc::not_enough_memory;
            }

            std::string nameStore;
            nameStore.resize(static_cast<std::size_t>(nameInfo.value));

            DPLX_TRY(dp::read(inStream,
                              reinterpret_cast<std::byte *>(nameStore.data()),
                              nameStore.size()));

            name = store.persist_arg_name(std::move(nameStore));
        }
        catch (std::bad_alloc const &)
        {
            return std::errc::not_enough_memory;
        }

        return decode_arg(inStream, store, name);
    }

    template <typename T>
    static auto revive_template(Stream &inStream,
                                dynamic_arg_store &store,
                                char_type const *name) noexcept -> result<void>
    try
    {
        DPLX_TRY(auto value, dp::decode(dp::as_value<T>, inStream));
        if (name == nullptr)
        {
            store.push_back(value);
        }
        else
        {
            store.push_back(fmt::arg(name, value));
        }
        return oc::success();
    }
    catch (std::bad_alloc const &)
    {
        return std::errc::not_enough_memory;
    }
};

} // namespace dplx::dlog
