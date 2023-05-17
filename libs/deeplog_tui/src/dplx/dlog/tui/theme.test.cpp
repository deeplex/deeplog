
// Copyright Henrik S. Gaßmann 2023.
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/tui/theme.hpp"

#include <catch2/catch_test_macros.hpp>

#include "test_utils.hpp"

namespace dlog_tui_tests
{

TEST_CASE("Dlog TUI provides the carbon_grey90 theme")
{
    auto theme = dlog::tui::theme_carbon_grey90();
    CHECK(theme.ui_background != ftxui::Color{});
    CHECK(theme.inverse_support_04 != ftxui::Color{});
}

} // namespace dlog_tui_tests
