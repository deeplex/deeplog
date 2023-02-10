
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
#include <dplx/dp/legacy/memory_output_stream.hpp>
#include <dplx/dp/object_def.hpp>

#include <dplx/dlog/concepts.hpp>
#include <dplx/dlog/core.hpp>
#include <dplx/dlog/detail/utils.hpp>
#include <dplx/dlog/llfio.hpp>
#include <dplx/dlog/log_clock.hpp>

namespace dplx::dlog
{

template <typename T>
concept sink_backend
        = std::movable<T>
       && requires(T &sinkBackend, unsigned msgSize, std::u8string_view text) {
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
        auto &&outStream = dp::get_output_buffer(outBuffer);

        DPLX_TRY(outStream.bulk_write(rawRecord.data(), rawRecord.size()));
        return oc::success();
    }
    auto drain_closed_impl() noexcept -> result<void> override
    {
        DPLX_TRY(mBackend.drain_closed());
        return oc::success();
    }
};

struct file_info
{
    log_clock::epoch_info epoch;

    static inline constexpr dp::object_def<
            dp::property_def<4U, &file_info::epoch>{}>
            layout_descriptor{.version = 0U,
                              .allow_versioned_auto_decoder = true};
};

class file_sink_backend
{
    using rotate_fn = std::function<result<void>(llfio::file_handle &)>;

    dp::memory_buffer mWriteBuffer;
    llfio::file_handle mBackingFile;

    dp::memory_allocation<llfio::utils::page_allocator<std::byte>>
            mBufferAllocation;
    rotate_fn mRotateBackingFile;
    unsigned mTargetBufferSize{};

public:
    file_sink_backend() noexcept = default;

    static auto file_sink(llfio::file_handle backingFile,
                          unsigned targetBufferSize,
                          rotate_fn &&rotate) noexcept
            -> result<file_sink_backend>
    {
        file_sink_backend self{};
        self.mTargetBufferSize = targetBufferSize;

        DPLX_TRY(self.mBufferAllocation.resize(targetBufferSize));
        self.mWriteBuffer = self.mBufferAllocation.as_memory_buffer();
        auto &&outBuffer = dp::get_output_buffer(self.mWriteBuffer);
        dp::emit_context emitCtx{outBuffer};

        DPLX_TRY(outBuffer.bulk_write(magic.data(), magic.size()));
        file_info info{.epoch = log_clock::epoch()};
        DPLX_TRY(dp::encode_object(emitCtx, info));

        DPLX_TRY(dp::emit_array_indefinite(emitCtx));
        DPLX_TRY(outBuffer.sync_output());

        if (rotate)
        {
            DPLX_TRY(rotate(backingFile));
        }
        self.mBackingFile = std::move(backingFile);

        DPLX_TRY(auto const maxExtent, self.mBackingFile.maximum_extent());
        // if created
        if (maxExtent == 0U)
        {
            llfio::file_handle::const_buffer_type writeBuffers[] = {
                    {self.mWriteBuffer.consumed_begin(),
                     self.mWriteBuffer.consumed_size()}
            };
            DPLX_TRY(self.mBackingFile.write({writeBuffers, 0U}));

            self.mWriteBuffer = self.mBufferAllocation.as_memory_buffer();
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
    static inline constexpr auto magic = detail::make_byte_array<16>(
            {0x83, 0x4e, 0x0d, 0x0a, 0xab, 0x7e, 0x7b, 0x64, 0x6c, 0x6f, 0x67,
             0x7d, 0x7e, 0xbb, 0x0a, 0x1a});

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
    static auto drain_opened() noexcept -> result<void>
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
            llfio::file_handle::const_buffer_type writeBuffers[] = {
                    {mWriteBuffer.consumed_begin(),
                     mWriteBuffer.consumed_size()}
            };

            DPLX_TRY(mBackingFile.write({writeBuffers, 0U}));
        }

        if (mRotateBackingFile)
        {
            DPLX_TRY(mRotateBackingFile(mBackingFile));
        }
        return oc::success();
    }

    auto finalize() noexcept -> result<void>
    {
        if (!mBackingFile.is_valid())
        {
            return oc::success();
        }
        auto &&outBuffer = dp::get_output_buffer(mWriteBuffer);
        dp::emit_context ctx{outBuffer};
        DPLX_TRY(dp::emit_break(ctx));
        DPLX_TRY(outBuffer.sync_output());
        llfio::file_handle::const_buffer_type writeBuffers[] = {
                {mWriteBuffer.consumed_begin(), mWriteBuffer.consumed_size()}
        };

        DPLX_TRY(mBackingFile.write({writeBuffers, 0U}));

        DPLX_TRY(mBackingFile.close());

        *this = file_sink_backend();

        return oc::success();
    }
};

} // namespace dplx::dlog

DPLX_DLOG_DECLARE_CODEC(::dplx::dlog::file_info);
