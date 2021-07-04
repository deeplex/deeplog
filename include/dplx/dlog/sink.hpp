
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

#include <dplx/dp/decoder/tuple_utils.hpp>
#include <dplx/dp/memory_buffer.hpp>
#include <dplx/dp/skip_item.hpp>
#include <dplx/dp/streams/memory_output_stream.hpp>

#include <dplx/dlog/concepts.hpp>
#include <dplx/dlog/core.hpp>
#include <dplx/dlog/detail/utils.hpp>
#include <dplx/dlog/llfio.hpp>

namespace dplx::dlog
{

template <typename T>
concept sink_backend = std::movable<T> && requires(T &sinkBackend,
                                                   unsigned msgSize,
                                                   std::u8string_view text)
{
    {
        sinkBackend.write(msgSize)
        } -> detail::tryable_result<dp::memory_buffer>;
    //{
    //    sinkBackend.write_static_string(text)
    //    } -> detail::tryable;
    {
        sinkBackend.drain_opened()
        } -> detail::tryable;
    {
        sinkBackend.drain_closed()
        } -> detail::tryable;
};

template <sink_backend Backend>
class basic_sink_frontend : public sink_frontend_base
{
    Backend mBackend;

public:
    using backend_type = Backend;

    basic_sink_frontend(basic_sink_frontend const &) = delete;
    auto operator=(basic_sink_frontend const &)
            -> basic_sink_frontend & = delete;

    basic_sink_frontend(basic_sink_frontend &&) noexcept = default;
    auto operator=(basic_sink_frontend &&) noexcept
            -> basic_sink_frontend & = default;

    basic_sink_frontend(severity threshold, backend_type backend)
        : sink_frontend_base(threshold, predicate_type{})
        , mBackend(std::move(backend))
    {
    }
    basic_sink_frontend(severity threshold,
                        predicate_type shouldDiscard,
                        backend_type backend)
        : sink_frontend_base(threshold, shouldDiscard)
        , mBackend(std::move(backend))
    {
    }

    auto backend() noexcept -> backend_type &
    {
        return mBackend;
    }
    auto backend() const noexcept -> backend_type const &
    {
        return mBackend;
    }

private:
    auto retire(bytes rawRecord,
                [[maybe_unused]] additional_record_info const
                        &additionalInfo) noexcept -> result<void> override
    {
        DPLX_TRY(auto &&outBuffer,
                 mBackend.write(static_cast<unsigned>(rawRecord.size())));

        DPLX_TRY(dp::write(outBuffer, rawRecord.data(), rawRecord.size()));
        return oc::success();
    }
    auto drain_closed_impl() noexcept -> result<void> override
    {
        DPLX_TRY(mBackend.drain_closed());
        return oc::success();
    }
};

class file_sink_backend
{
    dp::memory_buffer mWriteBuffer;
    llfio::file_handle mBackingFile;
    llfio::directory_handle mLogDirectory;

    dp::memory_allocation<llfio::utils::page_allocator<std::byte>>
            mBufferAllocation;
    std::string mLogFileNamePattern;
    llfio::file_handle::extent_type mFileRotationSizeThreshold;
    std::function<bool()> mShouldRotate;
    unsigned mTargetBufferSize;

public:
    file_sink_backend() noexcept = default;

    static auto
    file_sink(llfio::path_handle const &base,
              llfio::path_view logDirectoryPath,
              std::string &&logFileNamePattern,
              llfio::file_handle::extent_type fileRotationSizeThreshold,
              std::function<bool()> &&shouldRotate,
              unsigned targetBufferSize) noexcept -> result<file_sink_backend>
    {
        file_sink_backend self{};
        self.mTargetBufferSize = targetBufferSize;
        self.mFileRotationSizeThreshold = fileRotationSizeThreshold;

        DPLX_TRY(self.mBufferAllocation.resize(targetBufferSize));
        self.mWriteBuffer = self.mBufferAllocation.as_memory_buffer();

        std::string fileName;
        try
        {
            fileName = file_name(logFileNamePattern, 0);
        }
        catch (const std::bad_alloc &)
        {
            return std::errc::not_enough_memory;
        }

        DPLX_TRY(
                self.mLogDirectory,
                llfio::directory(base, logDirectoryPath,
                                 llfio::directory_handle::mode::write,
                                 llfio::directory_handle::creation::if_needed));

        DPLX_TRY(dp::item_emitter<dp::memory_buffer>::array_indefinite(
                self.mWriteBuffer));

        // TODO: properly detect whether we created the file or not
        DPLX_TRY(
                self.mBackingFile,
                llfio::file(self.mLogDirectory, fileName,
                            llfio::file_handle::mode::append,
                            llfio::file_handle::creation::always_new,
                            llfio::file_handle::caching::reads_and_metadata,
                            llfio::file_handle::flag::disable_safety_barriers));

        // if created
        {
            llfio::file_handle::const_buffer_type writeBuffers[]
                    = {self.mWriteBuffer.consumed()};
            DPLX_TRY(self.mBackingFile.write({writeBuffers, 0}));

            self.mWriteBuffer = self.mBufferAllocation.as_memory_buffer();
        }

        self.mLogFileNamePattern = std::move(logFileNamePattern);
        self.mShouldRotate = std::move(shouldRotate);

        return std::move(self);
    }

    auto write(unsigned size) noexcept -> result<dp::memory_buffer>
    {
        if (mWriteBuffer.remaining_size() < size)
        {
            DPLX_TRY(write_to_handle());

            if (mWriteBuffer.remaining_size() < size)
            {
                DPLX_TRY(mBufferAllocation.resize(size));
            }

            mWriteBuffer = mBufferAllocation.as_memory_buffer();
        }

        return dp::memory_buffer(mWriteBuffer.consume(static_cast<int>(size)),
                                 size, 0);
    }
    auto drain_opened() noexcept -> result<void>
    {
        return oc::success();
    }

    auto drain_closed() noexcept -> result<void>
    {
        DPLX_TRY(write_to_handle());
        DPLX_TRY(mBufferAllocation.resize(mTargetBufferSize));
        mWriteBuffer = mBufferAllocation.as_memory_buffer();

        return oc::success();
    }

    auto write_to_handle() noexcept -> result<void>
    {
        if (mWriteBuffer.consumed_size() > 0)
        {
            llfio::file_handle::const_buffer_type writeBuffers[]
                    = {mWriteBuffer.consumed()};

            DPLX_TRY(mBackingFile.write({writeBuffers, 0U}));
        }

        DPLX_TRY(auto maxExtent, mBackingFile.maximum_extent());
        if (maxExtent > mFileRotationSizeThreshold
            || (mShouldRotate && mShouldRotate()))
        {
            DPLX_TRY(rotate());
        }

        return oc::success();
    }

    auto rotate() noexcept -> result<void>
    {
        // TODO: implement file rotation
        return oc::success();
    }

    auto finalize() noexcept -> result<void>
    {
        if (!mBackingFile.is_valid())
        {
            return oc::success();
        }
        DPLX_TRY(dp::item_emitter<dp::memory_buffer>::break_(mWriteBuffer));
        llfio::file_handle::const_buffer_type writeBuffers[]
                = {mWriteBuffer.consumed()};

        DPLX_TRY(mBackingFile.write({writeBuffers, 0U}));

        DPLX_TRY(mBackingFile.close());

        *this = file_sink_backend();

        return oc::success();
    }

private:
    static auto file_name(std::string const &pattern, int rotationCount)
            -> std::string
    {
        using namespace std::string_view_literals;

        auto const timePoint = std::chrono::system_clock::now();
        auto const date = std::chrono::floor<std::chrono::days>(timePoint);
        std::chrono::hh_mm_ss const timeOfDay{timePoint - date};
        std::chrono::year_month_day const calendar{date};

        auto const iso8601DateTime = fmt::format(
                "{:04}{:02}{:02}T{:02}{:02}{:02}"sv,
                static_cast<int>(calendar.year()),
                static_cast<unsigned>(calendar.month()),
                static_cast<unsigned>(calendar.day()),
                timeOfDay.hours().count(), timeOfDay.minutes().count(),
                timeOfDay.seconds().count());

        return fmt::format(pattern, fmt::arg("iso8601", iso8601DateTime),
                           fmt::arg("rot-ctr", rotationCount));
    }
};

} // namespace dplx::dlog
