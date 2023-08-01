
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <dplx/dp/fwd.hpp>
#include <dplx/dp/legacy/memory_buffer.hpp>
#include <dplx/dp/macros.hpp>
#include <dplx/dp/object_def.hpp>
#include <dplx/dp/streams/output_buffer.hpp>

#include <dplx/dlog/attribute_transmorpher.hpp>
#include <dplx/dlog/concepts.hpp>
#include <dplx/dlog/core/file_database.hpp>
#include <dplx/dlog/core/log_clock.hpp>
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

class file_sink_backend : public dp::output_buffer
{
public:
    struct config_type
    {
        llfio::file_handle file;
        std::size_t target_buffer_size;
    };

private:
    llfio::file_handle mBackingFile;
    dp::memory_allocation<llfio::utils::page_allocator<std::byte>>
            mBufferAllocation;
    std::size_t mTargetBufferSize{};

public:
    file_sink_backend() noexcept = default;
    virtual ~file_sink_backend();

    file_sink_backend(file_sink_backend &&) noexcept = default;
    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    auto operator=(file_sink_backend &&other) -> file_sink_backend &;

    friend inline void swap(file_sink_backend &lhs,
                            file_sink_backend &rhs) noexcept
    {
        using std::swap;
        // swap(static_cast<output_buffer &>(lhs),
        //      static_cast<output_buffer &>(rhs));
        std::span<std::byte> rhsBytes(rhs.data(), rhs.size());
        rhs.reset(lhs.data(), lhs.size());
        lhs.reset(rhsBytes.data(), rhsBytes.size());

        swap(lhs.mBackingFile, rhs.mBackingFile);
        swap(lhs.mBufferAllocation, rhs.mBufferAllocation);
        swap(lhs.mTargetBufferSize, rhs.mTargetBufferSize);
    }

protected:
    explicit file_sink_backend(std::size_t targetBufferSize) noexcept;

    auto initialize() noexcept -> result<void>;

public:
    static auto create(config_type &&config) noexcept
            -> result<file_sink_backend>;

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

    // returns the size of the finalized log record container
    // or 0 if file_sink_backend wasn't initialized
    auto finalize() noexcept -> result<std::uint32_t>;

private:
    auto rotate() noexcept -> result<void>;

    auto resize(std::size_t requestedSize) noexcept -> result<void>;
    auto do_grow(size_type requestedSize) noexcept -> result<void> final;
    auto do_bulk_write(std::byte const *src, std::size_t srcSize) noexcept
            -> result<void> final;
    auto do_sync_output() noexcept -> result<void> final;
    virtual auto do_rotate(llfio::file_handle &backingFile) noexcept
            -> result<bool>;
};

extern template class basic_sink_frontend<file_sink_backend>;
using file_sink = basic_sink_frontend<file_sink_backend>;

class file_sink_db_backend : public file_sink_backend
{
    std::uint64_t mMaxFileSize{};
    file_database_handle mFileDatabase;
    std::string mFileNamePattern;
    file_sink_id mSinkId{};
    std::uint32_t mCurrentRotation{};

public:
    ~file_sink_db_backend() override;
    file_sink_db_backend() noexcept = default;

    file_sink_db_backend(file_sink_db_backend &&) noexcept = default;
    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    auto operator=(file_sink_db_backend &&other) -> file_sink_db_backend &;

    friend inline void swap(file_sink_db_backend &lhs,
                            file_sink_db_backend &rhs) noexcept
    {
        using std::swap;
        swap(static_cast<file_sink_backend &>(lhs),
             static_cast<file_sink_backend &>(rhs));

        swap(lhs.mMaxFileSize, rhs.mMaxFileSize);
        swap(lhs.mFileDatabase, rhs.mFileDatabase);
        swap(lhs.mFileNamePattern, rhs.mFileNamePattern);
        swap(lhs.mSinkId, rhs.mSinkId);
        swap(lhs.mCurrentRotation, rhs.mCurrentRotation);
    }

private:
    explicit file_sink_db_backend(std::size_t targetBufferSize,
                                  std::uint64_t maxFileSize,
                                  file_sink_id sinkId) noexcept;

public:
    struct config_type
    {
        std::uint64_t max_file_size;
        file_database_handle const &database;
        std::string_view file_name_pattern;
        std::size_t target_buffer_size;
        file_sink_id sink_id;
    };

    static auto create(config_type &&config) noexcept
            -> result<file_sink_db_backend>;

private:
    auto do_rotate(llfio::file_handle &backingFile) noexcept
            -> result<bool> final;
};

extern template class basic_sink_frontend<file_sink_db_backend>;
using file_sink_db = basic_sink_frontend<file_sink_db_backend>;

} // namespace dplx::dlog

DPLX_DP_DECLARE_CODEC_SIMPLE(::dplx::dlog::file_info);
