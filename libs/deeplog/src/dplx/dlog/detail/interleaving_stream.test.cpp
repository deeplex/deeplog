
// Copyright Henrik S. Gaßmann 2021-2022.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/detail/interleaving_stream.hpp"

#include <catch2/catch_test_macros.hpp>

#include "test_dir.hpp"
#include "test_utils.hpp"

namespace dlog_tests
{

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)

TEST_CASE("interleaving output stream flushes its buffers")
{
    auto fileOpenRx = llfio::file(test_dir, "interleaving_stream.bin",
                                  llfio::file_handle::mode::write,
                                  llfio::file_handle::creation::always_new);
    REQUIRE(fileOpenRx);
    auto &&file = fileOpenRx.value();

    constexpr std::size_t bufferSize = 4096;
    {
        auto interleavingStream
                = dlog::interleaving_output_stream_handle::
                          interleaving_output_stream(file, false)
                                  .value();

        std::byte buffer[bufferSize] = {};
        REQUIRE(interleavingStream.bulk_write(
                std::span(buffer).first(bufferSize - 1)));

        {
            REQUIRE(interleavingStream.ensure_size(3U));
            auto *out = interleavingStream.begin();
            out[0] = std::byte{'a'};
            out[1] = std::byte{'b'};
            out[2] = std::byte{'c'};
            interleavingStream.commit_written(3U);
        }

        REQUIRE(interleavingStream.sync_output());
    }

    {
        auto interleavingStream = dlog::interleaving_input_stream_handle::
                                          interleaving_input_stream(file, false)
                                                  .value();

        REQUIRE(interleavingStream.discard_input(bufferSize - 1));

        std::byte buffer[3];
        auto readRx = interleavingStream.bulk_read(buffer);
        REQUIRE(readRx);

        REQUIRE(buffer[0] == std::byte{'a'});
        REQUIRE(buffer[1] == std::byte{'b'});
        REQUIRE(buffer[2] == std::byte{'c'});
    }
}

TEST_CASE("interleaving output stream flushes its buffers bulk")
{
    auto fileOpenRx = llfio::file(test_dir, "interleaving_stream2.bin",
                                  llfio::file_handle::mode::write,
                                  llfio::file_handle::creation::always_new);
    REQUIRE(fileOpenRx);
    auto &&file = fileOpenRx.value();

    constexpr std::size_t bufferSize = 4096;
    {
        auto interleavingStream
                = dlog::interleaving_output_stream_handle::
                          interleaving_output_stream(file, false)
                                  .value();

        std::byte buffer[bufferSize] = {};
        REQUIRE(interleavingStream.bulk_write(
                std::span(buffer).first(bufferSize - 1)));

        {
            buffer[0] = std::byte{'a'};
            buffer[1] = std::byte{'b'};
            buffer[2] = std::byte{'c'};
            REQUIRE(interleavingStream.bulk_write(std::span(buffer).first(3)));
        }

        REQUIRE(interleavingStream.finalize());
    }

    {
        auto interleavingStream = dlog::interleaving_input_stream_handle::
                                          interleaving_input_stream(file, false)
                                                  .value();

        REQUIRE(interleavingStream.discard_input(bufferSize - 1));

        std::byte buffer[3];
        auto readRx = interleavingStream.bulk_read(buffer);
        REQUIRE(readRx);

        REQUIRE(buffer[0] == std::byte{'a'});
        REQUIRE(buffer[1] == std::byte{'b'});
        REQUIRE(buffer[2] == std::byte{'c'});
    }
}

// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

} // namespace dlog_tests
