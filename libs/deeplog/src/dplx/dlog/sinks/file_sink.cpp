
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
#include <dplx/dp/streams/memory_output_stream.hpp>
#include <dplx/scope_guard.hpp>

#include <dplx/dlog/record_container.hpp>

namespace dplx::dlog
{

auto cbor_attribute_map::insert_attributes(
        detail::attribute_args const &attrRefs) noexcept -> result<void>
{
    auto const encodedSize = dp::encoded_size_of(attrRefs);
    try
    {
        mSerialized.resize(encodedSize);
    }
    catch (std::bad_alloc const &)
    {
        return system_error::errc::not_enough_memory;
    }
    dp::memory_output_stream ostream(mSerialized);
    return dp::encode(ostream, attrRefs);
}

} // namespace dplx::dlog

auto dplx::make<dplx::dlog::file_sink_backend>::operator()() const noexcept
        -> result<dlog::file_sink_backend>
try
{
    using namespace dplx::dlog;
    using file_creation = llfio::file_handle::creation;

    constexpr unsigned defaultBufferSize = 64U * 1024;
    file_sink_backend self{target_buffer_size > 0U ? target_buffer_size
                                                   : defaultBufferSize,
                           attributes};
    DPLX_TRY(self.mBackingFile,
             llfio::file(base, path, file_sink_backend::file_mode,
                         file_creation::only_if_not_exist,
                         file_sink_backend::file_caching,
                         file_sink_backend::file_flags));

    llfio::unique_file_lock fileLock(self.mBackingFile,
                                     llfio::lock_kind::unlocked);
    DPLX_TRY(fileLock.lock());
    DPLX_TRY(self.initialize());
    fileLock.release();
    return self;
}
catch (std::bad_alloc const &)
{
    return system_error::errc::not_enough_memory;
}

namespace dplx::dlog
{

file_sink_backend::~file_sink_backend()
{
    (void)finalize();
}

// NOLINTNEXTLINE(performance-noexcept-move-constructor,cppcoreguidelines-noexcept-move-operations)
auto file_sink_backend::operator=(file_sink_backend &&other)
        -> file_sink_backend &
{
    finalize().value();

    output_buffer::operator=(static_cast<output_buffer &&>(other));
    mBackingFile = std::move(other.mBackingFile);
    mBufferAllocation = std::move(other.mBufferAllocation);
    mTargetBufferSize = std::exchange(other.mTargetBufferSize, 0U);
    mContainerInfo = std::exchange(other.mContainerInfo, {});
    return *this;
}

file_sink_backend::file_sink_backend(
        std::size_t targetBufferSize,
        dlog::cbor_attribute_map attributes) noexcept
    : mBackingFile{}
    , mBufferAllocation{}
    , mTargetBufferSize{targetBufferSize}
    , mContainerInfo{std::move(attributes)}
{
}

auto file_sink_backend::initialize() noexcept -> result<void>
{
    DPLX_TRY(resize(mTargetBufferSize));
    DPLX_TRY(rotate());
    return outcome::success();
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

auto file_sink_backend::clone_backing_file_handle() const noexcept
        -> result<llfio::file_handle>
{
    if (!mBackingFile.is_valid())
    {
        return system_error::errc::bad_file_descriptor;
    }
    DPLX_TRY(auto &&cloned, mBackingFile.reopen());
    return cloned;
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

    DPLX_TRY(dp::emit_map(emitCtx, 3U));

    // resource_v00 version information
    DPLX_TRY(dp::detail::store_inline_value(*this, 0U, dp::type_code::posint));
    DPLX_TRY(dp::emit_integer(emitCtx,
                              record_resource::layout_descriptor.version));

    // epoch
    DPLX_TRY(dp::emit_integer(
            emitCtx, record_resource::layout_descriptor.property<0U>().id));
    DPLX_TRY(dp::encode(emitCtx, log_clock::epoch()));

    // attributes
    DPLX_TRY(dp::emit_integer(
            emitCtx, record_resource::layout_descriptor.property<1U>().id));
    if (auto attributeBytes = mContainerInfo.bytes(); attributeBytes.empty())
    {
        DPLX_TRY(dp::emit_map(emitCtx, 0U));
    }
    else
    {
        DPLX_TRY(bulk_write(attributeBytes));
    }

    DPLX_TRY(dp::emit_array_indefinite(emitCtx));
    llfio::file_handle::const_buffer_type writeBuffers[]
            = {mBufferAllocation.as_span().first(mBufferAllocation.size()
                                                 - size())};

    DPLX_TRY(mBackingFile.write({writeBuffers, 0U}));
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

} // namespace dplx::dlog

auto dplx::make<dplx::dlog::db_file_sink_backend>::operator()() const noexcept
        -> result<dlog::db_file_sink_backend>
try
{
    using namespace dplx::dlog;

    db_file_sink_backend self{target_buffer_size, attributes, max_file_size,
                              std::string(file_name_pattern), sink_id};
    DPLX_TRY(self.mFileDatabase, database.clone());
    DPLX_TRY(self.initialize())

    return self;
}
catch (std::bad_alloc const &)
{
    return system_error::errc::not_enough_memory;
}

namespace dplx::dlog
{

db_file_sink_backend::~db_file_sink_backend()
{
    if (auto finalizeRx = finalize();
        finalizeRx.has_value() && finalizeRx.assume_value() != 0)
    {
        (void)mFileDatabase.update_record_container_size(
                mSinkId, mCurrentRotation, finalizeRx.assume_value());
    }
}

// NOLINTNEXTLINE(performance-noexcept-move-constructor,cppcoreguidelines-noexcept-move-operations)
auto db_file_sink_backend::operator=(db_file_sink_backend &&other)
        -> db_file_sink_backend &
{
    mFileDatabase
            .update_record_container_size(mSinkId, mCurrentRotation,
                                          finalize().value())
            .value();

    file_sink_backend::operator=(static_cast<file_sink_backend &&>(other));
    mMaxFileSize = std::exchange(other.mMaxFileSize, 0U);
    mFileDatabase = std::move(other.mFileDatabase);
    mFileNamePattern = std::move(other.mFileNamePattern);
    mSinkId = std::exchange(other.mSinkId, file_sink_id{});
    mCurrentRotation = std::exchange(other.mCurrentRotation, 0U);
    return *this;
}

db_file_sink_backend::db_file_sink_backend(std::size_t const targetBufferSize,
                                           dlog::cbor_attribute_map attributes,
                                           std::uint64_t const maxFileSize,
                                           std::string fileNamePattern,
                                           file_sink_id const sinkId) noexcept
    : file_sink_backend(targetBufferSize, std::move(attributes))
    , mMaxFileSize(maxFileSize)
    , mFileDatabase()
    , mFileNamePattern(std::move(fileNamePattern))
    , mSinkId(sinkId)
    , mCurrentRotation()
{
}

auto db_file_sink_backend::do_rotate(llfio::file_handle &backingFile) noexcept
        -> result<bool>
{
    if (mFileNamePattern.empty())
    {
        return false;
    }
    if (backingFile.is_valid())
    {
        if (mCurrentRotation == 0)
        {
            return false;
        }
        DPLX_TRY(auto const maxExtent, backingFile.maximum_extent());
        if (maxExtent <= mMaxFileSize && log_clock::epoch() == mFileEpoch)
        {
            return false;
        }

        {
            dp::emit_context ctx{*this};
            DPLX_TRY(dp::emit_break(ctx));
        }
        auto const rotation = std::exchange(mCurrentRotation, 0);
        scope_guard releaseFile = [&backingFile] {
            backingFile.unlock_file();
            (void)backingFile.close();
        };
        DPLX_TRY(sync_output());

        DPLX_TRY(auto const finalFileSize, backingFile.maximum_extent());
        (void)mFileDatabase.update_record_container_size(
                mSinkId, rotation, static_cast<std::uint32_t>(finalFileSize));
    }

    DPLX_TRY(auto &&created, mFileDatabase.create_record_container(
                                     mFileNamePattern, mSinkId, file_mode,
                                     file_caching, file_flags));
    backingFile = std::move(created.handle);
    mFileEpoch = log_clock::epoch();
    mCurrentRotation = created.rotation;

    return true;
}

template class basic_sink_frontend<db_file_sink_backend>;

} // namespace dplx::dlog
