
// Copyright Henrik Steffen Gaßmann 2021
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include "dplx/dlog/tui/log_display_grid.hpp"

#include <fmt/chrono.h>
#include <fmt/format.h>
#include <ftxui/component/animation.hpp>
#include <ftxui/dom/elements.hpp>

namespace dplx::dlog::tui
{

auto LogDisplayGridComponent::derive_severity_infos(theme const &t)
        -> std::vector<severity_info>
{
    using namespace std::string_literals;

    auto fatalColor = [&]
    {
        return color(t.inverse_support_01) | bgcolor(t.inverse_02);
    };
    return {
            {   "N/A"s, color(t.text_error),   ftxui::nothing},
            { "Trace"s,    color(t.text_03), color(t.text_03)},
            {"Trace2"s,    color(t.text_03), color(t.text_03)},
            {"Trace3"s,    color(t.text_03), color(t.text_03)},
            {"Trace4"s,    color(t.text_03), color(t.text_03)},
            { "Debug"s,    color(t.text_02),   ftxui::nothing},
            {"Debug2"s,    color(t.text_02),   ftxui::nothing},
            {"Debug3"s,    color(t.text_02),   ftxui::nothing},
            {"Debug4"s,    color(t.text_02),   ftxui::nothing},
            {  "Info"s, color(t.support_04),   ftxui::nothing},
            { "Info2"s, color(t.support_04),   ftxui::nothing},
            { "Info3"s, color(t.support_04),   ftxui::nothing},
            { "Info4"s, color(t.support_04),   ftxui::nothing},
            {  "WARN"s, color(t.support_03),   ftxui::nothing},
            { "WARN2"s, color(t.support_03),   ftxui::nothing},
            { "WARN3"s, color(t.support_03),   ftxui::nothing},
            { "WARN4"s, color(t.support_03),   ftxui::nothing},
            { "ERROR"s, color(t.support_01), color(t.text_01)},
            {"ERROR2"s, color(t.support_01), color(t.text_01)},
            {"ERROR3"s, color(t.support_01), color(t.text_01)},
            {"ERROR4"s, color(t.support_01), color(t.text_01)},
            { "FATAL"s,        fatalColor(), color(t.text_01)},
            {"FATAL2"s,        fatalColor(), color(t.text_01)},
            {"FATAL3"s,        fatalColor(), color(t.text_01)},
            {"FATAL4"s,        fatalColor(), color(t.text_01)},
            {"INVDAT"s, color(t.text_error),   ftxui::nothing},
    };
}

static auto compute_render_window(std::size_t selected,
                                  std::size_t numElements,
                                  std::size_t lines)
        -> std::pair<std::size_t, std::size_t>
{
    if (numElements <= lines)
    {
        return {0U, numElements};
    }
    auto const lineSplit = (lines + 1U) / 2U;
    if (selected <= lineSplit)
    {
        return {0U, lines};
    }
    if (lineSplit >= numElements - selected)
    {
        return {numElements - lines, numElements};
    }
    return {selected - lineSplit, selected + lineSplit};
}

auto LogDisplayGridComponent::Render() -> ftxui::Element
{
    constexpr int layout_size_level = 6;
    constexpr int iso8601_datetime_long_size = 26;
    constexpr int layout_size_timestamp = iso8601_datetime_long_size;

    auto spaceSeperator = ftxui::separator(ftxui::Pixel{});

    auto header = ftxui::hbox({
            ftxui::text("Timestamp")
                    | ftxui::size(ftxui::WIDTH, ftxui::EQUAL,
                                  layout_size_timestamp),
            spaceSeperator,
            ftxui::text("Level")
                    | ftxui::size(ftxui::WIDTH, ftxui::EQUAL,
                                  layout_size_level),
            spaceSeperator,
            ftxui::text("Message") | ftxui::flex,
    });

    std::string previousTime;
    ftxui::Elements formattedRecords;

    auto lines = static_cast<std::size_t>(mDisplayBox.y_max - mDisplayBox.y_min)
               + 2U;
    if (lines > mLastLines)
    {
        lines = lines * 3 / 2;
    }
    for (auto [i, limit]
         = tui::compute_render_window(mSelected, mRecords.size(), lines);
         i < limit; ++i)
    {
        auto const &record = *mRecords[i];
        auto const focused = i == mSelected;

        auto const normalizedLevel
                = std::min<unsigned>(cncr::to_underlying(record.severity), 24U);

        auto const &[severityName, severityFormat, lineDecoratorBase]
                = mSeverities[normalizedLevel];

        ftxui::Element formattedLevel
                = ftxui::text(severityName) | severityFormat
                | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, layout_size_level);
        ftxui::Decorator lineDecorator = lineDecoratorBase;
        if (focused)
        {
            if (Focused())
            {
                lineDecorator = lineDecorator | ftxui::focus | ftxui::inverted;
            }
            else
            {
                lineDecorator = lineDecorator | ftxui::focus;
            }
            lineDecorator = lineDecorator | ftxui::reflect(mSelectedRowBox);
        }

        auto sysTime
                = mDisplayEpoch.to_sys<std::chrono::system_clock::duration>(
                        record.timestamp);
        auto iso8601DateTime = fmt::format(
                "{:%FT%T}",
                std::chrono::floor<std::chrono::microseconds>(sysTime));

        auto mismatch
                = std::ranges::mismatch(iso8601DateTime, previousTime).in1;

        auto iso8601DateTimeUnchanged
                = std::string(iso8601DateTime.begin(), mismatch);
        auto iso8601DateTimeChanged
                = std::string(mismatch, iso8601DateTime.end());

        std::string formattedMessage;
        try
        {
            formattedMessage
                    = fmt::vformat(record.message, record.format_arguments);
        }
        catch (std::exception const &e)
        {
            formattedMessage = e.what();
        }

        auto formattedRecord
                = ftxui::hbox({
                          ftxui::hbox({
                                  ftxui::text(
                                          std::move(iso8601DateTimeUnchanged))
                                          | color(mCurrentTheme.text_03),
                                  ftxui::text(std::move(iso8601DateTimeChanged))
                                          | color(mCurrentTheme.text_02),
                          })
                                  | ftxui::size(ftxui::WIDTH, ftxui::EQUAL,
                                                layout_size_timestamp)
                                  | ftxui::notflex,
                          spaceSeperator,
                          formattedLevel,
                          spaceSeperator,
                          ftxui::text(formattedMessage) | ftxui::flex,
                  })
                | lineDecorator;

        previousTime = std::move(iso8601DateTime);
        formattedRecords.push_back(std::move(formattedRecord));
    }
    if (lines != mLastLines)
    {
        mLastLines = lines;
        ftxui::animation::RequestAnimationFrame();
    }

    return ftxui::vbox({header, ftxui::separator(),
                        ftxui::vbox(formattedRecords) | ftxui::yframe
                                | ftxui::reflect(mDisplayBox)});
}

auto LogDisplayGridComponent::OnEvent(ftxui::Event event) -> bool
{
    if (!Focused())
    {
        return false;
    }
    if (event.is_mouse() && event.mouse().button == ftxui::Mouse::Button::None)
    {
        return false;
    }

    auto const pageSize
            = static_cast<unsigned>(mDisplayBox.y_max - mDisplayBox.y_min) + 1;

    auto prev = mSelected;
    if (event == ftxui::Event::ArrowUp && mSelected != 0U)
    {
        mSelected -= 1U;
    }
    else if (event == ftxui::Event::ArrowDown)
    {
        mSelected += 1U;
    }
    else if (event == ftxui::Event::Home)
    {
        mSelected = 0U;
    }
    else if (event == ftxui::Event::End)
    {
        mSelected = std::numeric_limits<std::size_t>::max();
    }
    else if (event == ftxui::Event::PageUp)
    {
        mSelected = mSelected < pageSize ? 0U : mSelected - pageSize;
    }
    else if (event == ftxui::Event::PageDown)
    {
        mSelected += pageSize;
    }

    mSelected = std::min(mSelected,
                         !mRecords.empty() ? mRecords.size() - 1U : 0U);
    return mSelected != prev;
}

auto LogDisplayGridComponent::Focusable() const -> bool
{
    return !mRecords.empty();
}

auto LogDisplayGridComponent::ActiveChild() -> ftxui::Component
{
    return nullptr;
}

} // namespace dplx::dlog::tui
