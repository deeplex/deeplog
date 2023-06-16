
// Copyright Henrik Steffen Gaßmann 2021,2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/bus/mpsc_bus.hpp"

#include <algorithm>
#include <array>
#include <thread>

#include <dplx/cncr/utils.hpp>
#include <dplx/dp/api.hpp>
#include <dplx/dp/codecs/auto_object.hpp>
#include <dplx/dp/codecs/core.hpp>
#include <dplx/dp/object_def.hpp>
#include <dplx/predef/hardware.h>

#if DPLX_HW_SIMD_X86 >= DPLX_HW_SIMD_X86_SSE4_1_VERSION
#include <nmmintrin.h>
#endif

namespace dplx::dlog::detail
{

template <std::size_t S>
struct uint
{
};
template <>
struct uint<sizeof(std::uint8_t)>
{
    using type = std::uint8_t;
};
template <>
struct uint<sizeof(std::uint16_t)>
{
    using type = std::uint16_t;
};
template <>
struct uint<sizeof(std::uint32_t)>
{
    using type = std::uint32_t;
};
template <>
struct uint<sizeof(std::uint64_t)>
{
    using type = std::uint64_t;
};
template <std::size_t S>
using uint_t = typename uint<S>::type;

template <std::size_t S = sizeof(std::thread::id)>
    requires requires { typename uint<S>::type; }
auto thread_id_hash(std::thread::id id) noexcept -> std::uint32_t
{
#if DPLX_HW_SIMD_X86 >= DPLX_HW_SIMD_X86_SSE4_1_VERSION
    if constexpr (S == sizeof(std::uint8_t))
    {
        return _mm_crc32_u8(0, std::bit_cast<uint_t<S>>(id));
    }
    if constexpr (S == sizeof(std::uint16_t))
    {
        return _mm_crc32_u16(0, std::bit_cast<uint_t<S>>(id));
    }
    if constexpr (S == sizeof(std::uint32_t))
    {
        return _mm_crc32_u32(0, std::bit_cast<uint_t<S>>(id));
    }
    if constexpr (S == sizeof(std::uint64_t))
    {
        return static_cast<std::uint32_t>(
                _mm_crc32_u64(0, std::bit_cast<uint_t<S>>(id)));
    }
#else
    // NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)
    // NOLINTNEXTLINE(modernize-use-auto)
    std::uint64_t x = std::bit_cast<uint_t<S>>(id);
    x ^= x >> 27;
    x *= 0x3c79ac492ba7b653U;
    x ^= x >> 33;
    x *= 0x1c69b3f74ac4ae35U;
    x ^= x >> 27;
    return static_cast<std::uint32_t>(x);
    // NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
#endif
}
auto hashed_this_thread_id() noexcept -> std::uint32_t
{
    return detail::thread_id_hash(std::this_thread::get_id());
}

} // namespace dplx::dlog::detail

namespace dplx::dlog
{

auto mpsc_bus_handle::mpsc_bus(llfio::path_handle const &base,
                               llfio::path_view const path,
                               std::uint32_t const numRegions,
                               std::uint32_t const regionSize) noexcept
        -> result<mpsc_bus_handle>
{
    using handle = llfio::mapped_file_handle;
    DPLX_TRY(auto &&mappedFile,
             llfio::mapped_file(base, path, handle::mode::write,
                                handle::creation::only_if_not_exist,
                                handle::caching::temporary));
    return mpsc_bus(std::move(mappedFile), numRegions, regionSize);
}
auto mpsc_bus_handle::mpsc_bus(llfio::mapped_file_handle &&backingFile,
                               std::uint32_t const numRegions,
                               std::uint32_t const regionSize) noexcept
        -> result<mpsc_bus_handle>
{
    using extent_type = llfio::file_handle::extent_type;
    static_assert(sizeof(unsigned) <= sizeof(std::size_t));
    static_assert(sizeof(unsigned) <= sizeof(extent_type));

    if (!backingFile.is_valid() || !backingFile.is_writable())
    {
        return errc::invalid_argument;
    }
    if (numRegions == 0 || regionSize < min_region_size)
    {
        return errc::invalid_argument;
    }

    constexpr std::size_t page_size = std::size_t{4U} * 1024U;
    if (std::numeric_limits<std::uint32_t>::max() - page_size < regionSize)
    {
        return errc::invalid_argument;
    }
    std::size_t const realRegionSize = cncr::round_up_p2(regionSize, page_size);

    std::size_t const combinedRegionSize = numRegions * realRegionSize;
    if (combinedRegionSize / realRegionSize != numRegions)
    {
        return errc::invalid_argument;
    }

    std::size_t const fileSize = combinedRegionSize + head_area_size;
    if (fileSize < combinedRegionSize || !std::in_range<extent_type>(fileSize))
    {
        return errc::invalid_argument;
    }

    llfio::unique_file_lock fileLock(backingFile, llfio::lock_kind::unlocked);
    DPLX_TRY(fileLock.lock());

    DPLX_TRY(backingFile.truncate(static_cast<extent_type>(fileSize)));

    // TODO: signal handler

    // initialize the head area
    std::span busMemory(backingFile.address(),
                        static_cast<std::size_t>(fileSize));
    dp::memory_output_stream busStream(busMemory);

    DPLX_TRY(busStream.bulk_write(as_bytes(std::span(magic)).data(),
                                  std::size(magic)));

    DPLX_TRY(dp::encode(busStream, info{.num_regions = numRegions,
                                        .region_size = static_cast<unsigned>(
                                                realRegionSize)}));

    // initialize the regions
    busStream = dp::memory_output_stream(busMemory.subspan(head_area_size));
    while (!busStream.empty())
    {
        ::new (static_cast<void *>(busStream.data())) region_ctrl{0, 0, 0, {}};
        busStream.commit_written(region_ctrl_overhead);

        // invoke implicit object creation rules
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        std::fill_n(reinterpret_cast<std::uint32_t *>(busStream.data()),
                    (realRegionSize - region_ctrl_overhead) / block_size,
                    unused_block_content);
        busStream.commit_written(realRegionSize - region_ctrl_overhead);
    }

    // we can't transfer the unique_lock_file due to moving the handle;
    // instead the mpsc bus handle takes over lock ownership
    fileLock.release();
    return mpsc_bus_handle{std::move(backingFile), numRegions,
                           static_cast<std::uint32_t>(realRegionSize)};
}

auto mpsc_bus_handle::do_create_span_context(std::string_view name,
                                             severity &thresholdOut) noexcept
        -> span_context
{
    return create_span_context(trace_id::random(), name, thresholdOut);
}

auto dplx::dlog::mpsc_bus_handle::do_create_span_context(trace_id trace,
                                                         std::string_view,
                                                         severity &) noexcept
        -> span_context
{
    if (trace == trace_id::invalid())
    {
        return span_context{};
    }

    auto const traceBlob
            = std::bit_cast<cncr::blob<std::uint32_t, 4, alignof(trace_id)>>(
                    trace);
    auto const regionId = detail::hash_to_index(
            traceBlob.values[0] ^ traceBlob.values[1] ^ traceBlob.values[2]
                    ^ traceBlob.values[3],
            mNumRegions);
    auto const ctr
            = std::atomic_ref<std::uint64_t>(region(regionId)->span_prng_ctr)
                      .fetch_add(1U, std::memory_order_relaxed);
    auto const rawTraceId
            = std::bit_cast<cncr::blob<std::uint64_t, 2, alignof(trace_id)>>(
                    trace);

    return {trace, detail::derive_span_id(rawTraceId.values[0],
                                          rawTraceId.values[1], ctr)};
}

} // namespace dplx::dlog

namespace dplx::dlog
{

namespace
{

constexpr dp::object_def<
        dp::property_def<1U, &mpsc_bus_info_v00::num_regions>{},
        dp::property_def<2U, &mpsc_bus_info_v00::region_size>{}>
        info_v00_descriptor{.version = 0U};

}

} // namespace dplx::dlog

auto ::dplx::dp::codec<dplx::dlog::mpsc_bus_info_v00>::size_of(
        emit_context &ctx, dplx::dlog::mpsc_bus_info_v00 const &value) noexcept
        -> std::uint64_t
{
    return dp::size_of_object<dplx::dlog::info_v00_descriptor>(ctx, value);
}
auto ::dplx::dp::codec<dplx::dlog::mpsc_bus_info_v00>::encode(
        emit_context &ctx, dplx::dlog::mpsc_bus_info_v00 const &value) noexcept
        -> result<void>
{
    return dp::encode_object<dplx::dlog::info_v00_descriptor>(ctx, value);
}
/*
auto ::dplx::dp::codec<dplx::dlog::mpsc_bus_info_v00>::decode(
        parse_context &ctx, dplx::dlog::mpsc_bus_info_v00 &outValue) noexcept
        -> result<void>
{
    return dp::decode_object(ctx, outValue);
}
*/