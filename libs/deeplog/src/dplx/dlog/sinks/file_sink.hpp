
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

#include <dplx/dp/fwd.hpp>
#include <dplx/dp/legacy/memory_buffer.hpp>
#include <dplx/dp/macros.hpp>
#include <dplx/dp/object_def.hpp>
#include <dplx/dp/streams/output_buffer.hpp>

#include <dplx/dlog/attribute_transmorpher.hpp>
#include <dplx/dlog/concepts.hpp>
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
    ~file_sink_backend();

    file_sink_backend(file_sink_backend &&) noexcept = default;
    auto operator=(file_sink_backend &&) noexcept
            -> file_sink_backend & = default;

    static auto file_sink(llfio::file_handle backingFile,
                          unsigned targetBufferSize,
                          rotate_fn &&rotate) noexcept
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

    auto finalize() noexcept -> result<void>;

private:
    auto resize(std::size_t requestedSize) noexcept -> dp::result<void>;
    auto do_grow(size_type requestedSize) noexcept -> dp::result<void> override;
    auto do_bulk_write(std::byte const *src, std::size_t srcSize) noexcept
            -> dp::result<void> override;
    auto do_sync_output() noexcept -> dp::result<void> override;
};

extern template class basic_sink_frontend<file_sink_backend>;
using file_sink = basic_sink_frontend<file_sink_backend>;

} // namespace dplx::dlog

DPLX_DP_DECLARE_CODEC_SIMPLE(::dplx::dlog::file_info);
