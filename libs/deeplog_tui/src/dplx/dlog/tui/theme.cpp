
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/tui/theme.hpp"

using namespace ftxui::literals;

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers)

namespace dplx::dlog::tui
{

// carbon design system grey 90
auto theme_carbon_grey90() -> theme
{
    return theme{
            .ui_background = 0x26'2626_rgb,

            .interactive_01 = 0x0f'62fe_rgb,
            .interactive_02 = 0x6f'6f6f_rgb,
            .interactive_03 = 0xff'ffff_rgb,
            .interactive_04 = 0x45'89ff_rgb,

            .danger_01 = 0xda'1e28_rgb,
            .danger_02 = 0xff'8389_rgb,

            .ui_01 = 0x39'3939_rgb,
            .ui_02 = 0x52'5252_rgb,
            .ui_03 = 0x52'5252_rgb,
            .ui_04 = 0x8d'8d8d_rgb,
            .ui_05 = 0xf4'f4f4_rgb,

            .button_seperator = 0x16'1616_rgb,
            .decorative_01 = 0x6f'6f6f_rgb,

            .text_01 = 0xf4'f4f4_rgb,
            .text_02 = 0xc6'c6c6_rgb,
            .text_03 = 0x6f'6f6f_rgb,
            .text_04 = 0xff'ffff_rgb,
            .text_05 = 0x8d'8d8d_rgb,
            .text_error = 0xff'b3b8_rgb,

            .icon_01 = 0xf4'f4f4_rgb,
            .icon_02 = 0xc6'c6c6_rgb,
            .icon_03 = 0xff'ffff_rgb,

            .field_01 = 0x39'3939_rgb,
            .field_02 = 0x52'5252_rgb,

            .inverse_01 = 0x16'1616_rgb,
            .inverse_02 = 0xf4'f4f4_rgb,

            .support_01 = 0xff'8389_rgb,
            .support_02 = 0x42'be65_rgb,
            .support_03 = 0xf1'c21b_rgb,
            .support_04 = 0x45'89ff_rgb,

            .inverse_support_01 = 0xda'1e28_rgb,
            .inverse_support_02 = 0x24'a148_rgb,
            .inverse_support_03 = 0xf1'c21b_rgb,
            .inverse_support_04 = 0x00'43ce_rgb,

            .monochromatic_palette = {
                                      0xf6'f2ff_rgb, 0xe8'daff_rgb,
                                      0xd4'bbff_rgb, 0xbe'95ff_rgb,
                                      0xa5'6eff_rgb, 0x8a'3ffc_rgb,
                                      0x69'29c4_rgb, 0x49'1d8b_rgb,
                                      0x31'135e_rgb, 0x1c'0f30_rgb,
                                      }
    };
}

} // namespace dplx::dlog::tui

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers)
