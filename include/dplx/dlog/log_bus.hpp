
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <atomic>
#include <bit>
#include <concepts>
#include <type_traits>

#include <dplx/dp/item_emitter.hpp>
#include <dplx/dp/item_parser.hpp>
#include <dplx/dp/memory_buffer.hpp>
#include <dplx/dp/streams/memory_input_stream.hpp>
#include <dplx/dp/streams/memory_output_stream.hpp>

#include <dplx/dlog/concepts.hpp>
#include <dplx/dlog/detail/utils.hpp>
#include <dplx/dlog/disappointment.hpp>
#include <dplx/dlog/llfio.hpp>

namespace dplx::dlog
{

class bufferbus_handle
{
    llfio::mapped_file_handle mBackingFile;
    dp::memory_buffer mBuffer;

public:
    explicit bufferbus_handle() noexcept = default;

    bufferbus_handle(bufferbus_handle const &) = delete;
    auto operator=(bufferbus_handle const &) -> bufferbus_handle & = delete;

    bufferbus_handle(bufferbus_handle &&other) noexcept
        : mBackingFile(std::move(other.mBackingFile))
        , mBuffer(std::exchange(other.mBuffer, dp::memory_buffer{}))
    {
    }
    auto operator=(bufferbus_handle &&other) noexcept -> bufferbus_handle &
    {
        mBackingFile = std::exchange(other.mBackingFile,
                                     llfio::mapped_file_handle{});
        mBuffer = std::exchange(other.mBuffer, dp::memory_buffer{});

        return *this;
    }

    explicit bufferbus_handle(llfio::mapped_file_handle buffer,
                              unsigned int bufferCap)
        : mBackingFile(std::move(buffer))
        , mBuffer(mBackingFile.address(), static_cast<int>(bufferCap), 0)
    {
    }

    using output_stream = dp::memory_buffer;

    struct logger_token
    {
    };
    static auto create_token() noexcept -> result<logger_token>
    {
        return logger_token{};
    }

    static auto bufferbus(llfio::path_handle const &base,
                          llfio::path_view path,
                          unsigned int bufferSize) noexcept
            -> result<bufferbus_handle>
    {
        DPLX_TRY(
                auto &&backingFile,
                llfio::mapped_file(base, path, llfio::handle::mode::write,
                                   llfio::handle::creation::only_if_not_exist));

        return bufferbus(std::move(backingFile), bufferSize);
    }
    static auto bufferbus(llfio::mapped_file_handle &&backingFile,
                          unsigned int bufferSize) noexcept
            -> result<bufferbus_handle>
    {
        DPLX_TRY(auto truncatedSize, backingFile.truncate(bufferSize));
        if (truncatedSize != bufferSize)
        {
            return errc::bad;
        }

        // TODO: signal handler
        std::memset(backingFile.address(),
                    static_cast<int>(dp::type_code::null), bufferSize);

        return bufferbus_handle(std::move(backingFile), bufferSize);
    }

    auto write(logger_token &, unsigned int msgSize) noexcept
            -> result<output_stream>
    {
        auto const overhead = dp::detail::var_uint_encoded_size(msgSize);
        auto const totalSize = overhead + msgSize;

        if (totalSize > static_cast<unsigned>(mBuffer.remaining_size()))
        {
            return errc::not_enough_space;
        }

        auto const bufferStart = mBuffer.consume(totalSize);
        auto const msgStart
                = bufferStart
                + dp::detail::store_var_uint(
                          bufferStart, msgSize,
                          static_cast<std::byte>(dp::type_code::binary));

        return dp::memory_buffer(msgStart, static_cast<int>(msgSize), 0);
    }

    void commit(logger_token &)
    {
    }

    template <typename ConsumeFn>
        requires bus_consumer<ConsumeFn &&>
    auto consume_content(ConsumeFn &&consume) noexcept -> result<void>
    {
        dp::memory_view content(mBuffer.consumed_begin(),
                                mBuffer.consumed_size(), 0);

        while (content.remaining_size() > 0)
        {
            DPLX_TRY(auto msgInfo, dp::detail::parse_item(content));
            if (msgInfo.type != dp::type_code::binary || msgInfo.indefinite()
                || content.remaining_size() < msgInfo.value)
                DPLX_ATTR_UNLIKELY
                {
                    // we ignore bad blocks
                    // TODO: maybe do something more sensible
                    // OTOH the log buffer is corrupted, so...
                    break;
                }

            std::span<std::byte const> const msg(
                    content.consume(static_cast<int>(msgInfo.value)),
                    static_cast<std::size_t>(msgInfo.value));

            static_cast<ConsumeFn &&>(consume)(msg);
        }

        return clear_content();
    }
    auto clear_content() -> result<void>
    {
        // TODO: signal handler (?)
        std::memset(mBuffer.consumed_begin(),
                    static_cast<int>(dp::type_code::null),
                    mBuffer.consumed_size());

        mBuffer.move_consumer_to(0);
        return oc::success();
    }

    auto release() -> llfio::mapped_file_handle
    {
        mBuffer = dp::memory_buffer{};
        return std::move(mBackingFile);
    }
    auto unlink(llfio::deadline deadline = std::chrono::seconds(30)) noexcept
            -> result<void>
    {
        DPLX_TRY(mBackingFile.unlink(deadline));
        (void)release();
        return oc::success();
    }

private:
};
static_assert(bus<bufferbus_handle>);

inline auto bufferbus(llfio::path_handle const &base,
                      llfio::path_view path,
                      unsigned int bufferSize) noexcept
        -> result<bufferbus_handle>
{
    return bufferbus_handle::bufferbus(base, std::move(path), bufferSize);
}
inline auto bufferbus(llfio::mapped_file_handle &&backingFile,
                      unsigned int bufferSize) noexcept
        -> result<bufferbus_handle>
{
    return bufferbus_handle::bufferbus(std::move(backingFile), bufferSize);
}

class ringbus_mt_handle
{
    // n := block size
    // layout: 1*n ctrl block + 2*(x*n) buffer cap
    llfio::mapped_file_handle mBackingFile;
    unsigned int mNumRegions;

public:
    ~ringbus_mt_handle() noexcept
    {
        if (mBackingFile.is_valid())
        {
            (void)unlink();
        }
    }
    ringbus_mt_handle() noexcept = default;

    ringbus_mt_handle(ringbus_mt_handle const &) = delete;
    auto operator=(ringbus_mt_handle const &) -> ringbus_mt_handle & = delete;

    ringbus_mt_handle(ringbus_mt_handle &&other) noexcept
        : mBackingFile(std::move(other.mBackingFile))
        , mNumRegions(std::exchange(other.mNumRegions, 0))
    {
    }
    auto operator=(ringbus_mt_handle &&other) noexcept -> ringbus_mt_handle &
    {
        mBackingFile = std::exchange(other.mBackingFile,
                                     llfio::mapped_file_handle{});
        mNumRegions = std::exchange(other.mNumRegions, 0);

        return *this;
    }

    ringbus_mt_handle(llfio::mapped_file_handle backingFile,
                      unsigned int numRegions)
        : mBackingFile(std::move(backingFile))
        , mNumRegions(numRegions)
    {
    }

private:
    static constexpr unsigned int block_size = 64u;
    using bucket_type = std::uint64_t;
    static constexpr int blocks_per_bucket
            = std::numeric_limits<bucket_type>::digits;

    static constexpr unsigned int region_ctrl_overhead = 512u;
    static constexpr unsigned int region_alloc_buckets = 60u;

    static constexpr unsigned int alloc_map_size
            = region_alloc_buckets * sizeof(bucket_type);
    static constexpr unsigned int blocks_per_region
            = region_alloc_buckets * blocks_per_bucket;
    static constexpr unsigned int region_size
            = region_ctrl_overhead + blocks_per_region * block_size;

    struct region_ctrl;

public:
    static auto
    ringbus(llfio::path_handle const &base,
            llfio::path_view path,
            unsigned int const numBlocksPerWriter,
            unsigned int const expectedWriters
            = std::min(16u, std::thread::hardware_concurrency())) noexcept
            -> result<ringbus_mt_handle>
    {
        DPLX_TRY(
                auto &&backingFile,
                llfio::mapped_file(base, path, llfio::handle::mode::write,
                                   llfio::handle::creation::only_if_not_exist));

        return ringbus(std::move(backingFile), numBlocksPerWriter,
                       expectedWriters);
    }
    static auto
    ringbus(llfio::mapped_file_handle &&backingFile,
            unsigned int const numBlocksPerWriter,
            unsigned int const expectedWriters
            = std::min(16u, std::thread::hardware_concurrency())) noexcept
            -> result<ringbus_mt_handle>
    {
        auto const regionsPerWriter
                = detail::div_ceil(numBlocksPerWriter, blocks_per_region);
        auto const numRegions = expectedWriters * regionsPerWriter;
        auto const bufferSize = numRegions * region_size;

        DPLX_TRY(auto truncatedSize, backingFile.truncate(bufferSize));
        if (truncatedSize != bufferSize)
        {
            return errc::bad;
        }

        // TODO: signal handler
        for (auto begin = backingFile.address(),
                  end = backingFile.address() + bufferSize;
             begin != end; begin += region_size)

        {
            ::new (static_cast<void *>(begin))
                    region_ctrl{0, 0, blocks_per_region, {}, {}};

            std::memset(begin + region_ctrl_overhead,
                        0xfe, // invalid cbor item coding
                        region_size - region_ctrl_overhead);
        }

        return ringbus_mt_handle(std::move(backingFile), numRegions);
    }
    static auto ringbus(llfio::path_handle const &base,
                        llfio::path_view path) noexcept
            -> result<ringbus_mt_handle>
    {
        DPLX_TRY(auto backingFile,
                 llfio::mapped_file(base, path, llfio::handle::mode::write,
                                    llfio::handle::creation::open_existing));

        return ringbus(std::move(backingFile));
    }
    static auto ringbus(llfio::mapped_file_handle &&existing) noexcept
            -> result<ringbus_mt_handle>
    {
        if (!existing.is_readable() || !existing.is_writable())
        {
            return errc::bad;
        }

        DPLX_TRY(auto maxExtent, existing.maximum_extent());

        if (maxExtent % region_size != 0)
        {
            return errc::bad;
        }
        auto numRegions = static_cast<unsigned int>(maxExtent / region_size);

        return ringbus_mt_handle(std::move(existing), numRegions);
    }

    using output_stream = dp::memory_buffer;

    struct logger_token
    {
        unsigned int regionId;
        unsigned int msgRegion;
        unsigned int msgBegin;
        unsigned int msgBlocks;
    };
    auto create_token() noexcept -> result<logger_token>
    {
        auto const threadId = std::this_thread::get_id();
        auto const hashed = std::hash<std::thread::id>()(threadId);
        auto const distributed
                = static_cast<unsigned int>(hashed % mNumRegions);
        return logger_token{.regionId = distributed,
                            .msgRegion = 0,
                            .msgBegin = 0,
                            .msgBlocks = 0};
    }

    auto release() -> llfio::mapped_file_handle
    {
        mNumRegions = 0;
        return std::move(mBackingFile);
    }
    auto unlink(llfio::deadline deadline = std::chrono::seconds(30)) noexcept
            -> result<void>
    {
        DPLX_TRY(mBackingFile.unlink(deadline));
        (void)release();
        return oc::success();
    }

    template <typename ConsumeFn>
    auto consume_content(ConsumeFn &&consumeFn) noexcept -> result<void>
    {
        for (unsigned regionId = 0; regionId < mNumRegions; ++regionId)
        {
            DPLX_TRY(dequeue(static_cast<ConsumeFn &&>(consumeFn), regionId));
        }
        return oc::success();
    }

private:
    template <typename ConsumeFn>
    auto dequeue(ConsumeFn &&consumeFn, unsigned int regionId) noexcept
            -> result<void>
    {
        auto *const ctx = region(regionId);
        auto *const firstBlock = block(regionId, 0);

        std::atomic_ref<std::uint32_t> const readPtr(ctx->read_ptr);
        std::atomic_ref<std::uint32_t> const allocPtr(ctx->alloc_ptr);
        std::atomic_ref<std::uint32_t> const endPtr(ctx->end_ptr);

        auto const readPos = readPtr.load(std::memory_order::relaxed);
        auto const allocPos = allocPtr.load(std::memory_order::relaxed);
        if (allocPos == readPos)
        {
            // nothing to see here
            return oc::success();
        }

        auto numAvailable = pop_available_blocks(ctx, readPos);
        auto readEnd = readPos + numAvailable;
        if (numAvailable != 0)
        {
            auto blockIt = readPos;
            while (blockIt < readEnd)
            {
                auto blockStart = firstBlock + blockIt * block_size;

                DPLX_TRY(dp::item_info const msgInfo,
                         dp::detail::parse_item_speculative(blockStart));

                if (auto const space = (readEnd - blockIt) * block_size;
                    msgInfo.type != dp::type_code::binary
                    || msgInfo.indefinite()
                    || space < msgInfo.encoded_length + msgInfo.value)
                    DPLX_ATTR_UNLIKELY
                    {
                        // we ignore bad blocks
                        blockIt += 1;
                        continue;
                    }

                blockIt += detail::div_ceil(
                        static_cast<std::uint32_t>(msgInfo.encoded_length
                                                   + msgInfo.value),
                        block_size);

                std::span<std::byte const> const msg{
                        blockStart + msgInfo.encoded_length,
                        static_cast<std::size_t>(msgInfo.value)};

                static_cast<ConsumeFn &&>(consumeFn)(msg);
            }
        }

        if (endPtr.load(std::memory_order::relaxed) != readEnd)
        {
            readPtr.store(readEnd, std::memory_order::release);
            return oc::success();
        }
        else
        {
            // wrap around
            endPtr.store(blocks_per_region, std::memory_order::relaxed);
            readPtr.store(0, std::memory_order::release);
            return dequeue<ConsumeFn>(static_cast<ConsumeFn &&>(consumeFn),
                                      regionId);
        }
    }

    auto pop_available_blocks(region_ctrl *ctx, unsigned int readPos) -> int
    {
        auto bucketIndex = readPos / blocks_per_bucket;
        auto subBucketIndex = readPos % blocks_per_bucket;
        auto numAvailable = 0;

        do
        {
            std::atomic_ref bucketRef(ctx->alloc_map[bucketIndex]);

            // ready blocks are one bits => shifting in zeroes is safe
            auto const mapValue = bucketRef.load(std::memory_order::acquire)
                               >> subBucketIndex;
            if (mapValue == 0)
            {
                break;
            }

            auto const numAvailableLocal = std::countr_one(mapValue);
            numAvailable += numAvailableLocal;

            // use xor to flip the bits to zero
            bucketRef.fetch_xor(detail::make_mask<bucket_type>(
                                        numAvailableLocal, subBucketIndex),
                                std::memory_order::relaxed);

            subBucketIndex += numAvailableLocal;
            subBucketIndex %= blocks_per_bucket;
            if (0 != subBucketIndex)
            {
                break;
            }
            bucketIndex += 1;
        } while (bucketIndex < region_alloc_buckets);

        return numAvailable;
    }

public:
    auto write(logger_token &logger, unsigned int msgSize) noexcept
            -> result<output_stream>
    {
        if (msgSize > block_size * blocks_per_bucket - 3)
        {
            return errc::not_enough_space;
        }

        auto const overhead = dp::detail::var_uint_encoded_size(msgSize);
        auto const totalSize = overhead + msgSize;
        auto const numBlocks = detail::div_ceil(totalSize, block_size);

        unsigned int regionId = logger.regionId;
        region_ctrl *ctx;
        unsigned int msgStart = std::numeric_limits<unsigned>::max();
        do
        {
            ctx = region(regionId);
            if (auto reserveRx = write_to(ctx, numBlocks))
                DPLX_ATTR_LIKELY
                {
                    msgStart = reserveRx.assume_value();
                }
            else if (reserveRx.assume_error() == errc::not_enough_space)
            {
                regionId += 1;
                if (regionId == mNumRegions)
                {
                    regionId = 0;
                }
                if (regionId == logger.regionId)
                {
                    return std::move(reserveRx).assume_error();
                }
            }
            else
            {
                return std::move(reserveRx).as_failure();
            }
        } while (msgStart == std::numeric_limits<unsigned>::max());

        logger.msgRegion = regionId;
        logger.msgBegin = msgStart;
        logger.msgBlocks = numBlocks;

        auto bufferStart = region_data(regionId) + msgStart * block_size;
        auto encodedSize = dp::detail::store_var_uint(
                bufferStart, msgSize,
                static_cast<std::byte>(dp::type_code::binary));

        return dp::memory_buffer{bufferStart + encodedSize, msgSize, 0};
    }

    void commit(logger_token &logger)
    {
        auto const ctx = region(logger.msgRegion);

        auto firstBucket = logger.msgBegin / blocks_per_bucket;
        auto offset = logger.msgBegin % blocks_per_bucket;

        auto numFirst = std::min(logger.msgBlocks, blocks_per_bucket - offset);

        if (numFirst != logger.msgBlocks)
        {
            std::atomic_ref<bucket_type> const secondBucketRef(
                    ctx->alloc_map[firstBucket + 1]);

            auto const numSecond = logger.msgBlocks - numFirst;
            secondBucketRef.fetch_or(detail::make_mask<bucket_type>(numSecond),
                                     std::memory_order::relaxed);
        }

        std::atomic_ref<bucket_type> const firstBucketRef(
                ctx->alloc_map[firstBucket]);
        firstBucketRef.fetch_or(
                detail::make_mask<bucket_type>(numFirst, offset),
                std::memory_order::release);
    }

private:
    auto write_to(region_ctrl *ctx, unsigned int numBlocks) noexcept
            -> result<unsigned int>
    {
        std::atomic_ref<std::uint32_t> const readPtr(ctx->read_ptr);
        std::atomic_ref<std::uint32_t> const allocPtr(ctx->alloc_ptr);
        std::atomic_ref<std::uint32_t> const endPtr(ctx->end_ptr);

        auto readPos = readPtr.load(std::memory_order::acquire);
        auto writePos = allocPtr.load(std::memory_order::relaxed);
        for (;;)
        {
            auto bufferEnd = writePos < readPos ? readPos : blocks_per_region;

            auto endBlock = writePos + numBlocks;
            if (endBlock <= bufferEnd)
            {
                if (allocPtr.compare_exchange_weak(writePos, endBlock,
                                                   std::memory_order::relaxed))
                {
                    return writePos;
                }
                // else continue
            }
            else if (writePos < readPos
                     || (writePos >= readPos && readPos < numBlocks))
            {
                // not enough buffer space for our message. Too bad!
                return errc::not_enough_space;
            }
            else if (auto endPos = writePos; allocPtr.compare_exchange_weak(
                             writePos, numBlocks, std::memory_order::relaxed))
            {
                // we did the wrap around => update watermark
                endPtr.store(endPos, std::memory_order::relaxed);
                return 0u;
            }
        }
    }

    struct region_ctrl
    {
        alignas(std::atomic_ref<std::uint32_t>::required_alignment)
                std::uint32_t read_ptr;
        alignas(std::atomic_ref<std::uint32_t>::required_alignment)
                std::uint32_t alloc_ptr;
        alignas(std::atomic_ref<std::uint32_t>::required_alignment)
                std::uint32_t end_ptr;

        std::uint32_t padding[5];

        alignas(std::atomic_ref<bucket_type>::required_alignment)
                bucket_type alloc_map[region_alloc_buckets];
    };
    static_assert(sizeof(region_ctrl) == region_ctrl_overhead);
    static_assert(std::is_trivial_v<region_ctrl>);
    static_assert(std::is_standard_layout_v<region_ctrl>);

    auto region(unsigned which) noexcept -> region_ctrl *
    {
        return std::launder(reinterpret_cast<region_ctrl *>(
                mBackingFile.address() + which * region_size));
    }
    auto region_data(unsigned which) noexcept -> std::byte *
    {
        return mBackingFile.address() + which * region_size
             + region_ctrl_overhead;
    }

    auto block(unsigned int regionId, unsigned int blockId) -> std::byte const *
    {
        return mBackingFile.address() + regionId * region_size
             + region_ctrl_overhead + blockId * block_size;
    }
};
static_assert(bus<ringbus_mt_handle>);

inline auto ringbus(llfio::path_handle const &base,
                    llfio::path_view path,
                    unsigned int const numBlocksPerWriter,
                    unsigned int const expectedWriters
                    = std::min(16u,
                               std::thread::hardware_concurrency())) noexcept
        -> result<ringbus_mt_handle>
{
    return ringbus_mt_handle::ringbus(base, std::move(path), numBlocksPerWriter,
                                      expectedWriters);
}
inline auto ringbus(llfio::mapped_file_handle &&backingFile,
                    unsigned int const numBlocksPerWriter,
                    unsigned int const expectedWriters
                    = std::min(16u,
                               std::thread::hardware_concurrency())) noexcept
        -> result<ringbus_mt_handle>
{
    return ringbus_mt_handle::ringbus(std::move(backingFile),
                                      numBlocksPerWriter, expectedWriters);
}
inline auto ringbus(llfio::path_handle const &base,
                    llfio::path_view path) noexcept -> result<ringbus_mt_handle>
{
    return ringbus_mt_handle::ringbus(base, std::move(path));
}
inline auto ringbus(llfio::mapped_file_handle &&existing) noexcept
        -> result<ringbus_mt_handle>
{
    return ringbus_mt_handle::ringbus(std::move(existing));
}

template <bus Bus>
struct bus_write_lock
{
    using bus_type = Bus;
    using token_type = typename bus_type::logger_token;

    bus_type &bus;
    token_type &token;

    BOOST_FORCEINLINE explicit bus_write_lock(bus_type &bus, token_type &token)
        : bus(bus)
        , token(token)
    {
    }
    BOOST_FORCEINLINE ~bus_write_lock()
    {
        bus.commit(token);
    }
};
} // namespace dplx::dlog
