
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

#include <dplx/dp/legacy/chunked_input_stream.hpp>
#include <dplx/dp/legacy/chunked_output_stream.hpp>
#include <dplx/dp/legacy/memory_buffer.hpp>

#include <dplx/dlog/disappointment.hpp>
#include <dplx/dlog/llfio.hpp>

namespace dplx::dlog
{

class interleaving_stream_base
{
protected:
    using page_allocation
            = dp::memory_allocation<llfio::utils::page_allocator<std::byte>>;

    static constexpr unsigned page_size = 1U << 12;

    static constexpr auto index_to_block_size(unsigned idx) noexcept -> unsigned
    {
        return (idx < 5U ? 1U << idx : 1U << 4) * page_size;
    }
    static constexpr auto index_to_block_offset(unsigned idx, bool odd) noexcept
            -> std::uint64_t
    {
        constexpr std::uint64_t two = 2U;
        auto const uodd = static_cast<unsigned>(odd);
        auto const twodd = two | uodd;
        return (idx < 5U ? (twodd) << idx
                         : (two << 4) * (idx - 3U) + (uodd << 4))
             * page_size;
    }
    static constexpr auto index_to_file_size(unsigned idx) noexcept
            -> std::uint64_t
    {
        return index_to_block_offset(idx + 1, false);
    }

    static constexpr auto max_block_size() noexcept -> unsigned
    {
        return index_to_block_size(5U);
    }

    static constexpr std::uint64_t max_stream_size
            = static_cast<std::uint64_t>(std::numeric_limits<unsigned>::max())
            * page_size;
};

class interleaving_input_stream_handle final
    : public dp::legacy::chunked_input_stream_base<
              interleaving_input_stream_handle>
    , interleaving_stream_base
{
    friend class dp::legacy::chunked_input_stream_base<
            interleaving_input_stream_handle>;
    using base_t = dp::legacy::chunked_input_stream_base<
            interleaving_input_stream_handle>;

    page_allocation mBufferAllocation;
    llfio::byte_io_handle *mDataSource;
    unsigned mIndexPosition;
    bool mStreamSelector;

public:
    explicit interleaving_input_stream_handle() noexcept
        : base_t({}, 0U)
        , mBufferAllocation()
        , mDataSource()
        , mIndexPosition()
        , mStreamSelector()
    {
    }

    explicit interleaving_input_stream_handle(llfio::byte_io_handle &dataSource,
                                              bool streamSelector,
                                              std::uint64_t maxSize
                                              = max_stream_size) noexcept
        : base_t({}, maxSize)
        , mBufferAllocation()
        , mDataSource(&dataSource)
        , mIndexPosition(0U)
        , mStreamSelector(streamSelector)
    {
    }

private:
    explicit interleaving_input_stream_handle(
            llfio::byte_io_handle &dataSource,
            bool streamSelector,
            std::uint64_t maxSize,
            page_allocation &&buffer,
            std::span<std::byte const> initialReadArea) noexcept
        : base_t(initialReadArea, maxSize)
        , mBufferAllocation(std::move(buffer))
        , mDataSource(&dataSource)
        , mIndexPosition(1U)
        , mStreamSelector(streamSelector)
    {
    }

public:
    static auto interleaving_input_stream(llfio::byte_io_handle &dataSource,
                                          bool streamSelector,
                                          std::uint64_t maxSize
                                          = max_stream_size) noexcept
            -> result<interleaving_input_stream_handle>
    {
        return interleaving_input_stream(page_allocation{}, dataSource,
                                         streamSelector, maxSize);
    }

    auto reset(bool streamSelector,
               std::uint64_t maxSize = max_stream_size) noexcept -> result<void>
    {
        auto pages = std::move(mBufferAllocation);
        auto dataSource = mDataSource;

        *this = interleaving_input_stream_handle();

        DPLX_TRY(*this, interleaving_input_stream(std::move(pages), *dataSource,
                                                  streamSelector, maxSize));

        return oc::success();
    }

private:
    static auto interleaving_input_stream(page_allocation &&pages,
                                          llfio::byte_io_handle &dataSource,
                                          bool streamSelector,
                                          std::uint64_t maxSize) noexcept
            -> result<interleaving_input_stream_handle>
    {
        DPLX_TRY(pages.resize(max_block_size()));

        DPLX_TRY(auto initialReadArea,
                 read_chunk(pages, &dataSource, 0U, streamSelector));
        auto initialUsage
                = std::min<std::uint64_t>(maxSize, index_to_block_size(0U));
        initialReadArea = initialReadArea.first(initialUsage);

        return interleaving_input_stream_handle(dataSource, streamSelector,
                                                maxSize, std::move(pages),
                                                initialReadArea);
    }

    auto
    acquire_next_chunk_impl([[maybe_unused]] std::uint64_t remaining) noexcept
            -> dp::result<dp::memory_view>
    {
        if (mBufferAllocation.size() == 0)
        {
            DPLX_TRY(mBufferAllocation.resize(max_block_size()));
        }
        DPLX_TRY(auto nextReadArea, read_current_chunk());
        mIndexPosition += 1;

        return dp::memory_view{nextReadArea};
    }

    auto read_current_chunk() noexcept -> dp::result<std::span<std::byte const>>
    {
        return read_chunk(mBufferAllocation, mDataSource, mIndexPosition,
                          mStreamSelector);
    }
    static auto read_chunk(page_allocation &bufferAlloc,
                           llfio::byte_io_handle *dataSource,
                           unsigned index,
                           bool streamSelector) noexcept
            -> dp::result<std::span<std::byte const>>
    {
        auto const readPos = index_to_block_offset(index, streamSelector);
        auto const readSpan
                = bufferAlloc.as_span().first(index_to_block_size(index));

        llfio::byte_io_handle::buffer_type ioBuffers[] = {{readSpan}};
        DPLX_TRY(llfio::byte_io_handle::buffers_type const buffers,
                 dataSource->read({ioBuffers, readPos}));
        if (buffers.size() != 1 || buffers[0].size() != readSpan.size())
        {
            return dp::errc::end_of_stream;
        }

        return buffers[0];
    }
};

class interleaving_output_stream_handle final
    : public dp::legacy::chunked_output_stream_base<
              interleaving_output_stream_handle>
    , interleaving_stream_base
{
    friend class dp::legacy::chunked_output_stream_base<
            interleaving_output_stream_handle>;
    using base_t = dp::legacy::chunked_output_stream_base<
            interleaving_output_stream_handle>;

    page_allocation mBufferAllocation;
    llfio::byte_io_handle *mDataSink;
    unsigned mIndexPosition;
    bool mStreamSelector;

public:
    ~interleaving_output_stream_handle() noexcept
    {
        if (mDataSink != nullptr)
        {
            (void)write_current_chunk();
        }
    }
    explicit interleaving_output_stream_handle() noexcept
        : base_t({}, 0U)
        , mBufferAllocation()
        , mDataSink()
        , mIndexPosition()
        , mStreamSelector()
    {
    }
    interleaving_output_stream_handle(
            interleaving_output_stream_handle &&other) noexcept
        : base_t(std::move(other))
        , mBufferAllocation(std::move(other.mBufferAllocation))
        , mDataSink(std::exchange(other.mDataSink, nullptr))
        , mIndexPosition(other.mIndexPosition)
        , mStreamSelector(other.mStreamSelector)
    {
    }
    auto operator=(interleaving_output_stream_handle &&other) noexcept
            -> interleaving_output_stream_handle &
    {
        base_t::operator=(std::move(other));
        mBufferAllocation = std::move(other.mBufferAllocation);
        mDataSink = std::exchange(other.mDataSink, nullptr);
        mIndexPosition = other.mIndexPosition;
        mStreamSelector = other.mStreamSelector;
        return *this;
    }

    explicit interleaving_output_stream_handle(llfio::byte_io_handle &dataSink,
                                               bool streamSelector,
                                               std::uint64_t maxSize
                                               = max_stream_size) noexcept
        : base_t({}, maxSize)
        , mBufferAllocation()
        , mDataSink(&dataSink)
        , mIndexPosition(0U)
        , mStreamSelector(streamSelector)
    {
    }

private:
    explicit interleaving_output_stream_handle(
            llfio::byte_io_handle &dataSink,
            bool streamSelector,
            std::uint64_t maxSize,
            page_allocation &&buffer) noexcept
        : base_t(buffer.as_span().first(std::min<std::uint64_t>(
                         maxSize, index_to_block_size(0))),
                 maxSize
                         - std::min<std::uint64_t>(maxSize,
                                                   index_to_block_size(0)))
        , mBufferAllocation(std::move(buffer))
        , mDataSink(&dataSink)
        , mIndexPosition(0U)
        , mStreamSelector(streamSelector)
    {
    }

public:
    static auto interleaving_output_stream(llfio::byte_io_handle &dataSink,
                                           bool streamSelector,
                                           std::uint64_t maxSize
                                           = max_stream_size) noexcept
            -> result<interleaving_output_stream_handle>
    {
        page_allocation pages{};
        DPLX_TRY(pages.resize(max_block_size()));

        return interleaving_output_stream_handle(dataSink, streamSelector,
                                                 maxSize, std::move(pages));
    }

private:
    auto acquire_next_chunk_impl() noexcept -> dp::result<std::span<std::byte>>
    {
        if (mDataSink == nullptr)
        {
            return dp::errc::bad;
        }
        if (mBufferAllocation.size() > 0)
        {
            DPLX_TRY(write_current_chunk());
            mIndexPosition += 1;
        }
        else
        {
            DPLX_TRY(mBufferAllocation.resize(max_block_size()));
        }

        auto nextWriteArea = mBufferAllocation.as_span().first(
                index_to_block_size(mIndexPosition));

        // zero the write area, because the final chunk is unlikely to be filled
        // but needs to be written
        std::memset(nextWriteArea.data(), 0, nextWriteArea.size());
        return nextWriteArea;
    }

    auto write_current_chunk() noexcept -> dp::result<void>
    {
        auto const writePos
                = index_to_block_offset(mIndexPosition, mStreamSelector);
        auto const writeSpan = mBufferAllocation.as_span().first(
                index_to_block_size(mIndexPosition));

        llfio::byte_io_handle::const_buffer_type ioBuffers[] = {
                {writeSpan.data(), writeSpan.size()}
        };
        DPLX_TRY(mDataSink->write({ioBuffers, writePos}));

        return dp::oc::success();
    }

public:
    auto finalize() noexcept -> result<void>
    {
        if (mDataSink == nullptr)
        {
            return errc::bad;
        }

        DPLX_TRY(write_current_chunk());
        mDataSink = nullptr;
        return oc::success();
    }
};

} // namespace dplx::dlog
