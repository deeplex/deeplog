
// Copyright Henrik Steffen Gaßmann 2021,2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <atomic>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

#include <dplx/cncr/math_supplement.hpp>
#include <dplx/dp.hpp>
#include <dplx/dp/streams/memory_output_stream2.hpp>
#include <dplx/scope_guard.hpp>

#include <dplx/dlog/detail/utils.hpp> // declare_codec
#include <dplx/dlog/disappointment.hpp>
#include <dplx/dlog/llfio.hpp>

namespace dplx::dlog::detail
{

auto hashed_this_thread_id() noexcept -> std::uint32_t;

constexpr auto hash_to_index(std::uint32_t h, std::uint32_t buckets) noexcept
        -> std::uint32_t
{
    // https://lemire.me/blog/2016/06/27/a-fast-alternative-to-the-modulo-reduction/
    return static_cast<std::uint32_t>((static_cast<std::uint64_t>(h) * buckets)
                                      >> (sizeof(std::uint32_t) * CHAR_BIT));
}

} // namespace dplx::dlog::detail

namespace dplx::dlog
{

// layout:
// 4KiB metadata area
//   * magic
//   * mpsc_bus_info

struct mpsc_bus_info_v00
{
    std::uint32_t num_regions;
    std::uint32_t region_size;
};
using mpsc_bus_info = mpsc_bus_info_v00;

struct mpsc_bus_region_ctrl
{
    alignas(std::atomic_ref<std::uint32_t>::required_alignment)
            std::uint32_t read_ptr;
    alignas(std::atomic_ref<std::uint32_t>::required_alignment)
            std::uint32_t alloc_ptr;

    std::uint8_t padding[56]; // NOLINT(cppcoreguidelines-avoid-magic-numbers)
};

class mpsc_bus_handle final
{
    llfio::mapped_file_handle mBackingFile;
    std::uint32_t mNumRegions;
    std::uint32_t mRegionSize;

public:
    ~mpsc_bus_handle()
    {
        if (mBackingFile.is_valid())
        {
            mBackingFile.unlock_file();
        }
    }

    explicit mpsc_bus_handle() noexcept
        : mBackingFile()
        , mNumRegions(0U)
        , mRegionSize(0U)
    {
    }
    mpsc_bus_handle(mpsc_bus_handle &&other) noexcept
        : mBackingFile(std::move(other.mBackingFile))
        , mNumRegions(std::exchange(other.mNumRegions, 0U))
        , mRegionSize(std::exchange(other.mRegionSize, 0U))
    {
    }
    auto operator=(mpsc_bus_handle &&other) noexcept -> mpsc_bus_handle &
    {
        mBackingFile = std::move(other.mBackingFile);
        mNumRegions = std::exchange(other.mNumRegions, 0U);
        mRegionSize = std::exchange(other.mRegionSize, 0U);
        return *this;
    }

private:
    mpsc_bus_handle(llfio::mapped_file_handle &&backingFile,
                    std::uint32_t numRegions,
                    std::uint32_t regionSize) noexcept
        : mBackingFile(std::move(backingFile))
        , mNumRegions(numRegions)
        , mRegionSize(regionSize)
    {
    }

public:
    static auto mpsc_bus(llfio::path_handle const &base,
                         llfio::path_view path,
                         std::uint32_t numRegions,
                         std::uint32_t regionSize) noexcept
            -> result<mpsc_bus_handle>;
    static auto mpsc_bus(llfio::mapped_file_handle &&backingFile,
                         std::uint32_t numRegions,
                         std::uint32_t regionSize) noexcept
            -> result<mpsc_bus_handle>;

    static inline constexpr std::string_view extension{".dmsb"};
    static inline constexpr std::uint8_t magic[16]
            = {0x82, 0x4e, 0x0d, 0x0a, 0xab, 0x7e, 0x7b, 0x64,
               0x6d, 0x73, 0x62, 0x7d, 0x7e, 0xbb, 0x0a, 0x1a};

private:
    using info = mpsc_bus_info;
    using region_ctrl = mpsc_bus_region_ctrl;

    static constexpr std::uint32_t head_area_size = 4 * 1024U;
    static constexpr std::uint32_t region_ctrl_overhead
            = static_cast<std::uint32_t>(sizeof(region_ctrl));

    static constexpr std::uint32_t write_alignment
            = std::atomic_ref<std::uint32_t>::required_alignment;
    static constexpr std::uint32_t block_size = write_alignment;
    static constexpr std::uint32_t message_header_size = block_size;

    static constexpr std::uint32_t message_lock_flag = 0x8000'0000U;
    static constexpr std::uint32_t message_consumed_flag = 0xc000'0000U;
    static constexpr std::uint32_t unused_block_content = 0xfefe'fefeU;

public:
    static constexpr std::uint32_t min_region_size = 4 * 1024U;
    static constexpr std::uint32_t max_message_size = 0x1fff'ffffU;

    using output_stream = dp::memory_output_stream;

    class logger_token
    {
        friend class mpsc_bus_handle;

        std::uint32_t regionId{0U};
        std::uint32_t msgRegion{0U};
        std::uint32_t msgBegin{0U};

    public:
        logger_token() noexcept = default;

    private:
        explicit logger_token(std::uint32_t regionId_) noexcept
            : regionId(regionId_)
        {
        }
    };

    auto create_token() const noexcept -> result<logger_token>
    {
        auto const hashed = detail::hashed_this_thread_id();
        auto const regionId = detail::hash_to_index(hashed, mNumRegions);
        return logger_token(regionId);
    }

    template <typename ConsumeFn>
    auto consume_content(ConsumeFn &&consumeFn) noexcept -> result<void>
    {
        return this->consume_messages(static_cast<ConsumeFn &&>(consumeFn));
    }

    template <typename ConsumeFn>
    auto consume_messages(ConsumeFn &&consumeFn) noexcept -> result<void>
    {
        for (std::uint32_t regionId = 0U; regionId < mNumRegions; ++regionId)
        {
            DPLX_TRY(this->read_region(static_cast<ConsumeFn &&>(consumeFn),
                                       regionId));
        }
        return oc::success();
    }

private:
    template <typename ConsumeFn>
    auto read_region(ConsumeFn &&consumeFn, std::uint32_t regionId) noexcept
            -> result<void>
    {
        auto *const ctx = region(regionId);
        std::span<std::byte> blockData(region_data(regionId),
                                       mRegionSize - region_ctrl_overhead);

        std::atomic_ref<std::uint32_t> const readPtr(ctx->read_ptr);
        std::atomic_ref<std::uint32_t> const allocPtr(ctx->alloc_ptr);

        auto const originalReadPos = readPtr.load(std::memory_order::relaxed);
        auto const allocPos = allocPtr.load(std::memory_order::relaxed);
        if (allocPos == originalReadPos)
        {
            // nothing to see here
            return oc::success();
        }

        auto readPos = originalReadPos;
        scope_guard readUpdateGuard = [&readPos, &originalReadPos, &readPtr]
        {
            if (readPos != originalReadPos)
            {
                readPtr.store(readPos, std::memory_order::release);
            }
        };

        for (;;)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            auto *const ints = reinterpret_cast<std::uint32_t *>(
                    blockData.subspan(readPos).data());
            std::atomic_ref<std::uint32_t> const msgHeadPtr(*ints);

            auto const msgHead = msgHeadPtr.load(std::memory_order::acquire);
            if ((msgHead & message_lock_flag) != 0U)
            {
                break;
            }
            if (readPos + block_size + msgHead
                > mRegionSize - region_ctrl_overhead)
            {
                readPos = std::uint32_t{0U} - block_size;
            }

            std::uint32_t const allocSize
                    = cncr::round_up_p2(msgHead, block_size);

            std::span<std::byte> const rwmsg
                    = blockData.subspan(readPos + block_size, msgHead);
            std::span<std::byte const> const msg{rwmsg};

            (void)(static_cast<ConsumeFn &&>(consumeFn)(msg));

            msgHeadPtr.fetch_or(message_consumed_flag,
                                std::memory_order::release);
            std::memset(rwmsg.data(), static_cast<int>(unused_block_content),
                        allocSize);

            readPos += allocSize + block_size;
            if (readPos == mRegionSize - region_ctrl_overhead)
            {
                readPos = 0U;
            }
        }

        return oc::success();
    }

public:
    auto write(logger_token &logger, std::uint32_t const msgSize) noexcept
            -> pure_result<output_stream>
    {
        if (msgSize > max_message_size) [[unlikely]]
        {
            return errc::not_enough_space;
        }

        auto regionId = logger.regionId;
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
        write_allocation allocation;
        for (;;)
        {
            auto reserveRx = allocate(regionId, msgSize);
            if (reserveRx.has_value()) [[likely]]
            {
                allocation = reserveRx.assume_value();
                break;
            }
            regionId = ++regionId == mNumRegions ? 0U : regionId;
            if (reserveRx.assume_error() != errc::not_enough_space
                || regionId == logger.regionId)
            {
                return static_cast<decltype(reserveRx) &&>(reserveRx)
                        .assume_error();
            }
        }

        logger.msgRegion = regionId;
        logger.msgBegin = allocation.header_position;

        return dp::memory_output_stream{std::span(
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                region_data(regionId) + allocation.payload_position, msgSize)};
    }

    void commit(logger_token &logger)
    {
        auto *const msgSizeLocation
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                = region_data(logger.msgRegion) + logger.msgBegin;

        std::atomic_ref<std::uint32_t> msgSize(
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                *reinterpret_cast<std::uint32_t *>(msgSizeLocation));
        msgSize.fetch_and(max_message_size, std::memory_order::release);
    }

private:
    struct write_allocation
    {
        std::uint32_t header_position;
        std::uint32_t payload_position;
    };

    auto allocate(std::uint32_t const regionId,
                  std::uint32_t const payloadSize) noexcept
            -> pure_result<write_allocation>
    {
        auto *const ctx = region(regionId);
        auto *const regionData = region_data(regionId);
        auto const regionEnd = mRegionSize - region_ctrl_overhead;
        auto const allocSize = cncr::round_up_p2(payloadSize, block_size);

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
        write_allocation allocation;

        std::atomic_ref<std::uint32_t> const sharedReadHand(ctx->read_ptr);
        std::atomic_ref<std::uint32_t> const sharedAllocHand(ctx->alloc_ptr);

        auto const readHand = sharedReadHand.load(std::memory_order::acquire);
        auto allocHand = sharedAllocHand.load(std::memory_order::relaxed);
        for (;;)
        {
            allocation.header_position = allocHand;
            allocation.payload_position = allocHand + message_header_size;

            auto payloadEnd = allocation.payload_position + allocSize;
            auto const canWrap = allocHand >= readHand;
            auto bufferEnd = canWrap ? regionEnd : readHand;
            if (payloadEnd >= bufferEnd) [[unlikely]]
            {
                if (canWrap && payloadEnd == regionEnd && readHand != 0U)
                {
                    payloadEnd = 0U;
                }
                else if (canWrap && allocSize < readHand)
                {
                    allocation.payload_position = 0U;
                    payloadEnd = allocSize;
                }
                else
                {
                    return errc::not_enough_space;
                }
            }

            if (sharedAllocHand.compare_exchange_weak(
                        allocHand, payloadEnd, std::memory_order::relaxed))
            {
                break;
            }
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        auto *const headerPtr = regionData + allocation.header_position;
        std::atomic_ref<std::uint32_t> msgSize(
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                *reinterpret_cast<std::uint32_t *>(headerPtr));
        msgSize.store(payloadSize | message_lock_flag,
                      std::memory_order::relaxed);

        return allocation;
    }

    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    auto region(std::uint32_t which) noexcept -> region_ctrl *
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return std::launder(reinterpret_cast<region_ctrl *>(
                mBackingFile.address() + head_area_size
                + which * static_cast<std::size_t>(mRegionSize)));
    }
    auto region_data(std::uint32_t which) noexcept -> std::byte *
    {
        return mBackingFile.address() + head_area_size
             + which * static_cast<std::size_t>(mRegionSize)
             + region_ctrl_overhead;
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
};

inline auto mpsc_bus(llfio::path_handle const &base,
                     llfio::path_view const path,
                     std::uint32_t const numRegions,
                     std::uint32_t const regionSize) noexcept
        -> result<mpsc_bus_handle>
{
    return mpsc_bus_handle::mpsc_bus(base, path, numRegions, regionSize);
}
inline auto mpsc_bus(llfio::mapped_file_handle &&backingFile,
                     std::uint32_t numRegions,
                     std::uint32_t regionSize) noexcept
        -> result<mpsc_bus_handle>
{
    return mpsc_bus_handle::mpsc_bus(std::move(backingFile), numRegions,
                                     regionSize);
}

} // namespace dplx::dlog

DPLX_DLOG_DECLARE_CODEC(dplx::dlog::mpsc_bus_info_v00);
