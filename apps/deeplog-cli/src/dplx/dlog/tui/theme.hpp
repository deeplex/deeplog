
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <array>

#include <ftxui/screen/color.hpp>

namespace dplx::dlog::tui
{

struct theme
{
    using color = ftxui::Color;

    color ui_background;

    color interactive_01;
    color interactive_02;
    color interactive_03;
    color interactive_04;

    color danger_01;
    color danger_02;

    color ui_01;
    color ui_02;
    color ui_03;
    color ui_04;
    color ui_05;

    color button_seperator;
    color decorative_01;

    color text_01;
    color text_02;
    color text_03;
    color text_04;
    color text_05;
    color text_error;

    color icon_01;
    color icon_02;
    color icon_03;

    color field_01;
    color field_02;

    color inverse_01;
    color inverse_02;

    color support_01;
    color support_02;
    color support_03;
    color support_04;

    color inverse_support_01;
    color inverse_support_02;
    color inverse_support_03;
    color inverse_support_04;

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers)
    std::array<color, 10> monochromatic_palette;
};

auto theme_carbon_grey90() -> theme;

} // namespace dplx::dlog::tui
