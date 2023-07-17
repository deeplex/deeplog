
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
#include <dplx/dp/macros.hpp>
#include <dplx/dp/streams/memory_output_stream.hpp>
#include <dplx/scope_guard.hpp>

#include <dplx/dlog/concepts.hpp>
#include <dplx/dlog/core/file_database.hpp>
#include <dplx/dlog/core/strong_types.hpp>
#include <dplx/dlog/disappointment.hpp>
#include <dplx/dlog/llfio.hpp>
#include <dplx/dlog/log_fabric.hpp>
#include <dplx/dlog/source/record_output_buffer.hpp>

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
    alignas(std::atomic_ref<std::uint64_t>::required_alignment) std::uint64_t
            span_prng_ctr;

    std::uint8_t padding[48]; // NOLINT(cppcoreguidelines-avoid-magic-numbers)
};

class mpsc_bus_handle
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
                         std::uint32_t regionSize,
                         llfio::lock_kind lockState
                         = llfio::lock_kind::unlocked) noexcept
            -> result<mpsc_bus_handle>;

    static inline constexpr llfio::file_handle::mode file_mode
            = llfio::file_handle::mode::write;
    static inline constexpr llfio::file_handle::caching file_caching
            = llfio::file_handle::caching::temporary;
    static inline constexpr llfio::file_handle::flag file_flags
            = llfio::file_handle::flag::none;

    [[nodiscard]] auto release() noexcept -> llfio::mapped_file_handle
    {
        mNumRegions = 0U;
        mRegionSize = 0U;
        return std::move(mBackingFile);
    }
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    auto unlink(llfio::deadline deadline = std::chrono::seconds(30)) noexcept
            -> result<void>
    {
        DPLX_TRY(mBackingFile.unlink(deadline));
        (void)release();
        return oc::success();
    }

    static inline constexpr std::string_view extension{".dmpscb"};
    static inline constexpr std::uint8_t magic[18]
            = {0x82, 0x50, 0x0d, 0x0a, 0xab, 0x7e, 0x7b, 0x64, 0x6D,
               0x70, 0x73, 0x63, 0x62, 0x7d, 0x7e, 0xbb, 0x0a, 0x1a};

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

    static constexpr unsigned consume_batch_size = 64U;

    auto create_span_context(trace_id trace,
                             std::string_view,
                             severity &) noexcept -> span_context;

    class output_buffer final : public record_output_buffer
    {
        friend class mpsc_bus_handle;

        std::uint32_t *mMsgCtrl{nullptr};

        using record_output_buffer::record_output_buffer;

        auto do_sync_output() noexcept -> dp::result<void> final
        {
            if (mMsgCtrl == nullptr) [[unlikely]]
            {
                return errc::bad;
            }

            std::atomic_ref<std::uint32_t>(*mMsgCtrl).fetch_and(
                    max_message_size, std::memory_order::release);
            // reset();
            mMsgCtrl = nullptr;
            return dp::oc::success();
        }
    };

    template <typename ConsumeFn>
        requires raw_message_consumer<ConsumeFn &&>
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
        auto *const blockData = region_data(regionId);
        writable_bytes block(blockData, mRegionSize - region_ctrl_overhead);

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
            std::size_t batchSize = 0U;
            struct
            {
                std::uint32_t *head{};
                writable_bytes content;
            } infos[consume_batch_size] = {};
            bytes msgs[consume_batch_size];

            // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)
            for (; batchSize < consume_batch_size; ++batchSize)
            {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                auto *const msgHeadPtr = reinterpret_cast<std::uint32_t *>(
                        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                        blockData + readPos);

                auto const msgHead
                        = std::atomic_ref<std::uint32_t>{*msgHeadPtr}.load(
                                std::memory_order::acquire);
                if ((msgHead & message_lock_flag) != 0U)
                {
                    break;
                }
                if (readPos + block_size + msgHead
                    > mRegionSize - region_ctrl_overhead)
                {
                    readPos = std::uint32_t{0U} - block_size;
                }

                auto const allocSize = cncr::round_up_p2(msgHead, block_size);
                infos[batchSize] = {
                        .head = msgHeadPtr,
                        .content
                        = block.subspan(readPos + block_size, allocSize),
                };
                msgs[batchSize] = block.subspan(readPos + block_size, msgHead);

                readPos += allocSize + block_size;
                if (readPos == mRegionSize - region_ctrl_overhead)
                {
                    readPos = 0U;
                }
            }
            if (batchSize == 0U)
            {
                break;
            }

            (void)(static_cast<ConsumeFn &&>(consumeFn)(
                    std::span(static_cast<bytes const *>(msgs), batchSize)));

            for (std::size_t i = 0U; i < batchSize; ++i)
            {
                std::atomic_ref<std::uint32_t>{*infos[i].head}.fetch_or(
                        message_consumed_flag, std::memory_order::relaxed);
                std::memset(infos[i].content.data(),
                            static_cast<int>(unused_block_content),
                            infos[i].content.size());
            }
            // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
        }

        return oc::success();
    }

public:
    auto allocate_record_buffer_inplace(
            record_output_buffer_storage &bufferPlacementStorage,
            std::size_t messageSize,
            span_id spanId) noexcept -> result<record_output_buffer *>
    {
        static_assert(sizeof(record_output_buffer_storage)
                      >= sizeof(output_buffer));

        if (messageSize > max_message_size) [[unlikely]]
        {
            return errc::not_enough_space;
        }
        auto const payloadSize = static_cast<std::uint32_t>(messageSize);

        output_buffer out;
        auto const spread
                = (spanId == span_id::invalid()
                           ? detail::hashed_this_thread_id()
                           : static_cast<std::uint32_t>(spanId._state[0]));
        auto const firstRegionId = detail::hash_to_index(spread, mNumRegions);
        auto regionId = firstRegionId;
        for (;;)
        {
            auto const allocCode = allocate(out, payloadSize, regionId);
            if (allocCode == errc::success) [[likely]]
            {
                break;
            }
            regionId = ++regionId == mNumRegions ? 0U : regionId;
            if (allocCode != errc::not_enough_space
                || regionId == firstRegionId)
            {
                return cncr::data_defined_status_code<errc>{allocCode};
            }
        }

        return new (static_cast<void *>(&bufferPlacementStorage))
                output_buffer(out);
    }

private:
    auto allocate(output_buffer &out,
                  std::uint32_t const payloadSize,
                  std::uint32_t const regionId) noexcept -> errc
    {
        auto *const ctx = region(regionId);
        auto *const regionData = region_data(regionId);
        auto const regionEnd = mRegionSize - region_ctrl_overhead;
        auto const allocSize = cncr::round_up_p2(payloadSize, block_size);

        std::atomic_ref<std::uint32_t> const sharedReadHand(ctx->read_ptr);
        std::atomic_ref<std::uint32_t> const sharedAllocHand(ctx->alloc_ptr);

        auto const readHand = sharedReadHand.load(std::memory_order::acquire);
        std::uint32_t allocHand
                = sharedAllocHand.load(std::memory_order::relaxed);
        // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
        std::uint32_t payloadPosition;
        for (;;)
        {
            payloadPosition = allocHand + message_header_size;

            auto payloadEnd = payloadPosition + allocSize;
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
                    payloadPosition = 0U;
                    payloadEnd = allocSize;
                }
                else
                {
                    return errc::not_enough_space;
                }
            }

            if (sharedAllocHand.compare_exchange_weak(
                        allocHand, payloadEnd, std::memory_order::relaxed))
                    [[likely]]
            {
                break;
            }
        }

        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto *ctrl = reinterpret_cast<std::uint32_t *>(regionData + allocHand);
        auto *data = regionData + payloadPosition;
        // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        std::atomic_ref<std::uint32_t>(*ctrl).store(
                payloadSize | message_lock_flag, std::memory_order::relaxed);
        out.reset(data, allocSize);
        out.mMsgCtrl = ctrl;
        return errc::success;
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

class db_mpsc_bus_handle : private mpsc_bus_handle
{
    file_database_handle mFileDb;
    std::string mId;
    std::uint32_t mRotation;

public:
    db_mpsc_bus_handle()
        : mpsc_bus_handle()
        , mFileDb()
        , mId()
        , mRotation(0U)
    {
    }

    db_mpsc_bus_handle(db_mpsc_bus_handle &&other) noexcept
        : mpsc_bus_handle(std::move(other))
        , mFileDb(std::move(other).mFileDb)
        , mId(std::move(other.mId))
        , mRotation(std::exchange(other.mRotation, 0U))
    {
    }
    auto operator=(db_mpsc_bus_handle &&other) noexcept -> db_mpsc_bus_handle &
    {
        mpsc_bus_handle::operator=(std::move(other));

        // the base class operator only moves base class parts
        // NOLINTBEGIN(bugprone-use-after-move)
        mFileDb = std::move(other.mFileDb);
        mId = std::move(other.mId);
        mRotation = std::exchange(other.mRotation, 0U);
        // NOLINTEND(bugprone-use-after-move)
        return *this;
    }

private:
    db_mpsc_bus_handle(mpsc_bus_handle &&h,
        file_database_handle &&fileDb,
                       std::string id,
                       std::uint32_t rotation)
        : mpsc_bus_handle(std::move(h))
        , mFileDb(std::move(fileDb))
        , mId(std::move(id))
        , mRotation(rotation)
    {
    }

public:
    struct config_type
    {
        file_database_handle const &database;
        std::string bus_id;
        std::string_view file_name_pattern;
        std::uint32_t num_regions;
        std::uint32_t region_size;
    };

    static auto create(config_type &&config) -> result<db_mpsc_bus_handle>;

    using mpsc_bus_handle::consume_batch_size;
    using mpsc_bus_handle::max_message_size;
    using mpsc_bus_handle::min_region_size;

    using mpsc_bus_handle::allocate_record_buffer_inplace;
    using mpsc_bus_handle::consume_messages;
    using mpsc_bus_handle::create_span_context;

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    auto unlink(llfio::deadline deadline = std::chrono::seconds(30)) noexcept
            -> result<void>;
};

extern template class log_fabric<db_mpsc_bus_handle>;

} // namespace dplx::dlog

DPLX_DP_DECLARE_CODEC_SIMPLE(dplx::dlog::mpsc_bus_info_v00);
