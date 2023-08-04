
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
#include <dplx/make.hpp>

#include <dplx/dlog/attributes.hpp>
#include <dplx/dlog/concepts.hpp>
#include <dplx/dlog/core/file_database.hpp>
#include <dplx/dlog/core/log_clock.hpp>
#include <dplx/dlog/llfio.hpp>
#include <dplx/dlog/sinks/sink_frontend.hpp>

namespace dplx::dlog
{

class cbor_attribute_map
{
    std::vector<std::byte> mSerialized;

public:
    cbor_attribute_map() noexcept = default;
    cbor_attribute_map(detail::attribute_args const &attrs)
        : mSerialized()
    {
        insert_attributes(attrs).value();
    }

    template <typename... Attrs>
        requires(... && attribute<std::remove_cvref_t<Attrs>>)
    static auto from(Attrs &&...attrs) noexcept -> result<cbor_attribute_map>
    {
        cbor_attribute_map self;
        DPLX_TRY(self.insert_attributes(
                detail::stack_attribute_args<std::remove_cvref_t<Attrs>...>{
                        attrs...}));
        return self;
    }

    [[nodiscard]] auto bytes() const noexcept -> std::span<std::byte const>
    {
        return mSerialized;
    }

private:
    auto insert_attributes(detail::attribute_args const &attrRefs) noexcept
            -> result<void>;
};

} // namespace dplx::dlog

template <>
struct dplx::make<dplx::dlog::file_sink_backend>
{
    dlog::llfio::path_handle const &base;
    dlog::llfio::path_view path;
    std::size_t target_buffer_size;
    dlog::cbor_attribute_map attributes;

    auto operator()() const noexcept -> result<dlog::file_sink_backend>;
};

namespace dplx::dlog
{

class file_sink_backend : public dp::output_buffer
{
    friend struct make<file_sink_backend>;

    llfio::file_handle mBackingFile;
    dp::memory_allocation<llfio::utils::page_allocator<std::byte>>
            mBufferAllocation;
    std::size_t mTargetBufferSize{};
    dlog::cbor_attribute_map mContainerInfo;

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
    explicit file_sink_backend(std::size_t targetBufferSize,
                               dlog::cbor_attribute_map attributes) noexcept;

    auto initialize() noexcept -> result<void>;

public:
    using config_type = make<file_sink_backend>;
    static auto create(make<file_sink_backend> &&maker) noexcept
            -> result<file_sink_backend>
    {
        return maker();
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

} // namespace dplx::dlog

template <>
struct dplx::make<dplx::dlog::db_file_sink_backend>
{
    std::uint64_t max_file_size;
    dlog::file_database_handle const &database;
    std::string_view file_name_pattern;
    std::size_t target_buffer_size;
    dlog::file_sink_id sink_id;
    dlog::cbor_attribute_map attributes;

    auto operator()() const noexcept -> result<dlog::db_file_sink_backend>;
};

namespace dplx::dlog
{

class db_file_sink_backend : public file_sink_backend
{
    friend struct make<db_file_sink_backend>;

    std::uint64_t mMaxFileSize{};
    file_database_handle mFileDatabase;
    std::string mFileNamePattern;
    file_sink_id mSinkId{};
    std::uint32_t mCurrentRotation{};

public:
    ~db_file_sink_backend() override;
    db_file_sink_backend() noexcept = default;

    db_file_sink_backend(db_file_sink_backend &&) noexcept = default;
    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    auto operator=(db_file_sink_backend &&other) -> db_file_sink_backend &;

    friend inline void swap(db_file_sink_backend &lhs,
                            db_file_sink_backend &rhs) noexcept
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
    explicit db_file_sink_backend(std::size_t targetBufferSize,
                                  dlog::cbor_attribute_map attributes,
                                  std::uint64_t maxFileSize,
                                  std::string fileNamePattern,
                                  file_sink_id sinkId) noexcept;

public:
    using config_type = make<db_file_sink_backend>;
    static auto create(make<db_file_sink_backend> &&maker) noexcept
            -> result<db_file_sink_backend>
    {
        return maker();
    }

private:
    auto do_rotate(llfio::file_handle &backingFile) noexcept
            -> result<bool> final;
};

extern template class basic_sink_frontend<db_file_sink_backend>;
using db_file_sink = basic_sink_frontend<db_file_sink_backend>;
using file_sink_db = db_file_sink;

} // namespace dplx::dlog
