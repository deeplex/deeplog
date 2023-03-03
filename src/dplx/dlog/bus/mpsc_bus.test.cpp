
// Copyright Henrik Steffen Gaßmann 2021,2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/bus/mpsc_bus.hpp"

#include <thread>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_predicate.hpp>
#include <catch2/matchers/catch_matchers_quantifiers.hpp>

#include <dplx/dp/api.hpp>
#include <dplx/dp/codecs/core.hpp>
#include <dplx/dp/items/encoded_item_head_size.hpp>
#include <dplx/dp/legacy/memory_input_stream.hpp>

#include <dplx/dlog/concepts.hpp>
#include <dplx/dlog/log_bus.hpp>

#include "test_dir.hpp"
#include "test_utils.hpp"

// NOLINTBEGIN(readability-function-cognitive-complexity)

namespace dlog_tests
{

static_assert(dlog::bus<dlog::mpsc_bus_handle>);

TEST_CASE("mpsc_bus() creates a mpsc_bus_handle given a mapped_file_handle")
{
    auto backingFile = llfio::mapped_temp_inode().value();
    auto clonedBackingFile = backingFile.reopen(0U).value();

    auto createRx = dlog::mpsc_bus(std::move(clonedBackingFile), 1U,
                                   dlog::mpsc_bus_handle::min_region_size);

    REQUIRE(createRx);
}

TEST_CASE("mpsc_bus can be filled and drained")
{
    auto bufferbus = dlog::mpsc_bus(llfio::mapped_temp_inode().value(), 2U,
                                    dlog::mpsc_bus_handle::min_region_size)
                             .value();

    auto msgId = 0U;
    for (;;)
    {
        auto const size = static_cast<unsigned>(dp::encoded_size_of(msgId));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
        dlog::output_buffer_storage outStorage;
        // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
        dlog::bus_output_buffer *out;
        if (auto createRx
            = bufferbus.create_output_buffer_inplace(outStorage, size, {});
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
        dlog::bus_output_guard busLock(*out);

        dp::encode(*out, msgId).value();
        msgId += 1;
    }

    auto const endId = msgId;
    msgId = 0;

    auto consumeRx = bufferbus.consume_messages(
            [&](std::span<std::byte const> msg)
            {
                dp::memory_view msgBuffer(msg);

                auto decodeRx
                        = dp::decode(dp::as_value<unsigned int>, msgBuffer);
                REQUIRE(decodeRx);

                [[maybe_unused]] auto parsedId = decodeRx.assume_value();
                msgId += 1;
            });

    REQUIRE(consumeRx);
    CHECK(endId == msgId);
}

namespace
{

auto fill_mpsc_bus(dlog::mpsc_bus_handle &bus, unsigned const limit)
        -> dlog::result<void>
{
    for (unsigned i = 0U; i < limit; ++i)
    {
        /*
        if (i % 32U == 0U)
        {
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1ms);
        }
        */

        auto const encodedSize
                = static_cast<unsigned>(dplx::dp::encoded_size_of(i));
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
        dlog::output_buffer_storage outStorage;
        DPLX_TRY(auto *outStream,
                 bus.create_output_buffer_inplace(outStorage, encodedSize, {}));

        dlog::bus_output_guard busLock(*outStream);
        DPLX_TRY(dplx::dp::encode(*outStream, i));
    }
    return dlog::oc::success();
}

auto consume_content(dlog::mpsc_bus_handle &bus, std::span<std::uint8_t> ids)
        -> dlog::result<void>
{
    return bus.consume_messages(
            [ids](std::span<std::byte const> const &msg)
            {
                dplx::dp::memory_view legacyBuffer(msg);
                auto value = dplx::dp::decode(dplx::dp::as_value<unsigned>,
                                              legacyBuffer)
                                     .value();
                assert(value < ids.size());
                ++ids[value];
            });
}

} // namespace

TEST_CASE("mpsc_bus can be concurrently filled and drained",
          "[.][long-running][concurrent]")
{
    static auto const concurrency
            = std::max(std::thread::hardware_concurrency(), 4U) - 2U;
    constexpr auto msgsPerThread = 4 * 1024U;
    static_assert(dplx::dp::encoded_item_head_size<dp::type_code::posint>(
                          msgsPerThread)
                  < sizeof(std::uint32_t));
    constexpr auto sizePerThread = 2 * sizeof(std::uint32_t) * msgsPerThread;

    auto bufferbus
            = dlog::mpsc_bus(
                      llfio::mapped_file(
                              test_dir, "concurrent_bus.dmsb",
                              dlog::llfio::handle::mode::write,
                              dlog::llfio::handle::creation::only_if_not_exist)
                              .value(),
                      2U, (sizePerThread / 2U) * concurrency)
                      .value();

    std::vector<dlog::result<void>> threadResults;
    threadResults.reserve(concurrency);
    std::vector<std::jthread> threads;
    threads.reserve(concurrency);
    for (unsigned i = 0U; i < concurrency; ++i)
    {
        auto *rx = &threadResults.emplace_back(dlog::oc::success());
        threads.emplace_back(
                [&bufferbus, rx]
                { *rx = fill_mpsc_bus(bufferbus, msgsPerThread); });
    }

    std::vector<std::uint8_t> poppedIds(msgsPerThread, std::uint8_t{});
    for (unsigned i = 0U; i < concurrency; ++i)
    {
        using namespace std::chrono_literals;
        REQUIRE(consume_content(bufferbus, poppedIds));
        // std::this_thread::sleep_for(3ms);
    }

    threads.clear();
    REQUIRE(consume_content(bufferbus, poppedIds));

    REQUIRE_THAT(threadResults,
                 Catch::Matchers::AllMatch(
                         Catch::Matchers::Predicate<dlog::result<void>>(
                                 [](dlog::result<void> const &rx)
                                 { return rx.has_value(); },
                                 "All threads should complete successfully")));
    REQUIRE_THAT(
            poppedIds,
            Catch::Matchers::AllMatch(Catch::Matchers::Predicate<std::uint8_t>(
                    [](unsigned v) { return v == concurrency; },
                    "All message ids should have been queued "
                    "'concurrency' times")));
}

} // namespace dlog_tests

// NOLINTEND(readability-function-cognitive-complexity)