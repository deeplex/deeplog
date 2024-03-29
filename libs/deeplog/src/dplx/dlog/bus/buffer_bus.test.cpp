
// Copyright Henrik S. Gaßmann 2021-2023.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/bus/buffer_bus.hpp"

#include <catch2/catch_test_macros.hpp>

#include <dplx/dp/api.hpp>
#include <dplx/dp/codecs/core.hpp>
#include <dplx/scope_guard.hpp>

#include "dplx/dlog/concepts.hpp"
#include "test_dir.hpp"
#include "test_utils.hpp"

// NOLINTBEGIN(readability-function-cognitive-complexity)

namespace dlog_tests
{

static_assert(dlog::bus<dlog::bufferbus_handle>);

TEST_CASE("bufferbus buffers messages and outputs them afterwards")
{
    constexpr auto bufferSize = 64 * 1024;
    auto bufferbus
            = dlog::bufferbus(llfio::mapped_temp_inode().value(), bufferSize)
                      .value();

    auto msgId = 0U;
    for (;;)
    {
        auto const size = static_cast<unsigned>(dp::encoded_size_of(msgId));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
        dlog::record_output_buffer_storage outStorage;
        // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
        dlog::record_output_buffer *out;
        if (auto createRx
            = bufferbus.allocate_record_buffer_inplace(outStorage, size, {});
            createRx.has_value())
        {
            out = createRx.assume_value();
        }
        else
        {
            if (createRx.assume_error() == dlog::errc::not_enough_space)
            {
                break;
            }
            createRx.assume_error().throw_exception();
        }
        dlog::record_output_guard busLock(*out);

        dp::encode(*out, msgId).value();
        msgId += 1;
    }

    auto const endId = msgId;
    msgId = 0;

    auto consumeRx = bufferbus.consume_messages(
            [&](std::span<dlog::bytes const> const &msgs) noexcept {
                for (auto const msg : msgs)
                {
                    dp::memory_input_stream msgStream(msg);

                    auto decodeRx
                            = dp::decode(dp::as_value<unsigned int>, msgStream);
                    REQUIRE(decodeRx);

                    [[maybe_unused]] auto parsedId = decodeRx.assume_value();
                    msgId += 1;
                }
            });

    REQUIRE(consumeRx);
    CHECK(endId == msgId);
}

} // namespace dlog_tests

// NOLINTEND(readability-function-cognitive-complexity)
