
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <ftxui/component/component_base.hpp>
#include <ftxui/component/event.hpp>

#include <dplx/dlog/core/log_clock.hpp>
#include <dplx/dlog/core/strong_types.hpp>
#include <dplx/dlog/record_container.hpp>
#include <dplx/dlog/tui/theme.hpp>

namespace dplx::dlog::tui
{

struct severity_info
{
    std::string name;
    ftxui::Decorator severity_decorator;
    ftxui::Decorator line_decorator;
};

class LogDisplayGridComponent final : public ftxui::ComponentBase
{
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    std::vector<record *> &mRecords;
    std::size_t mSelected;
    std::size_t mLastLines;
    ftxui::Box mDisplayBox;
    ftxui::Box mSelectedRowBox;

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    log_clock::epoch_info &mDisplayEpoch;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    theme const &mCurrentTheme;

    std::vector<severity_info> mSeverities;

public:
    LogDisplayGridComponent(std::vector<record *> &records,
                            log_clock::epoch_info &displayEpoch,
                            theme const &currentTheme)
        : ComponentBase()
        , mRecords(records)
        , mSelected(0)
        , mLastLines(0)
        , mDisplayBox{.x_min = 0,
                      .x_max = INT_MAX,
                      .y_min = 0,
                      .y_max = INT_MAX}
        , mSelectedRowBox{}
        , mDisplayEpoch(displayEpoch)
        , mCurrentTheme(currentTheme)
        , mSeverities{derive_severity_infos(mCurrentTheme)}
    {
    }

    static auto derive_severity_infos(theme const &t)
            -> std::vector<severity_info>;

    auto OnRender() -> ftxui::Element override;
    auto OnEvent(ftxui::Event event) -> bool override;

    [[nodiscard]] auto Focusable() const -> bool override;
    [[nodiscard]] auto ActiveChild() -> ftxui::Component override;
};

} // namespace dplx::dlog::tui
