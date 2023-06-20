
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <concepts>
#include <deque>
#include <string>

#include <boost/unordered/unordered_flat_map.hpp>

#include <fmt/args.h>
#include <fmt/core.h>
#include <fmt/format.h>

#include <dplx/dp.hpp>
#include <dplx/dp/api.hpp>
#include <dplx/dp/codecs/core.hpp>
#include <dplx/dp/cpos/container.hpp>
#include <dplx/dp/items/parse_ranges.hpp>

#include <dplx/dlog/arguments.hpp>
#include <dplx/dlog/core/strong_types.hpp>
#include <dplx/dlog/detail/utils.hpp>
#include <dplx/dlog/disappointment.hpp>
#include <dplx/dlog/loggable.hpp>

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
        auto &inserted = mArgNames.emplace_back(std::move(name));
        return inserted.c_str();
    }

    friend inline auto tag_invoke(dp::container_reserve_fn,
                                  dynamic_format_arg_store &self,
                                  std::size_t const reservationSize) noexcept
            -> dp::result<void>
    {
        try
        {
            self.reserve(reservationSize, reservationSize);
            return oc::success();
        }
        catch (std::bad_alloc const &)
        {
            return dp::errc::not_enough_memory;
        }
    }
};

class argument_transmorpher
{
    using key_type = reification_type_id;
    using parse_context = dp::parse_context;
    using format_context = fmt::format_context;
    using char_type = typename fmt::format_context::char_type;
    using dynamic_arg_store = dynamic_format_arg_store<format_context>;
    using revive_fn = result<void> (*)(parse_context &ctx,
                                       dynamic_arg_store &store,
                                       char const *name) noexcept;

    boost::unordered_flat_map<key_type, revive_fn> mKnownTypes;

public:
    auto operator()(parse_context &ctx, dynamic_arg_store &store)
            -> result<void>
    {
        store.clear();

        if (auto parseRx = dp::parse_array_finite(
                    ctx, store,
                    [this](parse_context &lctx, dynamic_arg_store &lstore,
                           std::size_t const) noexcept
                    { return decode_arg(lctx, lstore); });
            parseRx.has_failure())
        {
            return std::move(parseRx).as_failure();
        }
        return oc::success();
    }

    template <typename T>
        requires reifiable<T>
    auto register_type() -> result<void>
    {
        try
        {
            mKnownTypes.insert(reification_tag_v<T>, &revive_template<T>);
        }
        catch (std::bad_alloc const &)
        {
            return errc::not_enough_memory;
        }
    }

private:
    auto decode_arg(parse_context &ctx, dynamic_arg_store &store) noexcept
            -> result<void>
    {
        DPLX_TRY(dp::item_head tupleHead, dp::parse_item_head(ctx));
        if (tupleHead.type != dp::type_code::array)
        {
            return dp::errc::item_type_mismatch;
        }
        if (tupleHead.indefinite()
            || (tupleHead.value != 2 && tupleHead.value != 3))
        {
            return dp::errc::tuple_size_mismatch;
        }
        if (tupleHead.encoded_length != 1)
        {
            return dp::errc::oversized_additional_information_coding;
        }
        bool const named = tupleHead.value == 3;

        DPLX_TRY(key_type key, dp::decode(dp::as_value<key_type>, ctx));

        char const *name = nullptr;
        if (named)
        {
            try
            {
                std::string nameStore;
                DPLX_TRY(dp::parse_text_finite(ctx, nameStore));

                name = store.persist_arg_name(std::move(nameStore));
            }
            catch (std::bad_alloc const &)
            {
                return dp::errc::not_enough_memory;
            }
        }

        if (key == reification_tag_v<std::uint64_t>)
        {
            return revive_template<std::uint64_t>(ctx, store, name);
        }
        if (key == reification_tag_v<std::int64_t>)
        {
            return revive_template<std::int64_t>(ctx, store, name);
        }
        if (auto it = mKnownTypes.find(key); it != mKnownTypes.end())
        {
            revive_fn revive = it->second;
            return revive(ctx, store, name);
        }

        return errc::unknown_argument_type_id;
    }

    template <typename T>
    static auto revive_template(parse_context &ctx,
                                dynamic_arg_store &store,
                                char_type const *name) noexcept -> result<void>
    try
    {
        DPLX_TRY(auto &&value, dp::decode(dp::as_value<T>, ctx));
        if (name == nullptr)
        {
            store.push_back(static_cast<decltype(value) &&>(value));
        }
        else
        {
            store.push_back(
                    fmt::arg(name, static_cast<decltype(value) &&>(value)));
        }
        return oc::success();
    }
    catch (std::bad_alloc const &)
    {
        return dp::errc::not_enough_memory;
    }
};

} // namespace dplx::dlog
