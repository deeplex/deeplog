
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

#include <dplx/dp/decoder/api.hpp>
#include <dplx/dp/decoder/core.hpp>
#include <dplx/dp/decoder/object_utils.hpp>
#include <dplx/dp/decoder/std_string.hpp>
#include <dplx/dp/decoder/tuple_utils.hpp>
#include <dplx/dp/item_parser.hpp>

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
        DPLX_TRY(dp::item_info tupleHead, parse::generic(inStream));
        if (tupleHead.type != dp::type_code::array)
        {
            return dp::errc::item_type_mismatch;
        }
        if (tupleHead.indefinite()
            || !(3 <= tupleHead.value && tupleHead.value <= 5))
        {
            return dp::errc::tuple_size_mismatch;
        }
        if (tupleHead.encoded_length != 1)
        {
            return dp::errc::oversized_additional_information_coding;
        }
        bool const hasAttributes = tupleHead.value > 3;
        bool const hasFmtArgs = tupleHead.value > 4;

        DPLX_TRY(decode(inStream, value.severity));
        DPLX_TRY(decode(inStream, value.timestamp));
        DPLX_TRY(parse::u8string_finite(inStream, value.message));

        if (hasAttributes)
        {
            DPLX_TRY(parse_attributes(inStream, value.attributes));
        }
        if (hasFmtArgs)
        {
            DPLX_TRY(parse_arguments(inStream, value.format_arguments));
        }
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

namespace dplx::dp
{

template <input_stream Stream>
class basic_decoder<dlog::record_container, Stream>
{
    static constexpr std::span magic{dlog::file_sink_backend::magic};

    using parse = item_parser<Stream>;
    using container = std::pmr::vector<dlog::record>;

public:
    using value_type = dlog::record_container;

    basic_decoder<dlog::record, Stream> &record_decoder;

    auto operator()(Stream &inStream, value_type &value) -> result<void>
    {
        {
            DPLX_TRY(auto magicProxy, read(inStream, magic.size()));

            if (!std::ranges::equal(magicProxy, magic))
            {
                return dlog::errc::invalid_record_container_header;
            }

            if constexpr (lazy_input_stream<Stream>)
            {
                DPLX_TRY(consume(inStream, magicProxy));
            }
        }

        DPLX_TRY(decode(inStream, value.info));

        DPLX_TRY(parse::array(
                inStream, value.records,
                [this](Stream &linStream, container &records, std::size_t const)
                { return parse_item(linStream, records); }));

        return oc::success();
    }

private:
    auto parse_item(Stream &inStream, container &records) -> result<void>
    {
        auto &record = records.emplace_back(dlog::record{
                .severity = dlog::severity::none,
                .timestamp = {},
                .message = {},
                .attributes
                = dlog::record_attribute_container(records.get_allocator()),
                .format_arguments = {}});

        if (auto decodeRx = record_decoder(inStream, record);
            decodeRx.has_failure())
        {
            return std::move(decodeRx).as_failure();
        }
        return oc::success();
    }
};

} // namespace dplx::dp
