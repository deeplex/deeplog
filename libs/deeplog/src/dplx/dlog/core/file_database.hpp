
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include <dplx/dp/macros.hpp>
#include <dplx/dp/object_def.hpp>
#include <dplx/dp/tuple_def.hpp>

#include <dplx/dlog/disappointment.hpp>
#include <dplx/dlog/llfio.hpp>

namespace dplx::dlog
{

enum class file_sink_id : std::uint32_t
{
    default_ = 0,
    recovered = 13,
};
constexpr auto format_as(file_sink_id id) noexcept
        -> std::underlying_type_t<file_sink_id>
{
    return static_cast<std::underlying_type_t<file_sink_id>>(id);
}

struct file_database_limits
{
    std::uint32_t max_files_to_keep;
    std::uint64_t global_size_limit;
};

class file_database_handle
{
public:
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    struct record_container_meta
    {
        std::filesystem::path path;
        std::uint32_t byte_size;
        file_sink_id sink_id;
        std::uint32_t rotation;

        static constexpr dp::tuple_def<
                dp::tuple_member_def<&record_container_meta::sink_id>{},
                dp::tuple_member_def<&record_container_meta::rotation>{},
                dp::tuple_member_def<&record_container_meta::byte_size>{},
                dp::tuple_member_def<&record_container_meta::path>{}>
                layout_descriptor{};
    };
    using record_containers_type = std::vector<record_container_meta>;

    struct message_bus_meta
    {
        std::filesystem::path path;
        std::vector<std::byte> magic;
        std::string id;
        std::uint32_t rotation{};
        std::uint32_t process_id{};

        static constexpr dp::tuple_def<
                dp::tuple_member_def<&message_bus_meta::magic>{},
                dp::tuple_member_def<&message_bus_meta::id>{},
                dp::tuple_member_def<&message_bus_meta::rotation>{},
                dp::tuple_member_def<&message_bus_meta::process_id>{},
                dp::tuple_member_def<&message_bus_meta::path>{}>
                layout_descriptor{};
    };
    using message_buses_type = std::vector<message_bus_meta>;

private:
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
    struct contents_t
    {
        unsigned long long revision;
        std::vector<record_container_meta> record_containers;
        std::vector<message_bus_meta> message_buses;

        static constexpr dp::object_def<
                dp::property_def<1, &contents_t::revision>{},
                dp::property_def<2, &contents_t::record_containers>{},
                dp::property_def<3, &contents_t::message_buses>{}>
                layout_descriptor{
                        .version = 0,
                        .allow_versioned_auto_decoder = true,
                };
    };
    friend class dplx::dp::codec<contents_t>;

    llfio::file_handle mRootHandle;
    llfio::path_handle mRootDirHandle;
    contents_t mContents;

public:
    file_database_handle() noexcept = default;

    file_database_handle(file_database_handle const &) = delete;
    auto operator=(file_database_handle const &)
            -> file_database_handle & = delete;

    file_database_handle(file_database_handle &&) noexcept = default;
    auto operator=(file_database_handle &&) noexcept
            -> file_database_handle & = default;

private:
    static constexpr llfio::deadline default_100ms
            = std::chrono::milliseconds{100};

    file_database_handle(llfio::file_handle rootHandle,
                         llfio::path_handle rootDirHandle)
        : mRootHandle(std::move(rootHandle))
        , mRootDirHandle(std::move(rootDirHandle))
        , mContents{}
    {
    }

public:
    auto clone() const noexcept -> result<file_database_handle>;

    static auto file_database(llfio::path_handle const &base,
                              llfio::path_view path) noexcept
            -> result<file_database_handle>;

    static inline constexpr std::string_view extension{".drot"};
    static inline constexpr std::uint8_t magic[17]
            = {0x82, 0x4e, 0x0d, 0x0a, 0xab, 0x7e, 0x7b, 0x64, 0x72,
               0x6f, 0x74, 0x7d, 0x7e, 0xbb, 0x0a, 0x1a, 0xa0};

    auto fetch_content() noexcept -> result<void>;
    auto unlink_all() noexcept -> result<void>;
    auto unlink_all_message_buses() noexcept -> result<void>;
    auto unlink_all_record_containers() noexcept -> result<void>;

private:
    auto fetch_content_impl() noexcept -> result<void>;

public:
    struct record_container_file
    {
        llfio::file_handle handle;
        std::uint32_t rotation;
    };
    auto create_record_container(std::string_view namePattern,
                                 file_sink_id sinkId = file_sink_id::default_,
                                 llfio::file_handle::mode fileMode
                                 = llfio::file_handle::mode::write,
                                 llfio::file_handle::caching caching
                                 = llfio::file_handle::caching::reads,
                                 llfio::file_handle::flag flags
                                 = llfio::file_handle::flag::none)
            -> result<record_container_file>;
    auto update_record_container_size(file_sink_id which,
                                      std::uint32_t rotation,
                                      std::uint32_t newSize) -> result<void>;
    auto prune_record_containers(
            std::function<result<bool>(llfio::file_handle &h,
                                       record_container_meta const &meta)>
                    predicate) -> result<void>;

    auto prune_record_containers(file_database_limits limits) -> result<void>;

    auto open_record_container(record_container_meta const &which,
                               llfio::file_handle::mode fileMode
                               = llfio::file_handle::mode::read,
                               llfio::file_handle::caching caching
                               = llfio::file_handle::caching::reads,
                               llfio::file_handle::flag flags
                               = llfio::file_handle::flag::none)
            -> result<llfio::file_handle>;

    auto record_containers() const noexcept -> record_containers_type const &
    {
        return mContents.record_containers;
    }

    struct message_bus_file
    {
        llfio::file_handle handle;
        std::uint32_t rotation;
    };
    auto create_message_bus(std::string_view namePattern,
                            std::string id,
                            std::span<std::byte const> busMagic,
                            llfio::file_handle::mode fileMode
                            = llfio::file_handle::mode::write,
                            llfio::file_handle::caching caching
                            = llfio::file_handle::caching::reads,
                            llfio::file_handle::flag flags
                            = llfio::file_handle::flag::none)
            -> result<message_bus_file>;
    auto remove_message_bus(std::string_view id, std::uint32_t rotation)
            -> result<void>;
    auto prune_message_buses(llfio::deadline deadline = default_100ms)
            -> result<void>;

    auto message_buses() const noexcept -> message_buses_type const &
    {
        return mContents.message_buses;
    }

private:
    static auto record_container_filename(std::string_view pattern,
                                          file_sink_id sinkId,
                                          std::uint32_t rotationCount)
            -> std::string;
    static auto message_bus_filename(std::string_view pattern,
                                     std::string_view id,
                                     std::uint32_t processId,
                                     std::uint32_t rotationCount)
            -> std::string;

    template <typename TransformFn>
    auto transform(TransformFn &&transformFn) -> result<void>;

    void unlink_all_message_buses_impl() noexcept;
    void unlink_all_record_containers_impl() noexcept;

    auto validate_magic() noexcept -> result<void>;
    auto initialize_storage() noexcept -> result<void>;
    auto retire_to_storage(contents_t const &contents) noexcept -> result<void>;

    static auto next_rotation(record_containers_type const &vs,
                              file_sink_id sinkId) noexcept -> std::uint32_t;

    static void
    sanitize_container_byte_size(llfio::file_handle &container,
                                 record_container_meta &containerMeta) noexcept;
};

} // namespace dplx::dlog

DPLX_DP_DECLARE_CODEC_SIMPLE(
        ::dplx::dlog::file_database_handle::record_container_meta);
DPLX_DP_DECLARE_CODEC_SIMPLE(
        ::dplx::dlog::file_database_handle::message_bus_meta);
DPLX_DP_DECLARE_CODEC_SIMPLE(::dplx::dlog::file_database_handle::contents_t);
