
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

#include <fmt/core.h>
#include <fmt/format.h>

#include <dplx/dp.hpp>
#include <dplx/dp/api.hpp>
#include <dplx/dp/codecs/auto_object.hpp>
#include <dplx/dp/codecs/auto_tuple.hpp>
#include <dplx/dp/codecs/core.hpp>
#include <dplx/dp/codecs/std-string.hpp>

#include <dplx/dlog/argument_transmorpher_fmt.hpp>
#include <dplx/dlog/attribute_transmorpher.hpp>
#include <dplx/dlog/attributes.hpp>
#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/sink.hpp>

namespace dplx::dlog
{

class record
{
public:
    dlog::severity severity;
    span_context context;
    std::uint64_t timestamp; // FIXME: correct timestamp type
    std::string message;
    dynamic_format_arg_store<fmt::format_context> format_arguments;
    attribute_container attributes;
};

} // namespace dplx::dlog

namespace dplx::dp
{

template <typename T>
class basic_decoder; // TODO: remove

template <>
class basic_decoder<dlog::record> // TODO: transform into a codec
{
public:
    dlog::argument_transmorpher &parse_arguments;

    auto operator()(parse_context &ctx, dlog::record &value) -> result<void>
    {
        DPLX_TRY(dp::item_head tupleHead, dp::parse_item_head(ctx));
        if (tupleHead.type != dp::type_code::array)
        {
            return dp::errc::item_type_mismatch;
        }
        // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
        // TODO: refactor record layout description into compile time constants
        if (tupleHead.indefinite()
            || (tupleHead.value != 2 && tupleHead.value != 6
                && tupleHead.value != 7))
        {
            return dp::errc::tuple_size_mismatch;
        }

        if (tupleHead.value != 6)
        {
            for (unsigned i = 0; i < tupleHead.value; i++)
            {
                DPLX_TRY(dp::skip_item(ctx));
            }
            return dp::success();
        }
        // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)

        DPLX_TRY(decode(ctx, value.severity));
        DPLX_TRY(decode(ctx, value.context));
        DPLX_TRY(decode(ctx, value.timestamp));
        DPLX_TRY(dp::parse_text_finite(ctx, value.message));

        DPLX_TRY(parse_arguments(ctx, value.format_arguments));
        DPLX_TRY(dp::decode(ctx, value.attributes));
        return oc::success();
    }

private:
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

namespace dplx::dp
{

template <>
class basic_decoder<dlog::record_container> // TODO: transform into a codec
{
    static constexpr std::span magic{dlog::file_sink_backend::magic};

    using container = std::pmr::vector<dlog::record>;

public:
    basic_decoder<dlog::record> &record_decoder;

    auto operator()(parse_context &ctx, dlog::record_container &value)
            -> result<void>
    {
        {
            DPLX_TRY(ctx.in.require_input(magic.size()));

            if (!std::ranges::equal(std::span(ctx.in).first(std::size(magic)),
                                    as_bytes(std::span(magic))))
            {
                return dlog::errc::invalid_record_container_header;
            }

            ctx.in.discard_buffered(magic.size());
        }

        DPLX_TRY(dp::decode(ctx, value.info));

        DPLX_TRY(dp::parse_array(ctx, value.records,
                                 [this](parse_context &lctx, container &records,
                                        std::size_t const) noexcept
                                 { return parse_item(lctx, records); }));

        return oc::success();
    }

private:
    auto parse_item(parse_context &ctx, container &records) -> result<void>
    {
        auto &record = records.emplace_back(
                dlog::record{.severity = dlog::severity::none,
                             .context = {},
                             .timestamp = {},
                             .message = {},
                             .format_arguments = {},
                             .attributes = {}});

        auto decodeRx = record_decoder(ctx, record);
        if (decodeRx.has_failure())
        {
            records.pop_back();
        }
        return decodeRx;
    }
};

} // namespace dplx::dp
