
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include <dplx/dlog/tui/theme.hpp>

namespace dplx::dlog::tui
{

inline auto operator""_color(unsigned long long int v) -> ftxui::Color
{
    auto const r = static_cast<std::uint8_t>(v >> 16);
    auto const g = static_cast<std::uint8_t>(v >> 8);
    auto const b = static_cast<std::uint8_t>(v);
    return ftxui::Color(r, g, b);
}

// carbon design system grey 90
auto theme_carbon_grey90() -> theme
{
    return theme{.ui_background = 0x262626_color,

                 .interactive_01 = 0x0f62fe_color,
                 .interactive_02 = 0x6f6f6f_color,
                 .interactive_03 = 0xffffff_color,
                 .interactive_04 = 0x4589ff_color,

                 .danger_01 = 0xda1e28_color,
                 .danger_02 = 0xff8389_color,

                 .ui_01 = 0x393939_color,
                 .ui_02 = 0x525252_color,
                 .ui_03 = 0x525252_color,
                 .ui_04 = 0x8d8d8d_color,
                 .ui_05 = 0xf4f4f4_color,

                 .button_seperator = 0x161616_color,
                 .decorative_01 = 0x6f6f6f_color,

                 .text_01 = 0xf4f4f4_color,
                 .text_02 = 0xc6c6c6_color,
                 .text_03 = 0x6f6f6f_color,
                 .text_04 = 0xffffff_color,
                 .text_05 = 0x8d8d8d_color,
                 .text_error = 0xffb3b8_color,

                 .icon_01 = 0xf4f4f4_color,
                 .icon_02 = 0xc6c6c6_color,
                 .icon_03 = 0xffffff_color,

                 .field_01 = 0x393939_color,
                 .field_02 = 0x525252_color,

                 .inverse_01 = 0x161616_color,
                 .inverse_02 = 0xf4f4f4_color,

                 .support_01 = 0xff8389_color,
                 .support_02 = 0x42be65_color,
                 .support_03 = 0xf1c21b_color,
                 .support_04 = 0x4589ff_color,

                 .inverse_support_01 = 0xda1e28_color,
                 .inverse_support_02 = 0x24a148_color,
                 .inverse_support_03 = 0xf1c21b_color,
                 .inverse_support_04 = 0x0043ce_color,

                 .monochromatic_palette = {
                         0xf6f2ff_color,
                         0xe8daff_color,
                         0xd4bbff_color,
                         0xbe95ff_color,
                         0xa56eff_color,
                         0x8a3ffc_color,
                         0x6929c4_color,
                         0x491d8b_color,
                         0x31135e_color,
                         0x1c0f30_color,
                 }};
}

} // namespace dplx::dlog::tui
