
// Copyright Henrik S. Gaßmann 2021-2023.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>
#include <cstring>
#include <utility>

#include <dplx/dp.hpp>
#include <dplx/dp/items/emit_core.hpp>
#include <dplx/dp/items/parse_core.hpp>
#include <dplx/dp/streams/memory_input_stream.hpp>
#include <dplx/dp/streams/memory_output_stream.hpp>

#include <dplx/dlog/concepts.hpp>
#include <dplx/dlog/disappointment.hpp>
#include <dplx/dlog/llfio.hpp>
#include <dplx/dlog/log_bus.hpp>

namespace dplx::dlog
{

class bufferbus_handle final : public bus_handle
{
    llfio::mapped_file_handle mBackingFile;
    std::span<std::byte> mBuffer;
    std::uint64_t mSpanPrngCtr{};
    std::size_t mWriteOffset;

public:
    explicit bufferbus_handle() noexcept
        : mBackingFile()
        , mBuffer()
        , mWriteOffset(0U)
    {
    }

    bufferbus_handle(bufferbus_handle &&other) noexcept
        : mBackingFile(std::move(other.mBackingFile))
        , mBuffer(std::exchange(other.mBuffer, std::span<std::byte>{}))
        , mWriteOffset(std::exchange(other.mWriteOffset, 0U))
    {
    }
    auto operator=(bufferbus_handle &&other) noexcept -> bufferbus_handle &
    {
        mBackingFile = std::exchange(other.mBackingFile,
                                     llfio::mapped_file_handle{});
        mBuffer = std::exchange(other.mBuffer, std::span<std::byte>{});
        mWriteOffset = std::exchange(other.mWriteOffset, 0U);

        return *this;
    }

    explicit bufferbus_handle(llfio::mapped_file_handle buffer,
                              std::size_t bufferCap)
        : mBackingFile(std::move(buffer))
        , mBuffer(mBackingFile.address(), bufferCap)
        , mWriteOffset(0U)
    {
    }

    class output_buffer final : public bus_output_buffer
    {
        friend class bufferbus_handle;

        using bus_output_buffer::bus_output_buffer;

    public:
    };

    static auto bufferbus(llfio::path_handle const &base,
                          llfio::path_view path,
                          unsigned int bufferSize) noexcept
            -> result<bufferbus_handle>
    {
        DPLX_TRY(
                auto &&backingFile,
                llfio::mapped_file(base, path, llfio::handle::mode::write,
                                   llfio::handle::creation::only_if_not_exist));

        return bufferbus(std::move(backingFile), bufferSize);
    }
    static auto bufferbus(llfio::mapped_file_handle &&backingFile,
                          std::size_t bufferSize) noexcept
            -> result<bufferbus_handle>
    {
        DPLX_TRY(auto truncatedSize, backingFile.truncate(bufferSize));
        if (truncatedSize != bufferSize)
        {
            return errc::bad;
        }

        // TODO: signal handler
        std::memset(backingFile.address(),
                    static_cast<int>(dp::type_code::null), bufferSize);

        return bufferbus_handle(std::move(backingFile), bufferSize);
    }

    template <typename ConsumeFn>
        requires bus_consumer<ConsumeFn &&>
    auto consume_content(ConsumeFn &&consume) noexcept -> result<void>
    {
        dp::memory_input_stream contentStream(mBuffer.first(mWriteOffset));
        dp::parse_context ctx{contentStream};

        while (!contentStream.empty())
        {
            DPLX_TRY(dp::item_head msgInfo, dp::parse_item_head(ctx));
            if (msgInfo.type != dp::type_code::binary || msgInfo.indefinite()
                || contentStream.size() < msgInfo.value) [[unlikely]]
            {
                // we ignore bad blocks
                // TODO: maybe do something more sensible
                // OTOH the log buffer is corrupted, so...
                break;
            }

            std::span<std::byte const> const msg(
                    contentStream.data(),
                    static_cast<std::size_t>(msgInfo.value));
            static_cast<ConsumeFn &&>(consume)(msg);

            contentStream.discard_buffered(
                    static_cast<std::size_t>(msgInfo.value));
        }

        return clear_content();
    }
    auto clear_content() -> result<void>
    {
        // TODO: signal handler (?)
        std::memset(mBuffer.data(), static_cast<int>(dp::type_code::null),
                    mBuffer.size());

        mWriteOffset = 0U;
        return oc::success();
    }

    [[nodiscard]] auto release() -> llfio::mapped_file_handle
    {
        mBuffer = std::span<std::byte>{};
        mWriteOffset = 0U;
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

private:
    auto do_create_output_buffer_inplace(
            output_buffer_storage &bufferPlacementStorage,
            std::size_t messageSize,
            span_id) noexcept -> result<bus_output_buffer *> override
    {
        auto const overhead = dp::detail::var_uint_encoded_size(messageSize);
        auto const totalSize = overhead + messageSize;

        if (totalSize > mBuffer.size() - mWriteOffset)
        {
            return errc::not_enough_space;
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        output_buffer out(mBackingFile.address() + mWriteOffset, totalSize);
        mWriteOffset += totalSize;

        dp::emit_context ctx{out};
        (void)dp::emit_binary(ctx, messageSize);
        return new (static_cast<void *>(&bufferPlacementStorage))
                output_buffer(static_cast<output_buffer &&>(out));
    }

    auto do_allocate_trace_id() noexcept -> trace_id override;
    auto do_allocate_span_id(trace_id trace) noexcept -> span_id override;
};

inline auto bufferbus(llfio::path_handle const &base,
                      llfio::path_view path,
                      unsigned int bufferSize) noexcept
        -> result<bufferbus_handle>
{
    return bufferbus_handle::bufferbus(base, path, bufferSize);
}
inline auto bufferbus(llfio::mapped_file_handle &&backingFile,
                      unsigned int bufferSize) noexcept
        -> result<bufferbus_handle>
{
    return bufferbus_handle::bufferbus(std::move(backingFile), bufferSize);
}

} // namespace dplx::dlog
