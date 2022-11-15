
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <boost/predef/os.h>

#include <dplx/dp/memory_buffer.hpp>
#include <dplx/dp/stream.hpp>
#include <dplx/dp/streams/chunked_input_stream.hpp>

#include <dplx/dlog/disappointment.hpp>
#include <dplx/dlog/llfio.hpp>

namespace dplx::dlog::detail
{

class os_input_stream_handle
    : public dp::chunked_input_stream_base<os_input_stream_handle>
{
    friend class dp::chunked_input_stream_base<os_input_stream_handle>;

    using base_t = dp::chunked_input_stream_base<os_input_stream_handle>;
    using page_allocation
            = dp::memory_allocation<llfio::utils::page_allocator<std::byte>>;

    page_allocation mBufferAllocation;
    llfio::byte_io_handle *mDataSource;
    std::uint64_t mReadOffset;

public:
    using extent_type = llfio::byte_io_handle::extent_type;
    static constexpr extent_type max_stream_size
            = std::numeric_limits<extent_type>::max();

    explicit os_input_stream_handle() noexcept
        : base_t({}, 0u)
        , mBufferAllocation()
        , mDataSource(nullptr)
        , mReadOffset(0)
    {
    }

    explicit os_input_stream_handle(llfio::byte_io_handle &dataSource,
                                    extent_type maxSize
                                    = max_stream_size) noexcept
        : base_t({}, maxSize)
        , mBufferAllocation()
        , mDataSource(&dataSource)
        , mReadOffset(maxSize)
    {
    }

private:
    explicit os_input_stream_handle(
            llfio::byte_io_handle &dataSource,
            extent_type maxSize,
            page_allocation &&buffer,
            std::span<std::byte const> initialReadArea) noexcept
        : base_t(initialReadArea, maxSize)
        , mBufferAllocation(std::move(buffer))
        , mDataSource(&dataSource)
        , mReadOffset(initialReadArea.size())
    {
    }

    static constexpr unsigned page_size = 1u << 12;
    static constexpr unsigned buffer_size = page_size * 16;

public:
    static auto os_input_stream(llfio::byte_io_handle &dataSource,
                                extent_type maxSize = max_stream_size) noexcept
            -> result<os_input_stream_handle>
    {
        return os_input_stream(page_allocation{}, dataSource, maxSize);
    }

    auto reset(std::uint64_t maxSize = max_stream_size) noexcept -> result<void>
    {
        if (mDataSource == nullptr)
        {
            return oc::success();
        }
        auto pages = std::move(mBufferAllocation);
        auto const dataSource = mDataSource;

        *this = os_input_stream_handle();

        DPLX_TRY(*this,
                 os_input_stream(std::move(pages), *dataSource, maxSize));

        return oc::success();
    }

private:
    static auto os_input_stream(page_allocation &&pages,
                                llfio::byte_io_handle &dataSource,
                                extent_type maxSize) noexcept
            -> result<os_input_stream_handle>
    {
        DPLX_TRY(pages.resize(buffer_size));

        DPLX_TRY(auto initialReadArea, read_chunk(pages, &dataSource, 0u));
        auto const initialUsage = std::min<extent_type>(maxSize, buffer_size);
        initialReadArea = initialReadArea.first(initialUsage);

        return os_input_stream_handle(dataSource, maxSize, std::move(pages),
                                      initialReadArea);
    }

    auto acquire_next_chunk_impl(
            [[maybe_unused]] std::uint64_t const remaining) noexcept
            -> dp::result<dp::memory_view>
    {
        if (mBufferAllocation.size() == 0)
        {
            DPLX_TRY(mBufferAllocation.resize(buffer_size));
        }
        DPLX_TRY(auto const nextReadArea, read_current_chunk());
        mReadOffset += nextReadArea.size();

        return dp::memory_view{nextReadArea};
    }

    auto read_current_chunk() noexcept -> dp::result<std::span<std::byte const>>
    {
        return read_chunk(mBufferAllocation, mDataSource, mReadOffset);
    }
    static auto read_chunk(page_allocation &bufferAlloc,
                           llfio::byte_io_handle *const dataSource,
                           extent_type const readPos) noexcept
            -> dp::result<std::span<std::byte const>>
    {
        constexpr auto offsetMask = extent_type{page_size - 1};
        auto const realReadPos = readPos & ~offsetMask;
        auto const discard = readPos & offsetMask;

        llfio::byte_io_handle::buffer_type ioBuffers[] = {bufferAlloc.as_span()};

        if (auto readRx = dataSource->read({ioBuffers, realReadPos});
            readRx.has_failure())
        {
            auto ec = make_error_code(readRx.error());

#if defined BOOST_OS_WINDOWS_AVAILABLE
            if (ec == std::error_code{38, std::system_category()})
            { // ERROR_HANDLE_EOF
                return dp::errc::end_of_stream;
            }
#endif

            return oc::failure(std::move(ec));
        }
        else if (readRx.bytes_transferred() <= discard)
        {
            return dp::errc::end_of_stream;
        }
        else if (readRx.assume_value().size() == 1u) [[likely]]
        {
            return std::span<std::byte const>{readRx.assume_value()[0]}.subspan(
                    discard);
        }
        else
        {
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
    }
};

} // namespace dplx::dlog::detail
