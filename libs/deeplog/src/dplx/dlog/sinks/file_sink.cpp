
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

// NOLINTNEXTLINE(performance-noexcept-move-constructor)
auto file_sink_backend::operator=(file_sink_backend &&other)
        -> file_sink_backend &
{
    finalize().value();

    output_buffer::operator=(std::move(other));
    // NOLINTBEGIN(bugprone-use-after-move)
    mBackingFile = std::move(other.mBackingFile);
    mBufferAllocation = std::move(other.mBufferAllocation);
    mTargetBufferSize = std::exchange(other.mTargetBufferSize, 0U);
    // NOLINTEND(bugprone-use-after-move)
    return *this;
}

file_sink_backend::file_sink_backend(std::size_t targetBufferSize) noexcept
    : mBackingFile{}
    , mBufferAllocation{}
    , mTargetBufferSize{targetBufferSize}
{
}

auto file_sink_backend::initialize() noexcept -> result<void>
{
    DPLX_TRY(resize(mTargetBufferSize));
    DPLX_TRY(rotate());
    return outcome::success();
}

auto file_sink_backend::create(config_type &&config) noexcept
        -> result<file_sink_backend>
{
    file_sink_backend self{config.target_buffer_size};
    self.mBackingFile = std::move(config.file);
    llfio::unique_file_lock fileLock(self.mBackingFile,
                                     llfio::lock_kind::unlocked);
    DPLX_TRY(fileLock.lock());
    DPLX_TRY(self.initialize());
    fileLock.release();
    return self;
}

auto file_sink_backend::finalize() noexcept -> result<std::uint32_t>
{
    if (!mBackingFile.is_valid())
    {
        return 0U;
    }
    {
        dp::emit_context ctx{*this};
        DPLX_TRY(dp::emit_break(ctx));
    }
    DPLX_TRY(sync_output());
    DPLX_TRY(auto const finalFileSize, mBackingFile.maximum_extent());
    mBackingFile.unlock_file();
    DPLX_TRY(mBackingFile.close());
    reset();
    return static_cast<std::uint32_t>(finalFileSize);
}

auto file_sink_backend::rotate() noexcept -> result<void>
{
    DPLX_TRY(auto const needsInit, do_rotate(mBackingFile));
    if (!needsInit)
    {
        return outcome::success();
    }
    reset(mBufferAllocation.as_span());
    dp::emit_context emitCtx{*this};

    DPLX_TRY(bulk_write(as_bytes(std::span(magic))));
    file_info info{.epoch = log_clock::epoch(), .attributes = {}};
    DPLX_TRY(dp::encode_object(emitCtx, info));

    DPLX_TRY(dp::emit_array_indefinite(emitCtx));
    llfio::file_handle::const_buffer_type writeBuffers[]
            = {mBufferAllocation.as_span().first(mBufferAllocation.size()
                                                 - size())};

    DPLX_TRY(mBackingFile.write({writeBuffers, 0U}))
    reset(mBufferAllocation.as_span());
    return outcome::success();
}

auto file_sink_backend::resize(std::size_t const requestedSize) noexcept
        -> result<void>
{
    reset();
    return mBufferAllocation.resize(static_cast<unsigned>(requestedSize));
}

auto file_sink_backend::do_grow(size_type requestedSize) noexcept
        -> result<void>
{
    if (size() != mBufferAllocation.size())
    {
        llfio::file_handle::const_buffer_type writeBuffers[]
                = {mBufferAllocation.as_span().first(mBufferAllocation.size()
                                                     - size())};

        DPLX_TRY(mBackingFile.write({writeBuffers, 0U}));
    }
    if (mBufferAllocation.size() < requestedSize)
    {
        DPLX_TRY(resize(requestedSize));
    }
    reset(mBufferAllocation.as_span());
    return outcome::success();
}

auto file_sink_backend::do_bulk_write(std::byte const *src,
                                      std::size_t srcSize) noexcept
        -> result<void>
{
    if (srcSize < mBufferAllocation.size() / 2)
    {
        auto const buffer = mBufferAllocation.as_span();
        llfio::file_handle::const_buffer_type writeBuffers[]
                = {buffer.first(buffer.size() - size())};
        DPLX_TRY(mBackingFile.write({writeBuffers, 0U}));
        reset(buffer);
        std::memcpy(buffer.data(), src, srcSize);
        commit_written(srcSize);
        return outcome::success();
    }

    auto const buffer = mBufferAllocation.as_span();
    llfio::file_handle::const_buffer_type writeBuffers[] = {
            {buffer.data(), buffer.size() - size()},
            {          src,                srcSize}
    };

    DPLX_TRY(mBackingFile.write({writeBuffers, 0U}));
    reset(buffer);
    return outcome::success();
}

auto file_sink_backend::do_sync_output() noexcept -> result<void>
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

    return rotate();
}

auto file_sink_backend::do_rotate(llfio::file_handle &backingFile) noexcept
        -> result<bool>
{
    DPLX_TRY(auto const maxExtent, backingFile.maximum_extent());
    return maxExtent == 0U;
}

template class basic_sink_frontend<file_sink_backend>;

file_sink_db_backend::~file_sink_db_backend()
{
    if (auto finalizeRx = finalize();
        finalizeRx.has_value() && finalizeRx.assume_value() != 0)
    {
        (void)mFileDatabase.update_record_container_size(
                mSinkId, mCurrentRotation, finalizeRx.assume_value());
    }
}

// NOLINTNEXTLINE(performance-noexcept-move-constructor)
auto file_sink_db_backend::operator=(file_sink_db_backend &&other)
        -> file_sink_db_backend &
{
    mFileDatabase
            .update_record_container_size(mSinkId, mCurrentRotation,
                                          finalize().value())
            .value();

    file_sink_backend::operator=(std::move(other));
    // NOLINTBEGIN(bugprone-use-after-move)
    mMaxFileSize = std::exchange(other.mMaxFileSize, 0U);
    mFileDatabase = std::move(other.mFileDatabase);
    mFileNamePattern = std::move(other.mFileNamePattern);
    mSinkId = std::exchange(other.mSinkId, file_sink_id{});
    mCurrentRotation = std::exchange(other.mCurrentRotation, 0U);
    // NOLINTEND(bugprone-use-after-move)
    return *this;
}

file_sink_db_backend::file_sink_db_backend(std::size_t const targetBufferSize,
                                           std::uint64_t const maxFileSize,
                                           file_sink_id const sinkId) noexcept
    : file_sink_backend(targetBufferSize)
    , mMaxFileSize(maxFileSize)
    , mFileDatabase()
    , mFileNamePattern()
    , mSinkId(sinkId)
    , mCurrentRotation()
{
}

auto file_sink_db_backend::create(config_type &&config) noexcept
        -> result<file_sink_db_backend>
{
    file_sink_db_backend self(config.target_buffer_size, config.max_file_size,
                              config.sink_id);
    try
    {
        self.mFileNamePattern = config.file_name_pattern;
    }
    catch (std::bad_alloc const &)
    {
        return system_error2::errc::not_enough_memory;
    }
    DPLX_TRY(self.mFileDatabase, config.database.clone());
    DPLX_TRY(self.initialize())

    return self;
}

auto file_sink_db_backend::do_rotate(llfio::file_handle &backingFile) noexcept
        -> result<bool>
{
    if (mFileNamePattern.empty())
    {
        return false;
    }
    if (backingFile.is_valid())
    {
        DPLX_TRY(auto const maxExtent, backingFile.maximum_extent());
        if (maxExtent <= mMaxFileSize)
        {
            return false;
        }

        {
            dp::emit_context ctx{*this};
            DPLX_TRY(dp::emit_break(ctx));
        }
        DPLX_TRY(sync_output());

        DPLX_TRY(auto const finalFileSize, backingFile.maximum_extent());
        (void)mFileDatabase.update_record_container_size(
                mSinkId, mCurrentRotation,
                static_cast<std::uint32_t>(finalFileSize));

        backingFile.unlock_file();
        DPLX_TRY(backingFile.close());
    }

    DPLX_TRY(auto &&created, mFileDatabase.create_record_container(
                                     mFileNamePattern, mSinkId, file_mode,
                                     file_caching, file_flags));
    backingFile = std::move(created.handle);
    mCurrentRotation = created.rotation;

    return true;
}

template class basic_sink_frontend<file_sink_db_backend>;

} // namespace dplx::dlog

DPLX_DLOG_DEFINE_AUTO_OBJECT_CODEC(::dplx::dlog::file_info)
