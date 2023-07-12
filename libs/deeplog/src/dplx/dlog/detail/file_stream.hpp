
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <dplx/dp/legacy/chunked_input_stream.hpp>
#include <dplx/dp/legacy/memory_buffer.hpp>
#include <dplx/predef/os.h>

#include <dplx/dlog/disappointment.hpp>
#include <dplx/dlog/llfio.hpp>

namespace dplx::dlog::detail
{

class os_input_stream final : public dp::input_buffer
{
    friend class oc::basic_result<
            os_input_stream,
            system_error::errored_status_code<system_error::erased<
                    system_error::system_code::value_type>>,
            oc::experimental::policy::default_status_result_policy<
                    os_input_stream,
                    system_error::errored_status_code<system_error::erased<
                            system_error::system_code::value_type>>>>;

    using byte_io_handle = llfio::byte_io_handle;
    using page_allocation
            = dp::memory_allocation<llfio::utils::page_allocator<std::byte>>;

    page_allocation mBufferAllocation;
    llfio::byte_io_handle *mDataSource;
    std::uint64_t mReadOffset;

public:
    static constexpr std::uint64_t max_stream_size
            = std::numeric_limits<byte_io_handle::extent_type>::max();

    ~os_input_stream() noexcept = default;
    explicit os_input_stream() noexcept
        : input_buffer(nullptr, 0U, 0U)
        , mBufferAllocation()
        , mDataSource(nullptr)
        , mReadOffset(0U)
    {
    }

    explicit os_input_stream(byte_io_handle &dataSource,
                             std::uint64_t maxSize = max_stream_size) noexcept
        : input_buffer(nullptr, 0U, maxSize)
        , mBufferAllocation()
        , mDataSource(&dataSource)
        , mReadOffset(maxSize)
    {
    }

    explicit os_input_stream(byte_io_handle &dataSource,
                             std::uint64_t maxSize,
                             page_allocation &&buffer,
                             std::byte const *initialReadArea,
                             std::size_t initialReadAreaSize) noexcept
        : input_buffer(initialReadArea, initialReadAreaSize, maxSize)
        , mBufferAllocation(static_cast<page_allocation &&>(buffer))
        , mDataSource(&dataSource)
        , mReadOffset(initialReadAreaSize)
    {
    }

private:
    static constexpr unsigned page_size = 1U << 12;
    static constexpr unsigned buffer_size = page_size * 16;

public:
    static auto create(llfio::byte_io_handle &dataSource,
                       std::uint64_t maxSize) noexcept
            -> result<os_input_stream>
    {
        return create(page_allocation{}, dataSource, maxSize);
    }

private:
    static auto create(page_allocation &&pages,
                       llfio::byte_io_handle &dataSource,
                       std::uint64_t maxSize) noexcept
            -> result<os_input_stream>
    {
        if (!dataSource.is_readable() || !dataSource.is_seekable())
        {
            return errc::invalid_argument;
        }

        DPLX_TRY(pages.resize(buffer_size));
        auto const buffer = pages.as_span();

        DPLX_TRY(auto initialReadArea,
                 read_chunk(&dataSource, 0U, buffer.data(), buffer.size()));
        auto const initialUsage = std::min<std::uint64_t>(maxSize, buffer_size);

        return result<os_input_stream>(std::in_place_type<os_input_stream>,
                                       dataSource, maxSize,
                                       static_cast<page_allocation &&>(pages),
                                       initialReadArea.data(), initialUsage);
    }

    auto do_require_input(size_type const requiredSize) noexcept
            -> dp::result<void> override
    {
        DPLX_TRY(read_next_chunk());
        if (input_buffer::size() < requiredSize)
        {
            return requiredSize > dp::minimum_input_buffer_size
                         ? dp::errc::buffer_size_exceeded
                         : dp::errc::end_of_stream;
        }
        return oc::success();
    }
    auto do_discard_input(size_type const amount) noexcept
            -> dp::result<void> override
    {
        mReadOffset += amount;
        auto const remainingInput = input_size() - amount;
        reset(nullptr, 0U, remainingInput);
        if (remainingInput > 0U)
        {
            return read_next_chunk();
        }
        return oc::success();
    }
    auto do_bulk_read(std::byte *const dest,
                      std::size_t const destSize) noexcept
            -> dp::result<void> override
    {
        std::span<std::byte> destinationBuffer(dest, destSize);
        if (mDataSource->requires_aligned_io())
        {
            return dp::errc::bad;
        }

        while (destinationBuffer.size() > buffer_size)
        {
            DPLX_TRY(auto &&ioBuffer, read_chunk(mDataSource, mReadOffset,
                                                 destinationBuffer.data(),
                                                 destinationBuffer.size()));

            if (ioBuffer.data() != destinationBuffer.data())
            {
                std::memcpy(destinationBuffer.data(), ioBuffer.data(),
                            ioBuffer.size());
            }

            destinationBuffer = destinationBuffer.subspan(ioBuffer.size());
            mReadOffset += ioBuffer.size();
            reset(nullptr, 0U, input_size() - ioBuffer.size());
        }
        while (empty() && input_size() > 0U)
        {
            DPLX_TRY(read_next_chunk());

            auto const transferAmount
                    = std::min(size(), destinationBuffer.size());
            std::memcpy(destinationBuffer.data(), data(), transferAmount);

            destinationBuffer = destinationBuffer.subspan(transferAmount);
            discard_buffered(transferAmount);
        }
        if (!destinationBuffer.empty())
        {
            return dp::errc::end_of_stream;
        }
        return dp::oc::success();
    }

    auto read_next_chunk() -> dp::result<void>
    {
        if (mBufferAllocation.size() == 0U)
        {
            DPLX_TRY(mBufferAllocation.resize(buffer_size));
        }

        DPLX_TRY(auto nextChunk, read_chunk(mDataSource, mReadOffset - size(),
                                            mBufferAllocation.as_span().data(),
                                            mBufferAllocation.size()));

        reset(nextChunk.data(), nextChunk.size(), input_size());
        mReadOffset += nextChunk.size();
        return dp::oc::success();
    }

    static auto read_chunk(llfio::byte_io_handle *const dataSource,
                           std::uint64_t const readPos,
                           std::byte *readBuffer,
                           std::size_t readBufferSize) noexcept
            -> dp::result<std::span<std::byte const>>
    {
        constexpr auto pageMask = std::uint64_t{page_size - 1};
        auto const alignMask
                = dataSource->requires_aligned_io() ? pageMask : 0U;
        auto const realReadPos = readPos & ~alignMask;
        auto const discard = readPos & alignMask;

        llfio::byte_io_handle::buffer_type ioBuffers[] = {
                {readBuffer, readBufferSize}
        };

        auto readRx = dataSource->read({ioBuffers, realReadPos});
        if (readRx.has_failure())
        {
#if defined(DPLX_OS_WINDOWS_AVAILABLE)
            constexpr system_error::win32_code error_handle_eof{0x00000026};
            constexpr system_error::nt_code status_end_of_file{
                    static_cast<long>(0xc0000011U)};
            if (readRx.assume_error() == error_handle_eof
                || readRx.assume_error() == status_end_of_file)
            {
                return dp::errc::end_of_stream;
            }
#endif
            return static_cast<decltype(readRx) &&>(readRx).as_failure();
        }
        if (readRx.bytes_transferred() <= discard)
        {
            return dp::errc::end_of_stream;
        }
        if (readRx.assume_value().size() == 1U) [[likely]]
        {
            return std::span<std::byte const>{readRx.assume_value()[0]}.subspan(
                    discard);
        }

        auto rdisc = discard;
        for (auto const buffer : readRx.assume_value())
        {
            if (buffer.size() > rdisc)
            {
                return std::span<std::byte const>{buffer}.subspan(rdisc);
            }
            rdisc -= buffer.size();
        }

        return dp::errc::end_of_stream;
    }
};

} // namespace dplx::dlog::detail
