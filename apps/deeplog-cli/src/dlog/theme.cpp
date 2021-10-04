
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include <dplx/dlog/tui/theme.hpp>

using namespace ftxui::literals;

namespace dplx::dlog::tui
{

// carbon design system grey 90
auto theme_carbon_grey90() -> theme
{
    return theme{.ui_background = 0x262626_rgb,

                 .interactive_01 = 0x0f62fe_rgb,
                 .interactive_02 = 0x6f6f6f_rgb,
                 .interactive_03 = 0xffffff_rgb,
                 .interactive_04 = 0x4589ff_rgb,

                 .danger_01 = 0xda1e28_rgb,
                 .danger_02 = 0xff8389_rgb,

                 .ui_01 = 0x393939_rgb,
                 .ui_02 = 0x525252_rgb,
                 .ui_03 = 0x525252_rgb,
                 .ui_04 = 0x8d8d8d_rgb,
                 .ui_05 = 0xf4f4f4_rgb,

                 .button_seperator = 0x161616_rgb,
                 .decorative_01 = 0x6f6f6f_rgb,

                 .text_01 = 0xf4f4f4_rgb,
                 .text_02 = 0xc6c6c6_rgb,
                 .text_03 = 0x6f6f6f_rgb,
                 .text_04 = 0xffffff_rgb,
                 .text_05 = 0x8d8d8d_rgb,
                 .text_error = 0xffb3b8_rgb,

                 .icon_01 = 0xf4f4f4_rgb,
                 .icon_02 = 0xc6c6c6_rgb,
                 .icon_03 = 0xffffff_rgb,

                 .field_01 = 0x393939_rgb,
                 .field_02 = 0x525252_rgb,

                 .inverse_01 = 0x161616_rgb,
                 .inverse_02 = 0xf4f4f4_rgb,

                 .support_01 = 0xff8389_rgb,
                 .support_02 = 0x42be65_rgb,
                 .support_03 = 0xf1c21b_rgb,
                 .support_04 = 0x4589ff_rgb,

                 .inverse_support_01 = 0xda1e28_rgb,
                 .inverse_support_02 = 0x24a148_rgb,
                 .inverse_support_03 = 0xf1c21b_rgb,
                 .inverse_support_04 = 0x0043ce_rgb,

                 .monochromatic_palette = {
                         0xf6f2ff_rgb,
                         0xe8daff_rgb,
                         0xd4bbff_rgb,
                         0xbe95ff_rgb,
                         0xa56eff_rgb,
                         0x8a3ffc_rgb,
                         0x6929c4_rgb,
                         0x491d8b_rgb,
                         0x31135e_rgb,
                         0x1c0f30_rgb,
                 }};
}

} // namespace dplx::dlog::tui
