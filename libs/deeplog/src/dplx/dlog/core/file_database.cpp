
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/core/file_database.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <numeric>
#include <shared_mutex>
#include <utility>

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/xchar.h>

#include <dplx/dp.hpp>
#include <dplx/dp/api.hpp>
#include <dplx/dp/codecs/auto_enum.hpp>
#include <dplx/dp/codecs/auto_object.hpp>
#include <dplx/dp/codecs/auto_tuple.hpp>
#include <dplx/dp/codecs/core.hpp>
#include <dplx/dp/codecs/std-container.hpp>
#include <dplx/dp/codecs/std-filesystem.hpp>
#include <dplx/dp/codecs/std-string.hpp>
#include <dplx/dp/legacy/memory_buffer.hpp>
#include <dplx/scope_guard.hpp>

#include <dplx/dlog/attributes.hpp>
#include <dplx/dlog/bus/mpsc_bus.hpp>
#include <dplx/dlog/concepts.hpp>
#include <dplx/dlog/detail/interleaving_stream.hpp>
#include <dplx/dlog/detail/platform.hpp>
#include <dplx/dlog/detail/utils.hpp>
#include <dplx/dlog/sinks/file_sink.hpp>

namespace dplx::dlog
{

auto file_database_handle::clone() const noexcept
        -> result<file_database_handle>
{
    DPLX_TRY(auto &&rootHandle, mRootHandle.reopen());
    DPLX_TRY(auto &&rootDirHandle, mRootDirHandle.clone_to_path_handle());
    file_database_handle db(std::move(rootHandle), std::move(rootDirHandle));
    try
    {
        db.mContents = mContents;
    }
    catch (std::bad_alloc const &)
    {
        return system_error2::errc::not_enough_memory;
    }
    return db;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto file_database_handle::file_database(llfio::path_handle const &base,
                                         llfio::path_view const path) noexcept
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
        constexpr llfio::path_view dot(".", llfio::path_view::native_format,
                                       llfio::path_view::zero_terminated);
        DPLX_TRY(rootDirHandle, llfio::path(dot));
    }

    file_database_handle db(std::move(root), std::move(rootDirHandle));

    {
        llfio::unique_file_lock rootLock{db.mRootHandle,
                                         llfio::lock_kind::unlocked};
        DPLX_TRY(rootLock.lock());

        DPLX_TRY(auto maxExtent, db.mRootHandle.maximum_extent());
        if (maxExtent != 0)
        {
            DPLX_TRY(db.validate_magic());
            DPLX_TRY(db.fetch_content_impl());
        }
        else
        {
            DPLX_TRY(db.initialize_storage());
        }
    }

    return db;
}

auto file_database_handle::fetch_content() noexcept -> result<void>
{
    llfio::unique_file_lock rootLock{mRootHandle, llfio::lock_kind::unlocked};
    DPLX_TRY(rootLock.lock_shared());
    return fetch_content_impl();
}

auto file_database_handle::unlink_all() noexcept -> result<void>
{
    llfio::unique_file_lock rootLock{mRootHandle, llfio::lock_kind::unlocked};
    DPLX_TRY(rootLock.lock());
    DPLX_TRY(fetch_content_impl());

    unlink_all_record_containers_impl();
    unlink_all_message_buses_impl();

    (void)retire_to_storage(mContents);

    if (!mContents.record_containers.empty())
    {
        return errc::container_unlink_failed;
    }
    if (!mContents.message_buses.empty())
    {
        return errc::message_bus_unlink_failed;
    }

    rootLock.unlock();
    DPLX_TRY(mRootHandle.unlink());

    *this = {};

    return outcome::success();
}

auto file_database_handle::unlink_all_message_buses() noexcept -> result<void>
{
    llfio::unique_file_lock rootLock{mRootHandle, llfio::lock_kind::unlocked};
    DPLX_TRY(rootLock.lock());
    DPLX_TRY(fetch_content_impl());

    unlink_all_message_buses_impl();
    (void)retire_to_storage(mContents);

    if (!mContents.message_buses.empty())
    {
        return errc::message_bus_unlink_failed;
    }
    return outcome::success();
}

auto file_database_handle::unlink_all_record_containers() noexcept
        -> result<void>
{
    llfio::unique_file_lock rootLock{mRootHandle, llfio::lock_kind::unlocked};
    DPLX_TRY(rootLock.lock());
    DPLX_TRY(fetch_content_impl());

    unlink_all_record_containers_impl();
    (void)retire_to_storage(mContents);

    if (!mContents.record_containers.empty())
    {
        return errc::container_unlink_failed;
    }
    return outcome::success();
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
            odd = 0U;
        }
    }

    DPLX_TRY(inStream.reset(true));
    if (auto decodeRx = dp::decode(dp::as_value<contents_t>, inStream);
        decodeRx.has_value())
    {
        if (mContents.revision < decodeRx.assume_value().revision)
        {
            mContents = std::move(decodeRx).assume_value();
            odd = 1U;
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
    std::ranges::sort(mContents.record_containers, std::less<void>{},
                      [](record_container_meta const &self)
                      { return self.rotation; });
    return outcome::success();
}

auto file_database_handle::create_record_container(
        std::string_view const fileNamePattern,
        file_sink_id const sinkId,
        llfio::file_handle::mode const fileMode,
        llfio::file_handle::caching const caching,
        llfio::file_handle::flag const flags) -> result<record_container_file>
{
    llfio::unique_file_lock rootLock{mRootHandle, llfio::lock_kind::unlocked};
    DPLX_TRY(rootLock.lock());
    DPLX_TRY(fetch_content_impl());

    auto contents = mContents;
    contents.revision += 1;

    contents.record_containers.push_back(
            {.path = {}, .byte_size = 0U, .sink_id = sinkId, .rotation = 0U});
    auto &meta = contents.record_containers.back();

    auto const lastRotation = std::ranges::max(contents.record_containers, {},
                                               [sinkId](auto const &rcm) {
                                                   return rcm.sink_id == sinkId
                                                                ? rcm.rotation
                                                                : 0U;
                                               })
                                      .rotation;
    meta.rotation = lastRotation + 1;

    llfio::file_handle file;
    do // open retry loop
    {
        meta.path = record_container_filename(fileNamePattern, meta.sink_id,
                                              meta.rotation);

        if (auto openRx
            = llfio::file(mRootDirHandle, meta.path, fileMode,
                          llfio::file_handle::creation::only_if_not_exist,
                          caching, flags);
            openRx.has_value())
        {
            DPLX_TRY(openRx.assume_value().lock_file());
            file = std::move(openRx).assume_value();
        }
        else if (openRx.assume_error() == system_error::errc::file_exists)
        {
            // 5 retries hoping that the user chose a file name pattern which
            // disambiguates by timestamp or rotation count
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
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
        file.unlock_file();
        (void)file.unlink();
        return std::move(retireRx).as_failure();
    }

    mContents = std::move(contents);
    return record_container_file{.handle = std::move(file),
                                 .rotation = meta.rotation};
}

auto file_database_handle::update_record_container_size(file_sink_id which,
                                                        std::uint32_t rotation,
                                                        std::uint32_t newSize)
        -> result<void>
{
    llfio::unique_file_lock rootLock{mRootHandle, llfio::lock_kind::unlocked};
    DPLX_TRY(rootLock.lock());
    DPLX_TRY(fetch_content_impl());

    auto contents = mContents;
    contents.revision += 1;

    if (auto const it = std::ranges::find_if(
                contents.record_containers,
                [which, rotation](record_container_meta const &meta)
                { return meta.sink_id == which && meta.rotation == rotation; });
        it != contents.record_containers.end())
    {
        it->byte_size = newSize;
    }
    else
    {
        return errc::unknown_sink;
    }

    DPLX_TRY(retire_to_storage(contents));
    return outcome::success();
}

auto file_database_handle::prune_record_containers(
        std::function<result<bool>(llfio::file_handle &h,
                                   record_container_meta const &meta)>
                predicate) -> result<void>
{
    using file_handle = llfio::file_handle;
    return transform(
            [&](contents_t &contents) -> result<void>
            {
                std::erase_if(
                        contents.record_containers,
                        [&](record_container_meta &containerMeta)
                        {
                            file_handle container;
                            if (auto openRx = llfio::file(
                                        mRootDirHandle, containerMeta.path,
                                        file_handle::mode::write);
                                openRx.has_value())
                            {
                                container = std::move(openRx).assume_value();
                            }
                            else if (openRx.assume_error()
                                     == system_error::errc::
                                             no_such_file_or_directory)
                            {
                                return true;
                            }
                            else
                            {
                                return false;
                            }
                            llfio::unique_file_lock containerLock{
                                    container, llfio::lock_kind::unlocked};
                            if (!containerLock.try_lock())
                            {
                                return false;
                            }
                            sanitize_container_byte_size(container,
                                                         containerMeta);

                            auto predicateResult
                                    = predicate(container, containerMeta);
                            if (!container.is_valid())
                            {
                                containerLock.release();
                                return true;
                            }
                            auto pruneCandidate
                                    = predicateResult.has_value()
                                   && predicateResult.assume_value();
                            if (pruneCandidate)
                            {
                                containerLock.unlock();
                                (void)container.unlink();
                            }
                            return pruneCandidate;
                        });
                return outcome::success();
            });
}

auto file_database_handle::prune_record_containers(
        file_database_limits const limits) -> result<void>
{
    using file_handle = llfio::file_handle;
    return transform(
            [&](contents_t &contents) -> result<void>
            {
                auto const pageSize = llfio::utils::page_size();
                std::uint32_t numFiles = 0;
                std::uint64_t accumulatedSize = 0;
                // clang-14 and -15 can't grok `ranges::reverse_view`,
                // so reverse iteration isn't really possible with ranged for.
                // NOLINTNEXTLINE(modernize-loop-convert)
                for (auto it = contents.record_containers.rbegin(),
                          end = contents.record_containers.rend();
                     it != end; ++it)
                {
                    record_container_meta &containerMeta = *it;

                    file_handle container;
                    if (auto openRx
                        = llfio::file(mRootDirHandle, containerMeta.path,
                                      file_handle::mode::write);
                        openRx.has_value())
                    {
                        container = std::move(openRx).assume_value();
                    }
                    else
                    {
                        if (openRx.assume_error()
                            == system_error::errc::no_such_file_or_directory)
                        {
                            containerMeta = {};
                        }
                        continue;
                    }
                    llfio::unique_file_lock containerLock{
                            container, llfio::lock_kind::unlocked};
                    if (!containerLock.try_lock())
                    {
                        continue;
                    }

                    if (numFiles > limits.max_files_to_keep
                        || accumulatedSize >= limits.global_size_limit)
                    {
                        containerLock.unlock();
                        (void)container.unlink();
                        containerMeta = {};
                    }
                    else
                    {
                        sanitize_container_byte_size(container, containerMeta);
                        numFiles += 1;
                        accumulatedSize += cncr::round_up_p2(
                                containerMeta.byte_size, pageSize);
                    }
                }

                std::erase_if(contents.record_containers,
                              [](record_container_meta const &meta)
                              { return meta.path.empty(); });
                return outcome::success();
            });
}

auto file_database_handle::open_record_container(
        record_container_meta const &which,
        llfio::file_handle::mode fileMode,
        llfio::file_handle::caching caching,
        llfio::file_handle::flag flags) -> result<llfio::file_handle>
{
    DPLX_TRY(auto &&containerFile,
             llfio::file_handle::file(
                     mRootDirHandle, which.path, fileMode,
                     llfio::file_handle::creation::open_existing, caching,
                     flags));
    return std::move(containerFile);
}

auto file_database_handle::create_message_bus(
        std::string_view namePattern,
        std::string id,
        std::span<std::byte const> busMagic,
        llfio::file_handle::mode fileMode,
        llfio::file_handle::caching caching,
        llfio::file_handle::flag flags) -> result<message_bus_file>
{
    llfio::unique_file_lock rootLock{mRootHandle, llfio::lock_kind::unlocked};
    DPLX_TRY(rootLock.lock());
    DPLX_TRY(fetch_content_impl());

    auto contents = mContents;
    contents.revision += 1;

    contents.message_buses.push_back({
            .path = {},
            .magic = {busMagic.begin(), busMagic.end()},
            .id = std::move(id),
            .rotation = {},
            .process_id = detail::get_current_process_id(),
    });
    auto &meta = contents.message_buses.back();

    auto lastRotation
            = std::ranges::max(contents.message_buses, {},
                               [&meta](auto const &mbm) {
                                   return mbm.id == meta.id ? mbm.rotation : 0U;
                               })
                      .rotation;
    meta.rotation = lastRotation + 1;

    llfio::file_handle file;
    do // open retry loop
    {
        meta.path = message_bus_filename(namePattern, meta.id, meta.process_id,
                                         meta.rotation);

        if (auto openRx
            = llfio::file(mRootDirHandle, meta.path, fileMode,
                          llfio::file_handle::creation::only_if_not_exist,
                          caching, flags);
            openRx.has_value())
        {
            file = std::move(openRx).assume_value();
            DPLX_TRY(file.lock_file());
        }
        else if (openRx.assume_error() == system_error::errc::file_exists)
        {
            // 5 retries hoping that the user chose a file name pattern which
            // disambiguates by timestamp or rotation count
            // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
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
        file.unlock_file();
        (void)file.unlink();
        return std::move(retireRx).as_failure();
    }

    mContents = std::move(contents);
    return message_bus_file{.handle = std::move(file),
                            .rotation = meta.rotation};
}

auto file_database_handle::remove_message_bus(std::string_view id,
                                              std::uint32_t rotation)
        -> result<void>
{
    llfio::unique_file_lock rootLock{mRootHandle, llfio::lock_kind::unlocked};
    DPLX_TRY(rootLock.lock());
    DPLX_TRY(fetch_content_impl());

    auto contents = mContents;
    contents.revision += 1;

    if (std::erase_if(contents.message_buses, [id, rotation](auto const &mbm)
                      { return mbm.id == id && mbm.rotation == rotation; })
        == 0)
    {
        return errc::unknown_message_bus;
    }
    DPLX_TRY(retire_to_storage(contents));
    mContents = std::move(contents);
    return outcome::success();
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
auto file_database_handle::prune_message_buses(llfio::deadline deadline)
        -> result<void>
{
    using file_handle = llfio::file_handle;
    using mapped_file_handle = llfio::mapped_file_handle;
    LLFIO_DEADLINE_TO_SLEEP_INIT(deadline)

    llfio::unique_file_lock rootLock{mRootHandle, llfio::lock_kind::unlocked};
    // FIXME: look into a file lock solution which supports a deadline
    DPLX_TRY(rootLock.lock());
    DPLX_TRY(fetch_content_impl());

    auto contents = mContents;
    auto eraseBusEntry
            = [this, &contents](message_buses_type::iterator which) noexcept
            -> result<message_buses_type::iterator>
    {
        assert(!contents.message_buses.empty());
        using std::swap;
        contents.revision += 1;

        swap(*which, contents.message_buses.back());
        contents.message_buses.pop_back();

        DPLX_TRY(retire_to_storage(contents));

        return contents.message_buses.end();
    };

    for (auto it = mContents.message_buses.begin(),
              end = mContents.message_buses.end();
         it != end; ++it)
    {
        LLFIO_DEADLINE_TO_TIMEOUT_LOOP(deadline)

        auto &candidate = *it;
        if (auto mpscMagic = as_bytes(std::span(mpsc_bus_handle::magic));
            !std::equal(candidate.magic.begin(), candidate.magic.end(),
                        mpscMagic.begin(), mpscMagic.end()))
        {
            continue;
        }
        auto openRx = mapped_file_handle::mapped_file(
                mRootDirHandle, candidate.path, mapped_file_handle::mode::write,
                mapped_file_handle::creation::open_existing);
        if (openRx.has_error())
        {
            if (openRx.assume_error()
                == system_error::errc::no_such_file_or_directory)
            {
                DPLX_TRY(end, eraseBusEntry(it));
            }
            continue;
        }
        auto &handle = openRx.assume_value();
        llfio::unique_file_lock busLock{handle, llfio::lock_kind::unlocked};
        if (!busLock.try_lock())
        {
            continue;
        }
        scope_exit unlockBus = [&handle, &busLock]
        {
            if (!handle.is_valid())
            {
                busLock.release();
            }
        };

        bool rollback = true;

        contents.record_containers.push_back({
                .path = {},
                .byte_size = 0U,
                .sink_id = file_sink_id::recovered,
                .rotation = 0U,
        });
        scope_guard containerRollback = [&rollback, &contents]
        {
            if (rollback)
            {
                contents.record_containers.pop_back();
            }
        };
        auto &recoveryContainer = contents.record_containers.back();

        recoveryContainer.rotation = next_rotation(contents.record_containers,
                                                   file_sink_id::recovered);
        recoveryContainer.path = fmt::format(
                u8"{}.{}.{}.dlog", candidate.path.u8string(),
                file_sink_id::recovered, recoveryContainer.rotation);

        auto openRecoverySinkRx = make<file_sink>{
                        .threshold = severity::trace,
                        .backend = {
                                .base = mRootDirHandle,
                                .path = recoveryContainer.path,
                                .target_buffer_size = {}, // use default
                                .attributes = dlog::make_attributes(
                                    attr::process_id{candidate.process_id}
                                ),
                        },
                 }();
        if (openRecoverySinkRx.has_error())
        {
            continue;
        }
        auto rollbackRecoveryFileHandle = openRecoverySinkRx.assume_value()
                                                  .backend()
                                                  .clone_backing_file_handle();
        scope_guard rollbackSinkFileCreation
                = [&rollback, &rollbackRecoveryFileHandle]
        {
            if (rollback && rollbackRecoveryFileHandle.has_value())
            {
                (void)rollbackRecoveryFileHandle.assume_value().unlink();
            }
        };
        auto &recoverySink = openRecoverySinkRx.assume_value();

        detail::simple_consume_record_fn<file_sink,
                                         mpsc_bus_handle::consume_batch_size>
                consumeFn(recoverySink);

        DPLX_TRY(auto &&recoveryHandle,
                 static_cast<file_handle &>(handle).reopen());
        auto recoverRx = mpsc_bus_handle::recover_mpsc_bus(
                std::move(handle), consumeFn, llfio::lock_kind::exclusive);

        if (!recoverySink.try_finalize()
            || (recoverRx.has_error()
                && recoverRx.assume_error().domain()
                           != cncr::data_defined_status_domain<errc>))
        {
            continue;
        }
        if (handle.is_valid()) // NOLINT(bugprone-use-after-move)
        {
            busLock.unlock();
            (void)handle.close();
        }
        if (recoveryHandle.unlink().has_error())
        {
            continue;
        }
        DPLX_TRY(eraseBusEntry(it));
        rollback = false;
    }

    return outcome::success();
}

auto file_database_handle::record_container_filename(
        std::string_view pattern,
        file_sink_id sinkId,
        std::uint32_t rotationCount) -> std::string
{
    auto const now = std::chrono::floor<std::chrono::seconds>(
            std::chrono::system_clock::now());

    return fmt::format(fmt::runtime(pattern), fmt::arg("id", sinkId),
                       fmt::arg("now", now), fmt::arg("ctr", rotationCount));
}

auto file_database_handle::message_bus_filename(std::string_view pattern,
                                                std::string_view id,
                                                std::uint32_t processId,
                                                std::uint32_t rotationCount)
        -> std::string
{
    auto const now = std::chrono::floor<std::chrono::seconds>(
            std::chrono::system_clock::now());

    return fmt::format(fmt::runtime(pattern), fmt::arg("id", id),
                       fmt::arg("now", now), fmt::arg("pid", processId),
                       fmt::arg("ctr", rotationCount));
}

template <typename TransformFn>
auto file_database_handle::transform(TransformFn &&transformFn) -> result<void>
{
    llfio::unique_file_lock rootLock{mRootHandle, llfio::lock_kind::unlocked};
    DPLX_TRY(rootLock.lock());
    DPLX_TRY(fetch_content_impl());

    auto contents = mContents;
    contents.revision += 1;

    DPLX_TRY(transformFn(contents));

    DPLX_TRY(retire_to_storage(contents));
    return outcome::success();
}

auto file_database_handle::next_rotation(record_containers_type const &vs,
                                         file_sink_id sinkId) noexcept
        -> std::uint32_t
{

    return 1U
         + std::ranges::max(vs, {},
                            [sinkId](auto const &v)
                            { return v.sink_id == sinkId ? v.rotation : 0U; })
                   .rotation;
}

void file_database_handle::unlink_all_message_buses_impl() noexcept
{
    for (auto &messageBus : mContents.message_buses)
    {
        if (auto openRx = llfio::file(mRootDirHandle, messageBus.path,
                                      llfio::file_handle::mode::write);
            openRx.has_value())
        {
            llfio::file_handle &messageBusFile = openRx.assume_value();
            if (!messageBusFile.try_lock_file())
            {
                continue;
            }
            messageBusFile.unlock_file();
            if (messageBusFile.unlink().has_value())
            {
                messageBus.rotation = 0U;
            }
        }
        else if (openRx.assume_error()
                 == system_error::errc::no_such_file_or_directory)
        {
            messageBus.rotation = 0U;
        }
    }

    std::erase_if(mContents.message_buses,
                  [](auto const &c) { return !c.rotation; });
}

void file_database_handle::unlink_all_record_containers_impl() noexcept
{
    for (auto &recordContainer : mContents.record_containers)
    {
        if (auto openRx = llfio::file(mRootDirHandle, recordContainer.path,
                                      llfio::file_handle::mode::write);
            openRx.has_value())
        {
            llfio::file_handle &containerFile = openRx.assume_value();
            if (!containerFile.try_lock_file())
            {
                continue;
            }
            containerFile.unlock_file();
            if (containerFile.unlink().has_value())
            {
                recordContainer.rotation = 0U;
            }
        }
        else if (openRx.assume_error()
                 == system_error::errc::no_such_file_or_directory)
        {
            recordContainer.rotation = 0U;
        }
    }

    erase_if(mContents.record_containers,
             [](auto const &c) { return !c.rotation; });
}

auto file_database_handle::validate_magic() noexcept -> result<void>
{
    constexpr auto coordinationAreaSize = 2U * 4096U;
    dp::memory_allocation<llfio::utils::page_allocator<std::byte>> readBuffer;
    DPLX_TRY(readBuffer.resize(coordinationAreaSize));

    llfio::file_handle::buffer_type readBuffers[] = {readBuffer.as_span()};
    DPLX_TRY(auto const read, detail::xread(mRootHandle, {readBuffers, 0U}));
    if (read.size() != 1 || read[0].size() != readBuffer.size())
    {
        return errc::missing_data;
    }

    bytes header = read[0];
    if (!std::ranges::equal(header.first(std::ranges::size(magic)),
                            as_bytes(std::span(magic))))
    {
        return errc::invalid_file_database_header;
    }
    if (auto areaZero = header.subspan(std::ranges::size(magic));
        std::ranges::any_of(areaZero,
                            [](std::byte v) { return v != std::byte{}; }))
    {
        return errc::invalid_file_database_header;
    }

    return outcome::success();
}

auto file_database_handle::initialize_storage() noexcept -> result<void>
{
    constexpr auto initialSize = 4U * 4096U; // 16KiB aka 4 pages
    DPLX_TRY(mRootHandle.truncate(initialSize));
    DPLX_TRY(mRootHandle.zero({0U, initialSize}));

    llfio::file_handle::const_buffer_type writeBuffers[]
            = {as_bytes(std::span(magic))};
    DPLX_TRY(mRootHandle.write({writeBuffers, 0U}));

    DPLX_TRY(retire_to_storage(mContents));
    return outcome::success();
}

auto file_database_handle::retire_to_storage(
        contents_t const &contents) noexcept -> result<void>
{
    DPLX_TRY(auto &&outStream,
             interleaving_output_stream_handle::interleaving_output_stream(
                     mRootHandle, contents.revision & 0b01));

    DPLX_TRY(dp::encode(outStream, contents));
    return outcome::success();
}

void file_database_handle::sanitize_container_byte_size(
        llfio::file_handle &container,
        record_container_meta &containerMeta) noexcept
{
    if (containerMeta.byte_size == 0U)
    {
        if (auto maxExtentRx = container.maximum_extent();
            maxExtentRx.has_value())
        {
            containerMeta.byte_size
                    = static_cast<std::uint32_t>(std::min<std::uint64_t>(
                            UINT32_MAX, maxExtentRx.assume_value()));
        }
        containerMeta.byte_size
                = std::max<std::uint32_t>(containerMeta.byte_size, 1U);
    }
}

} // namespace dplx::dlog

DPLX_DLOG_DEFINE_AUTO_TUPLE_CODEC(
        ::dplx::dlog::file_database_handle::record_container_meta)
DPLX_DLOG_DEFINE_AUTO_TUPLE_CODEC(
        ::dplx::dlog::file_database_handle::message_bus_meta)

DPLX_DLOG_DEFINE_AUTO_OBJECT_CODEC(
        ::dplx::dlog::file_database_handle::contents_t)
