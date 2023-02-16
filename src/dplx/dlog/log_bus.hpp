
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <atomic>
#include <bit>
#include <concepts>
#include <type_traits>

#include <dplx/dp/items/emit_core.hpp>
#include <dplx/dp/items/parse_core.hpp>
#include <dplx/dp/legacy/memory_buffer.hpp>
#include <dplx/dp/legacy/memory_input_stream.hpp>
#include <dplx/dp/legacy/memory_output_stream.hpp>

#include <dplx/dlog/concepts.hpp>
#include <dplx/dlog/detail/utils.hpp>
#include <dplx/dlog/disappointment.hpp>
#include <dplx/dlog/llfio.hpp>

namespace dplx::dlog
{

class bufferbus_handle
{
    llfio::mapped_file_handle mBackingFile;
    dp::memory_buffer mBuffer;

public:
    explicit bufferbus_handle() noexcept = default;

    bufferbus_handle(bufferbus_handle const &) = delete;
    auto operator=(bufferbus_handle const &) -> bufferbus_handle & = delete;

    bufferbus_handle(bufferbus_handle &&other) noexcept
        : mBackingFile(std::move(other.mBackingFile))
        , mBuffer(std::exchange(other.mBuffer, dp::memory_buffer{}))
    {
    }
    auto operator=(bufferbus_handle &&other) noexcept -> bufferbus_handle &
    {
        mBackingFile = std::exchange(other.mBackingFile,
                                     llfio::mapped_file_handle{});
        mBuffer = std::exchange(other.mBuffer, dp::memory_buffer{});

        return *this;
    }

    explicit bufferbus_handle(llfio::mapped_file_handle buffer,
                              unsigned bufferCap)
        : mBackingFile(std::move(buffer))
        , mBuffer(mBackingFile.address(), bufferCap, 0)
    {
    }

    using output_stream = dp::memory_buffer;

    struct logger_token
    {
    };
    static auto create_token() noexcept -> result<logger_token>
    {
        return logger_token{};
    }

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
                          unsigned bufferSize) noexcept
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

    auto write(logger_token &, unsigned msgSize) noexcept
            -> result<output_stream>
    {
        auto const overhead = dp::detail::var_uint_encoded_size(msgSize);
        auto const totalSize = overhead + msgSize;

        if (totalSize > static_cast<unsigned>(mBuffer.remaining_size()))
        {
            return errc::not_enough_space;
        }

        dp::memory_buffer msgBuffer(
                mBuffer.consume(static_cast<int>(totalSize)), totalSize, 0);

        auto &&outStream = dp::get_output_buffer(msgBuffer);
        DPLX_TRY(dp::detail::store_var_uint(outStream, msgSize,
                                            dp::type_code::binary));
        DPLX_TRY(outStream.sync_output());

        return dp::memory_buffer{msgBuffer.remaining()};
    }

    void commit(logger_token &)
    {
    }

    template <typename ConsumeFn>
        requires bus_consumer<ConsumeFn &&>
    auto consume_content(ConsumeFn &&consume) noexcept -> result<void>
    {
        dp::memory_view content(mBuffer.consumed_begin(),
                                mBuffer.consumed_size(), 0);
        auto &&contentStream = dp::get_input_buffer(content);
        dp::parse_context ctx{contentStream};

        while (content.remaining_size() > 0)
        {
            DPLX_TRY(dp::item_head msgInfo, dp::parse_item_head(ctx));
            if (msgInfo.type != dp::type_code::binary || msgInfo.indefinite()
                || content.remaining_size() < msgInfo.value) [[unlikely]]
            {
                // we ignore bad blocks
                // TODO: maybe do something more sensible
                // OTOH the log buffer is corrupted, so...
                break;
            }

            DPLX_TRY(contentStream.sync_input());
            std::span<std::byte const> const msg(
                    content.consume(static_cast<int>(msgInfo.value)),
                    static_cast<std::size_t>(msgInfo.value));
            contentStream.discard_buffered(msgInfo.value);

            static_cast<ConsumeFn &&>(consume)(msg);
        }

        return clear_content();
    }
    auto clear_content() -> result<void>
    {
        // TODO: signal handler (?)
        std::memset(mBuffer.consumed_begin(),
                    static_cast<int>(dp::type_code::null),
                    mBuffer.consumed_size());

        mBuffer.move_consumer_to(0);
        return oc::success();
    }

    auto release() -> llfio::mapped_file_handle
    {
        mBuffer = dp::memory_buffer{};
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
};
static_assert(bus<bufferbus_handle>);

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

template <bus Bus>
struct bus_write_lock
{
    using bus_type = Bus;
    using token_type = typename bus_type::logger_token;

    bus_type &bus;
    token_type &token;

    BOOST_FORCEINLINE explicit bus_write_lock(bus_type &busRef,
                                              token_type &tokenRef)
        : bus(busRef)
        , token(tokenRef)
    {
    }
    BOOST_FORCEINLINE ~bus_write_lock()
    {
        bus.commit(token);
    }
};
} // namespace dplx::dlog
