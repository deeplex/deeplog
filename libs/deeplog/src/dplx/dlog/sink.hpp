
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <concepts>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

#include <fmt/format.h>

#include <dplx/dp.hpp>
#include <dplx/dp/codecs/auto_object.hpp>
#include <dplx/dp/codecs/core.hpp>
#include <dplx/dp/items/skip_item.hpp>
#include <dplx/dp/legacy/memory_buffer.hpp>
#include <dplx/dp/object_def.hpp>

#include <dplx/dlog/attribute_transmorpher.hpp>
#include <dplx/dlog/concepts.hpp>
#include <dplx/dlog/core/log_clock.hpp>
#include <dplx/dlog/core/serialized_messages.hpp>
#include <dplx/dlog/detail/utils.hpp>
#include <dplx/dlog/llfio.hpp>
#include <dplx/dlog/sinks/sink_frontend.hpp>

namespace dplx::dlog
{

struct file_info
{
    log_clock::epoch_info epoch;
    attribute_container attributes;

    static inline constexpr dp::object_def<
            dp::property_def<4U, &file_info::epoch>{},
            dp::property_def<23U, &file_info::attributes>{}>
            layout_descriptor{.version = 0U,
                              .allow_versioned_auto_decoder = true};
};

class file_sink_backend final : public dp::output_buffer
{
    using rotate_fn = std::function<result<void>(llfio::file_handle &)>;

    llfio::file_handle mBackingFile;

    dp::memory_allocation<llfio::utils::page_allocator<std::byte>>
            mBufferAllocation;
    rotate_fn mRotateBackingFile;
    std::size_t mTargetBufferSize{};

public:
    file_sink_backend() noexcept = default;

    static auto file_sink(llfio::file_handle backingFile,
                          unsigned targetBufferSize,
                          rotate_fn &&rotate) noexcept
            -> result<file_sink_backend>
    {
        file_sink_backend self{};
        self.mTargetBufferSize = targetBufferSize;

        DPLX_TRY(self.resize(targetBufferSize));
        self.reset(self.mBufferAllocation.as_span());
        dp::emit_context emitCtx{self};

        DPLX_TRY(self.bulk_write(as_bytes(std::span(magic)).data(),
                                 std::size(magic)));
        file_info info{.epoch = log_clock::epoch(), .attributes = {}};
        DPLX_TRY(dp::encode_object(emitCtx, info));

        DPLX_TRY(dp::emit_array_indefinite(emitCtx));

        if (rotate)
        {
            DPLX_TRY(rotate(backingFile));
        }
        self.mBackingFile = std::move(backingFile);

        DPLX_TRY(auto const maxExtent, self.mBackingFile.maximum_extent());
        // if created
        if (maxExtent == 0U)
        {
            DPLX_TRY(self.sync_output());
        }

        self.mRotateBackingFile = std::move(rotate);

        return self;
    }

    static inline constexpr llfio::file_handle::mode file_mode
            = llfio::file_handle::mode::append;
    static inline constexpr llfio::file_handle::caching file_caching
            = llfio::file_handle::caching::reads;
    static inline constexpr llfio::file_handle::flag file_flags
            = llfio::file_handle::flag::none;

    static inline constexpr std::string_view extension{".dlog"};
    static inline constexpr std::uint8_t magic[16]
            = {0x83, 0x4e, 0x0d, 0x0a, 0xab, 0x7e, 0x7b, 0x64,
               0x6c, 0x6f, 0x67, 0x7d, 0x7e, 0xbb, 0x0a, 0x1a};

    auto finalize() noexcept -> result<void>
    {
        if (!mBackingFile.is_valid())
        {
            return oc::success();
        }
        dp::emit_context ctx{*this};
        DPLX_TRY(dp::emit_break(ctx));
        DPLX_TRY(sync_output());
        DPLX_TRY(mBackingFile.close());

        *this = file_sink_backend();
        return oc::success();
    }

private:
    auto resize(std::size_t requestedSize) noexcept -> dp::result<void>
    {
        return mBufferAllocation.resize(static_cast<unsigned>(requestedSize));
    }
    auto do_grow(size_type requestedSize) noexcept -> dp::result<void> override
    {
        if (size() != mBufferAllocation.size())
        {
            auto const buffer = mBufferAllocation.as_span();
            llfio::file_handle::const_buffer_type writeBuffers[] = {
                    {buffer.data(), buffer.size() - size()}
            };

            DPLX_TRY(mBackingFile.write({writeBuffers, 0U}));
        }
        if (mBufferAllocation.size() < requestedSize)
        {
            DPLX_TRY(resize(requestedSize));
        }
        reset(mBufferAllocation.as_span());
        return dp::oc::success();
    }
    auto do_bulk_write(std::byte const *src, std::size_t srcSize) noexcept
            -> dp::result<void> override
    {
        if (srcSize < mBufferAllocation.size() / 2)
        {
            auto const buffer = mBufferAllocation.as_span();
            llfio::file_handle::const_buffer_type writeBuffers[] = {
                    {buffer.data(), buffer.size() - size()}
            };
            DPLX_TRY(mBackingFile.write({writeBuffers, 0U}));
            reset(buffer);
            std::memcpy(buffer.data(), src, srcSize);
            commit_written(srcSize);
            return dp::oc::success();
        }

        auto const buffer = mBufferAllocation.as_span();
        llfio::file_handle::const_buffer_type writeBuffers[] = {
                {buffer.data(), buffer.size() - size()},
                {          src,                srcSize}
        };

        DPLX_TRY(mBackingFile.write({writeBuffers, 0U}));
        reset(buffer);
        return dp::oc::success();
    }
    auto do_sync_output() noexcept -> dp::result<void> override
    {
        if (size() != mBufferAllocation.size())
        {
            auto const buffer = mBufferAllocation.as_span();
            llfio::file_handle::const_buffer_type writeBuffers[] = {
                    {buffer.data(), buffer.size() - size()}
            };

            DPLX_TRY(mBackingFile.write({writeBuffers, 0U}));
            reset(buffer);
        }
        if (mBufferAllocation.size() != mTargetBufferSize)
        {
            DPLX_TRY(resize(mTargetBufferSize));
        }

        if (mRotateBackingFile)
        {
            DPLX_TRY(mRotateBackingFile(mBackingFile));
        }
        return dp::oc::success();
    }
};

} // namespace dplx::dlog

DPLX_DLOG_DECLARE_CODEC(::dplx::dlog::file_info);
