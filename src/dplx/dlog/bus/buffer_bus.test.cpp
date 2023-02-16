
// Copyright Henrik S. Gaßmann 2021-2023.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/bus/buffer_bus.hpp"

#include <catch2/catch_test_macros.hpp>

#include <dplx/dp/api.hpp>
#include <dplx/dp/codecs/core.hpp>

#include "test_dir.hpp"
#include "test_utils.hpp"

// NOLINTBEGIN(readability-function-cognitive-complexity)

namespace dlog_tests
{

TEST_CASE("bufferbus buffers messages and outputs them afterwards")
{
    constexpr auto bufferSize = 64 * 1024;
    auto createRx
            = dlog::bufferbus(llfio::mapped_temp_inode().value(), bufferSize);
    REQUIRE(createRx);

    auto bufferbus = std::move(createRx).assume_value();

    auto msgId = 0U;
    dlog::bufferbus_handle::logger_token token{};
    for (;;)
    {
        auto size = static_cast<unsigned>(dp::encoded_size_of(msgId));
        if (auto writeRx = bufferbus.write(token, size); writeRx.has_value())
        {
            auto encodeRx = dp::encode(writeRx.assume_value(), msgId);
            REQUIRE(encodeRx);

            bufferbus.commit(token);
            msgId += 1;
        }
        else if (writeRx.assume_error() == dlog::errc::not_enough_space)
        {
            break;
        }
        else
        {
            REQUIRE(writeRx);
        }
    }

    auto const endId = msgId;
    msgId = 0;

    auto consumeRx = bufferbus.consume_content(
            [&](std::span<std::byte const> msg)
            {
                auto decodeRx
                        = dp::decode(dp::as_value<unsigned int>, as_bytes(msg));
                REQUIRE(decodeRx);

                auto parsedId = decodeRx.assume_value();
                REQUIRE(parsedId == msgId);
                msgId += 1;
            });

    REQUIRE(consumeRx);
    CHECK(endId == msgId);
}

} // namespace dlog_tests

// NOLINTEND(readability-function-cognitive-complexity)
