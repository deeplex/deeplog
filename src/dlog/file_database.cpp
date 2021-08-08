
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include <dplx/dlog/file_database.hpp>

#include <bit>
#include <chrono>
#include <shared_mutex>

#include <fmt/format.h>

#include <dplx/dp/decoder/api.hpp>
#include <dplx/dp/decoder/object_utils.hpp>
#include <dplx/dp/decoder/std_container.hpp>
#include <dplx/dp/decoder/std_path.hpp>
#include <dplx/dp/decoder/tuple_utils.hpp>
#include <dplx/dp/encoder/api.hpp>
#include <dplx/dp/encoder/core.hpp>
#include <dplx/dp/encoder/object_utils.hpp>
#include <dplx/dp/encoder/std_path.hpp>
#include <dplx/dp/encoder/tuple_utils.hpp>

#include <dplx/dlog/detail/interleaving_stream.hpp>
#include <dplx/dlog/detail/utils.hpp>

namespace dplx::dlog
{

auto file_database_handle::file_database(
        llfio::path_handle const &base,
        llfio::path_view const path,
        std::string sinkFileNamePattern) noexcept
        -> result<file_database_handle>
{
    constexpr auto openMode = llfio::file_handle::mode::write;
    constexpr auto caching = llfio::file_handle::caching::reads;
    constexpr auto rootCreation = llfio::file_handle::creation::if_needed;
    DPLX_TRY(auto &&root,
             llfio::file(base, path, openMode, rootCreation, caching));

    llfio::path_handle rootDirHandle;
    if (auto const parentPath = path.parent_path(); !parentPath.empty())
    {
        DPLX_TRY(rootDirHandle, llfio::path(base, parentPath));
    }
    else if (base.is_valid())
    {
        DPLX_TRY(rootDirHandle, base.clone_to_path_handle());
    }
    else
    {
        constexpr llfio::path_view dot(".", llfio::path_view::native_format);
        DPLX_TRY(rootDirHandle, llfio::path(dot));
    }

    file_database_handle db(std::move(root), std::move(rootDirHandle),
                            sinkFileNamePattern);

    {
        llfio::unique_file_lock dbLock{db.mRootHandle,
                                       llfio::lock_kind::exclusive};

        DPLX_TRY(auto maxExtent, db.mRootHandle.maximum_extent());
        if (std::popcount(maxExtent) == 1)
        {
            DPLX_TRY(db.fetch_content_impl());
        }
        else
        {
            DPLX_TRY(db.initialize_storage());
        }
    }

    return std::move(db);
}

auto file_database_handle::fetch_content() noexcept -> result<void>
{
    llfio::unique_file_lock rootLock{mRootHandle, llfio::lock_kind::shared};
    return fetch_content_impl();
}

auto file_database_handle::unlink_all() noexcept -> result<void>
{
    llfio::unique_file_lock rootLock{mRootHandle, llfio::lock_kind::exclusive};
    DPLX_TRY(fetch_content_impl());

    for (auto &recordContainer : mContents.record_containers)
    {
        llfio::file_handle containerFile;
        if (auto openRx = llfio::file(mRootDirHandle, recordContainer.path,
                                      llfio::file_handle::mode::write);
            openRx.has_value())
        {
            containerFile = std::move(openRx).assume_value();
        }
        else
        {
            continue;
        }

        if (!containerFile.unlink())
        {
            continue;
        }
        recordContainer.rotation = 0u;
    }

    erase_if(mContents.record_containers,
             [](auto const &c) { return !c.rotation; });

    (void)retire_to_storage(mContents);

    if (!mContents.record_containers.empty())
    {
        return std::errc::directory_not_empty;
    }

    rootLock.unlock();
    DPLX_TRY(mRootHandle.unlink());

    *this = {};

    return oc::success();
}

auto file_database_handle::fetch_content_impl() noexcept -> result<void>
{
    auto odd = mContents.revision & 0b01;
    bool firstValid = false;

    DPLX_TRY(auto &&inStream,
             interleaving_input_stream_handle::interleaving_input_stream(
                     mRootHandle, false));
    if (auto decodeRx = dp::decode(dp::as_value<contents_t>, inStream);
        decodeRx.has_value())
    {
        firstValid = true;
        if (mContents.revision < decodeRx.assume_value().revision)
        {
            mContents = std::move(decodeRx).assume_value();
            odd = 0u;
        }
    }

    DPLX_TRY(inStream.reset(true));
    if (auto decodeRx = dp::decode(dp::as_value<contents_t>, inStream);
        decodeRx.has_value())
    {
        if (mContents.revision < decodeRx.assume_value().revision)
        {
            mContents = std::move(decodeRx).assume_value();
            odd = 1u;
        }
    }
    else if (!firstValid)
    {
        return std::move(decodeRx).as_failure();
    }

    if ((mContents.revision & 0b01) != odd)
    {
        mContents.revision += 1;
    }

    return oc::success();
}

auto file_database_handle::create_record_container(
        file_sink_id sinkId,
        llfio::file_handle::mode fileMode,
        llfio::file_handle::caching caching,
        llfio::file_handle::flag flags) -> result<llfio::file_handle>
{
    llfio::unique_file_lock rootLock{mRootHandle, llfio::lock_kind::exclusive};
    DPLX_TRY(fetch_content_impl());

    auto contents = mContents;
    contents.revision += 1;

    contents.record_containers.push_back(
            {.path = {}, .byte_size = 0u, .sink_id = sinkId, .rotation = 0u});
    auto &meta = contents.record_containers.back();

    auto lastRotation = std::ranges::max(contents.record_containers, {},
                                         [sinkId](auto const &rcm) {
                                             return rcm.sink_id == sinkId
                                                          ? rcm.rotation
                                                          : 0u;
                                         })
                                .rotation;
    meta.rotation = lastRotation + 1;

    llfio::file_handle file;
    do // open retry loop
    {
        meta.path = file_name(mFileNamePattern, meta.sink_id, meta.rotation);

        if (auto openRx
            = llfio::file(mRootDirHandle, meta.path, fileMode,
                          llfio::file_handle::creation::only_if_not_exist,
                          caching, flags);
            openRx.has_value())
        {
            file = std::move(openRx).assume_value();
        }
        else if (openRx.assume_error() == std::errc::file_exists)
        {
            // 5 retries hoping that the user chose a file name pattern which
            // disambiguates by timestamp or rotation count
            if (meta.rotation - 10 < lastRotation)
            {
                meta.rotation += 2; // preserve oddness
            }
            else
            {
                return std::move(openRx).as_failure();
            }
        }
        else
        {
            return std::move(openRx).as_failure();
        }
    } while (!file.is_valid());

    if (auto retireRx = retire_to_storage(contents); retireRx.has_failure())
    {
        (void)file.unlink();
        return std::move(retireRx).as_failure();
    }

    mContents = std::move(contents);
    return std::move(file);
}

auto file_database_handle::file_name(std::string const &pattern,
                                     file_sink_id sinkId,
                                     unsigned rotationCount) -> std::string
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
            static_cast<unsigned>(calendar.day()), timeOfDay.hours().count(),
            timeOfDay.minutes().count(), timeOfDay.seconds().count());

    return fmt::format(pattern, fmt::arg("id", detail::to_underlying(sinkId)),
                       fmt::arg("iso8601", iso8601DateTime),
                       fmt::arg("ctr", rotationCount));
}

auto file_database_handle::initialize_storage() noexcept -> result<void>
{
    DPLX_TRY(mRootHandle.truncate(4u << 12)); // 16KiB aka 4 pages
    DPLX_TRY(retire_to_storage(mContents));
    return oc::success();
}

auto file_database_handle::retire_to_storage(
        contents_t const &contents) noexcept -> result<void>
{
    DPLX_TRY(auto &&outStream,
             interleaving_output_stream_handle::interleaving_output_stream(
                     mRootHandle, contents.revision & 0b01));

    DPLX_TRY(dp::encode(outStream, contents));
    return oc::success();
}

} // namespace dplx::dlog
