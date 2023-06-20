
// Copyright Henrik S. Gaßmann 2021-2023
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <fmt/format.h>

#include <dplx/dlog/llfio.hpp>

#include "test_utils.hpp"

namespace dlog_tests
{

namespace llfio = dlog::llfio;

// we don't want to throw from within an initializer
// initialized in dlog.test.cpp
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern llfio::directory_handle test_dir;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
inline int test_file_ctr{0};

inline auto make_file_name(char const *testfile, char const *suffix)
        -> std::string
{
    auto const filename = llfio::path_view::zero_terminated_rendered_path<char>(
            llfio::path_view(testfile).filename().stem());

    return fmt::format("{}.{}.{}", filename.data(), test_file_ctr++, suffix);
}

inline constexpr std::size_t small_buffer_bus_size = 4096;

} // namespace dlog_tests

#define TEST_FILE_DMSB (::dlog_tests::make_file_name(__FILE__, ".dmsb"))
#define TEST_FILE_BB   (::dlog_tests::make_file_name(__FILE__, ".dbb"))
