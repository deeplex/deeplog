
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

#include <dplx/cncr/misc.hpp>
#include <dplx/dp/object_def.hpp>
#include <dplx/dp/tuple_def.hpp>

#include <dplx/dlog/detail/utils.hpp>
#include <dplx/dlog/disappointment.hpp>
#include <dplx/dlog/llfio.hpp>

namespace dplx::dlog
{

enum class file_sink_id : std::uint32_t
{
    default_ = 0,
};

class file_database_handle
{
public:
    struct record_container_meta
    {
        std::filesystem::path path;
        unsigned byte_size;
        file_sink_id sink_id;
        unsigned rotation;

        static constexpr dp::tuple_def<
                dp::tuple_member_def<&record_container_meta::sink_id>{},
                dp::tuple_member_def<&record_container_meta::rotation>{},
                dp::tuple_member_def<&record_container_meta::byte_size>{},
                dp::tuple_member_def<&record_container_meta::path>{}>
                layout_descriptor{};
    };
    using record_containers_type = std::vector<record_container_meta>;

private:
    struct contents_t
    {
        unsigned revision;
        std::vector<record_container_meta> record_containers;

        static constexpr dp::object_def<
                dp::property_def<1, &contents_t::revision>{},
                dp::property_def<2, &contents_t::record_containers>{}>
                layout_descriptor{
                        .version = 0,
                        .allow_versioned_auto_decoder = true,
                };
    };

    llfio::file_handle mRootHandle;
    llfio::path_handle mRootDirHandle;
    contents_t mContents;
    std::string mFileNamePattern;

public:
    file_database_handle() noexcept = default;
    file_database_handle(llfio::file_handle rootHandle,
                         llfio::path_handle rootDirHandle,
                         std::string sinkFileNamePattern)
        : mRootHandle(std::move(rootHandle))
        , mRootDirHandle(std::move(rootDirHandle))
        , mContents{}
        , mFileNamePattern(std::move(sinkFileNamePattern))
    {
    }

    static auto file_database(llfio::path_handle const &base,
                              llfio::path_view const path,
                              std::string sinkFileNamePattern) noexcept
            -> result<file_database_handle>;

    static inline constexpr std::string_view extension{".drot"};
    static inline constexpr auto magic = cncr::make_byte_array<17>(
            {0x82, 0x4E, 0x0D, 0x0A, 0xAB, 0x7E, 0x7B, 0x64, 0x72, 0x6F, 0x74,
             0x7D, 0x7E, 0xBB, 0x0A, 0x1A, 0xA0},
            0);

    auto fetch_content() noexcept -> result<void>;
    auto unlink_all() noexcept -> result<void>;

private:
    auto fetch_content_impl() noexcept -> result<void>;

public:
    auto create_record_container(file_sink_id sinkId = file_sink_id::default_,
                                 llfio::file_handle::mode fileMode
                                 = llfio::file_handle::mode::write,
                                 llfio::file_handle::caching caching
                                 = llfio::file_handle::caching::reads,
                                 llfio::file_handle::flag flags
                                 = llfio::file_handle::flag::none)
            -> result<llfio::file_handle>;

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

private:
    static auto file_name(std::string const &pattern,
                          file_sink_id sinkId,
                          unsigned rotationCount) -> std::string;

    auto validate_magic() noexcept -> result<void>;
    auto initialize_storage() noexcept -> result<void>;
    auto retire_to_storage(contents_t const &contents) noexcept -> result<void>;
};

} // namespace dplx::dlog

template <typename Char>
struct fmt::formatter<dplx::dlog::file_sink_id, Char>
    : private fmt::formatter<std::underlying_type_t<dplx::dlog::file_sink_id>,
                             Char>
{
private:
    using value_type = dplx::dlog::file_sink_id;
    using underlying_type = std::underlying_type_t<value_type>;
    using base_type = fmt::formatter<underlying_type, Char>;

public:
    using base_type::base_type;
    using base_type::parse;

    template <typename FormatContext>
    auto format(value_type value, FormatContext &ctx) -> decltype(ctx.out())
    {
        return base_type::format(static_cast<underlying_type>(value), ctx);
    }
};
