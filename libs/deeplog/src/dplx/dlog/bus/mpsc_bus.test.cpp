
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
#include <dplx/dp/streams/memory_input_stream.hpp>

#include <dplx/dlog/concepts.hpp>

#include "test_dir.hpp"
#include "test_utils.hpp"

// NOLINTBEGIN(readability-function-cognitive-complexity)

namespace dlog_tests
{

static_assert(dlog::bus<dlog::mpsc_bus_handle>);
static_assert(dlog::bus<dlog::db_mpsc_bus_handle>);

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

namespace
{

auto fill_mpsc_bus(dlog::mpsc_bus_handle &bus, unsigned const limit)
        -> result<void>
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

        DPLX_TRY(dlog::enqueue_message(bus, {}, i));
    }
    return outcome::success();
}

struct test_content_consumer final : dlog::record_consumer
{
    std::span<std::uint8_t> ids;

    constexpr ~test_content_consumer() noexcept = default;
    constexpr test_content_consumer() noexcept = default;

    constexpr test_content_consumer(test_content_consumer &&) noexcept
            = default;
    constexpr auto operator=(test_content_consumer &&) noexcept
            -> test_content_consumer & = default;

    explicit test_content_consumer(std::span<std::uint8_t> is) noexcept
        : ids(is)
    {
    }

    void operator()(std::span<dlog::bytes const> records) noexcept override
    {
        for (auto const msg : records)
        {
            dp::memory_input_stream msgStream(msg);
            auto value
                    = dp::decode(dp::as_value<unsigned int>, msgStream).value();

            assert(value < ids.size());
            ++ids[value];
        }
    }
};

auto consume_content(dlog::mpsc_bus_handle &bus, std::span<std::uint8_t> ids)
        -> result<void>
{
    test_content_consumer consumeFn(ids);
    return bus.consume_messages(consumeFn);
}

} // namespace

TEST_CASE("mpsc_bus can be concurrently filled and drained")
{
    static auto const concurrency
            = std::max(std::thread::hardware_concurrency(), 4U) - 2U;
    constexpr auto msgsPerThread = 4 * 1024U;
    static_assert(dplx::dp::encoded_item_head_size<dp::type_code::posint>(
                          msgsPerThread)
                  < sizeof(std::uint32_t));
    constexpr auto regionOverhead = 64U;
    constexpr auto sizePerThread
            = 2 * (sizeof(std::uint32_t) * msgsPerThread + regionOverhead);

    auto bufferbus
            = dlog::mpsc_bus(
                      llfio::mapped_file(
                              test_dir, "concurrent_bus.dmsb",
                              dlog::llfio::handle::mode::write,
                              dlog::llfio::handle::creation::only_if_not_exist)
                              .value(),
                      2U, (sizePerThread / 2U) * concurrency)
                      .value();

    std::vector<result<void>> threadResults;
    threadResults.reserve(concurrency);
    std::vector<std::thread> threads;
    threads.reserve(concurrency);
    for (unsigned i = 0U; i < concurrency; ++i)
    {
        auto *rx = &threadResults.emplace_back(outcome::success());
        threads.emplace_back([&bufferbus, rx] {
            *rx = fill_mpsc_bus(bufferbus, msgsPerThread);
        });
    }

    std::vector<std::uint8_t> poppedIds(msgsPerThread, std::uint8_t{});
    for (unsigned i = 0U; i < concurrency; ++i)
    {
        using namespace std::chrono_literals;
        REQUIRE(consume_content(bufferbus, poppedIds));
        // std::this_thread::sleep_for(3ms);
    }

    for (auto &t : threads)
    {
        t.join();
    }
    threads.clear();
    REQUIRE(consume_content(bufferbus, poppedIds));

    CHECK_THAT(
            threadResults,
            Catch::Matchers::AllMatch(Catch::Matchers::Predicate<result<void>>(
                    [](result<void> const &rx) { return rx.has_value(); },
                    "All threads should complete successfully")));
    CHECK_THAT(
            poppedIds,
            Catch::Matchers::AllMatch(Catch::Matchers::Predicate<std::uint8_t>(
                    [](unsigned v) { return v == concurrency; },
                    "All message ids should have been queued "
                    "'concurrency' times")));
}

TEST_CASE("mpsc bus can be recovered")
{
    auto mpscbus = dlog::mpsc_bus(llfio::mapped_temp_inode().value(), 2U,
                                  dlog::mpsc_bus_handle::min_region_size)
                           .value();

    constexpr auto loadFactor = 512U;

    REQUIRE(fill_mpsc_bus(mpscbus, loadFactor));
    auto h = mpscbus.release();

    std::vector<std::uint8_t> poppedIds(loadFactor, std::uint8_t{});
    test_content_consumer consumeFn(poppedIds);
    REQUIRE(dlog::mpsc_bus_handle::recover_mpsc_bus(
            // false positive due to REQUIRE using do-while(0) under the hood
            // NOLINTNEXTLINE(bugprone-use-after-move)
            std::move(h), consumeFn, llfio::lock_kind::exclusive));

    CHECK_THAT(
            poppedIds,
            Catch::Matchers::AllMatch(Catch::Matchers::Predicate<std::uint8_t>(
                    [](unsigned v) { return v == 1; },
                    "All message ids should have been popped.")));
}

} // namespace dlog_tests

// NOLINTEND(readability-function-cognitive-complexity)
