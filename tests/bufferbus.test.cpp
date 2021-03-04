
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include <dplx/dlog/log_bus.hpp>

#include <dplx/dp/decoder/api.hpp>
#include <dplx/dp/decoder/core.hpp>
#include <dplx/dp/encoder/api.hpp>
#include <dplx/dp/encoder/core.hpp>

#include "boost-test.hpp"
#include "test-utils.hpp"

namespace dlog_tests
{

BOOST_AUTO_TEST_SUITE(bufferbus)

BOOST_AUTO_TEST_CASE(tmp)
{
    auto createRx
            = dlog::bufferbus(llfio::mapped_temp_inode().value(), 64 * 1024);
    DPLX_REQUIRE_RESULT(createRx);

    auto bufferbus = std::move(createRx).assume_value();

    auto msgId = 0u;
    dlog::bufferbus_handle::logger_token token{};
    for (;;)
    {
        auto size = dp::encoded_size_of(msgId);
        if (auto writeRx = bufferbus.write(token, size); writeRx.has_value())
        {
            auto encodeRx = dp::encode(writeRx.assume_value(), msgId);
            DPLX_REQUIRE_RESULT(encodeRx);

            bufferbus.commit(token);
            msgId += 1;
        }
        else if (writeRx.assume_error() == dlog::errc::not_enough_space)
        {
            break;
        }
        else
        {
            DPLX_REQUIRE_RESULT(writeRx);
        }
    }

    auto const endId = msgId;
    msgId = 0;

    auto consumeRx
            = bufferbus.consume_content([&](std::span<std::byte const> msg) {
                  dp::const_byte_buffer_view msgBuffer(msg);

                  auto decodeRx
                          = dp::decode(dp::as_value<unsigned int>, msgBuffer);
                  DPLX_REQUIRE_RESULT(decodeRx);

                  auto parsedId = decodeRx.assume_value();
                  BOOST_TEST_REQUIRE(parsedId == msgId);
                  msgId += 1;
              });

    DPLX_REQUIRE_RESULT(consumeRx);
    BOOST_TEST(endId == msgId);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace dlog_tests
