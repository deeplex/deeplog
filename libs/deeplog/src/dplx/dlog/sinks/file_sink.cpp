
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/sinks/file_sink.hpp"

#include <type_traits>

#include <fmt/format.h>

#include <dplx/dp.hpp>
#include <dplx/dp/codecs/auto_object.hpp>
#include <dplx/dp/codecs/core.hpp>
#include <dplx/dp/items/skip_item.hpp>

#include <dplx/dlog/detail/utils.hpp>

namespace dplx::dlog
{

file_sink_backend::~file_sink_backend()
{
    (void)finalize();
}

auto file_sink_backend::file_sink(llfio::file_handle backingFile,
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

auto file_sink_backend::finalize() noexcept -> result<void>
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

auto file_sink_backend::resize(std::size_t const requestedSize) noexcept
        -> dp::result<void>
{
    reset();
    return mBufferAllocation.resize(static_cast<unsigned>(requestedSize));
}

auto file_sink_backend::do_grow(size_type requestedSize) noexcept
        -> dp::result<void>
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

auto file_sink_backend::do_bulk_write(std::byte const *src,
                                      std::size_t srcSize) noexcept
        -> dp::result<void>
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

auto file_sink_backend::do_sync_output() noexcept -> dp::result<void>
{
    if (size() != mBufferAllocation.size())
    {
        auto const buffer = mBufferAllocation.as_span();
        llfio::file_handle::const_buffer_type writeBuffers[] = {
                {buffer.data(), buffer.size() - size()}
        };

        DPLX_TRY(mBackingFile.write({writeBuffers, 0U}));
    }
    if (mBufferAllocation.size() != mTargetBufferSize)
    {
        DPLX_TRY(resize(mTargetBufferSize));
    }
    reset(mBufferAllocation.as_span());

    if (mRotateBackingFile)
    {
        DPLX_TRY(mRotateBackingFile(mBackingFile));
    }
    return dp::oc::success();
}

template class basic_sink_frontend<file_sink_backend>;

} // namespace dplx::dlog

DPLX_DLOG_DEFINE_AUTO_OBJECT_CODEC(::dplx::dlog::file_info)
