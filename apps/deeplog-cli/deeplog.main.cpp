
// Copyright 2021, 2023 Henrik Steffen Gaßmann
//
// Distributed under the Boost Software License, Version 1.0.
//         (See accompanying file LICENSE or copy at
//           https://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <numeric>
#include <ranges>

#include <boost/unordered/unordered_map.hpp>

#include <fmt/format.h>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>

#include <dplx/dp/codecs/core.hpp>
#include <dplx/dp/codecs/std-string.hpp>

#include <dplx/dlog/argument_transmorpher_fmt.hpp>
#include <dplx/dlog/core/file_database.hpp>
#include <dplx/dlog/detail/file_stream.hpp>
#include <dplx/dlog/record_container.hpp>
#include <dplx/dlog/tui/log_display_grid.hpp>
#include <dplx/dlog/tui/theme.hpp>

namespace dplx::dlog::tui
{

auto const current_theme = theme_carbon_grey90();

struct options
{
    boost::unordered_map<std::string, bool> enabled_containers;
    log_clock::epoch_info display_epoch;
};

class OptionsComponent : public ftxui::ComponentBase
{
    file_database_handle &mFileDb;
    options &mValue;

    ftxui::Component mFileSelection;

    boost::unordered_map<std::string, bool> mEnabledContainersBuilder;

public:
    OptionsComponent(file_database_handle &fileDb, options &value)
        : ComponentBase()
        , mFileDb(fileDb)
        , mValue(value)
        , mFileSelection(ftxui::Container::Vertical({}))
    {
        UpdateFileSelection();
        Add(ftxui::Container::Vertical({
                ftxui::Renderer(mFileSelection,
                                [this]
                                {
                                    return ftxui::window(
                                            ftxui::text("enabled sinks"),
                                            mFileSelection->Render());
                                }),
        }));
    }

    auto Render() -> ftxui::Element override
    {

        return ComponentBase::Render();
    }

private:
    void UpdateFileSelection()
    {
        auto const &recordContainers = mFileDb.record_containers();

        boost::unordered_map<file_sink_id, ftxui::Component> resourcesView;

        mEnabledContainersBuilder.clear();
        for (auto const &container : recordContainers)
        {
            bool enabled = true;
            auto key = container.path.generic_string();
            if (auto state = mValue.enabled_containers.find(key);
                state != mValue.enabled_containers.end())
            {
                enabled = state->second;
            }

            auto itsertet = mEnabledContainersBuilder
                                    .insert({std::move(key), enabled})
                                    .first;

            ftxui::Component *conti = nullptr;
            if (auto resIt = resourcesView.find(container.sink_id);
                resIt != resourcesView.end())
            {
                conti = &resIt->second;
            }
            else
            {
                conti = &resourcesView
                                 .insert({container.sink_id,
                                          ftxui::Container::Vertical({})})
                                 .first->second;
            }

            (*conti)->Add(ftxui::Checkbox(&itsertet->first, &itsertet->second));
        }

        mFileSelection->DetachAllChildren();
        for (auto &sinkView : resourcesView)
        {
            auto name = fmt::format("sink {}", sinkView.first);
            mFileSelection->Add(ftxui::Renderer(
                    sinkView.second,
                    [name = std::move(name), component = sinkView.second] {
                        return ftxui::window(ftxui::text(name),
                                             component->Render());
                    }));
        }

        std::ranges::swap(mValue.enabled_containers, mEnabledContainersBuilder);
    }
};

class LogDisplayComponent : public ftxui::ComponentBase
{
    file_database_handle &mFileDb;
    options &mOptions;

    boost::unordered_map<std::string, record_container> mClosedContainers;

    std::vector<record *> mDisplayRecords;
    std::shared_ptr<LogDisplayGridComponent> mLogGrid;

public:
    LogDisplayComponent(file_database_handle &fileDb, options &opts)
        : ComponentBase()
        , mFileDb(fileDb)
        , mOptions(opts)
        , mLogGrid(std::make_shared<LogDisplayGridComponent>(
                  mDisplayRecords, mOptions.display_epoch, current_theme))
    {
        Add(mLogGrid);

        for (auto const &container : fileDb.record_containers())
        {
            if (auto loadRx = LoadClosedContainer(container);
                loadRx.has_value())
            {
                mOptions.display_epoch
                        = loadRx.assume_value().info.epoch; // FIXME
                mClosedContainers.insert({container.path.generic_string(),
                                          std::move(loadRx).assume_value()});
            }
        }

        std::size_t const numRecords = std::transform_reduce(
                mClosedContainers.begin(), mClosedContainers.end(),
                std::size_t{}, std::plus<>{},
                [](auto const &pair) { return pair.second.records.size(); });
        mDisplayRecords.reserve(numRecords);
        for (auto &container : mClosedContainers)
        {
            auto &records = container.second.records;
            std::ranges::transform(records, std::back_inserter(mDisplayRecords),
                                   [](record &v) -> record * { return &v; });
        }
        std::ranges::stable_sort(mDisplayRecords, std::ranges::less{},
                                 [](record *r) { return r->timestamp; });
    }

    auto Render() -> ftxui::Element override
    {
        return mLogGrid->Render();
    }

    [[nodiscard]] auto Focusable() const -> bool override
    {
        return mLogGrid->Focusable();
    }
    [[nodiscard]] auto ActiveChild() -> ftxui::Component override
    {
        return mLogGrid;
    }

private:
    auto
    LoadClosedContainer(file_database_handle::record_container_meta const &meta)
            -> result<record_container>
    {
        DPLX_TRY(auto &&containerFile, mFileDb.open_record_container(meta));
        DPLX_TRY(auto const maxExtent, containerFile.maximum_extent());

        DPLX_TRY(auto &&inStream,
                 detail::os_input_stream::create(containerFile, maxExtent));

        dp::parse_context ctx(inStream);
        dp::scoped_state attributeTypeRegistryScope(
                ctx.states, attribute_type_registry_state);
        {
            auto *attributeTypeRegistry = attributeTypeRegistryScope.get();
            (void)attributeTypeRegistry->insert<attr::file>();
            (void)attributeTypeRegistry->insert<attr::line>();
        }

        dlog::argument_transmorpher argumentTransmorpher;
        DPLX_TRY(argumentTransmorpher
                         .register_type<dlog::detail::reified_status_code>());
        DPLX_TRY(argumentTransmorpher
                         .register_type<dlog::detail::reified_system_code>());

        dp::basic_decoder<record> decode_record{argumentTransmorpher};
        dp::basic_decoder<record_container> decode{decode_record};

        record_container value;
        DPLX_TRY(decode(ctx, value));

        return value;
    }
};

class MainComponent : public ftxui::ComponentBase
{
    file_database_handle mFileDb;

    options mOptions;

    int mTabSelector;
    std::vector<std::string> mTabs;
    ftxui::Component mTabToggle;

    ftxui::Component mOptionsComponent;
    ftxui::Component mLogDisplayComponent;

public:
    MainComponent(file_database_handle &&fileDb)
        : ComponentBase()
        , mFileDb(std::move(fileDb))
        , mOptions{}
        , mTabSelector(0)
        , mTabs{"Options", "Log"}
        , mTabToggle(ftxui::Toggle(&mTabs, &mTabSelector))
        , mOptionsComponent(
                  std::make_shared<OptionsComponent>(mFileDb, mOptions))
        , mLogDisplayComponent(
                  std::make_shared<LogDisplayComponent>(mFileDb, mOptions))
    {
        Add(ftxui::Container::Vertical(
                {mTabToggle, ftxui::Container::Tab(
                                     {
                                             mOptionsComponent,
                                             mLogDisplayComponent,
                                     },
                                     &mTabSelector)}));
    }

    auto Render() -> ftxui::Element override
    {
        ftxui::Element detail
                = ftxui::text("u broke the main component render :(");

        if (mTabSelector == 0)
        {
            detail = mOptionsComponent->Render();
        }
        else if (mTabSelector == 1)
        {
            detail = mLogDisplayComponent->Render();
        }

        return ftxui::vbox({mTabToggle->Render(), ftxui::separator(), detail})
             | color(current_theme.text_02)
             | bgcolor(current_theme.ui_background);
    }
};

} // namespace dplx::dlog::tui

auto main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) -> int
try
{
    using namespace ftxui;
    using namespace dplx;
    namespace llfio = dlog::llfio;

    std::span<char *> args(static_cast<char **>(argv),
                           static_cast<std::size_t>(argc));
    if (args.size() < 2)
    {
        return -3;
    }

    llfio::path_view dbPath(args[1]);
    auto db = dlog::file_database_handle::file_database({}, dbPath).value();

    auto screen = ScreenInteractive::Fullscreen();
    auto mainComponent
            = std::make_shared<dlog::tui::MainComponent>(std::move(db));
    screen.Loop(mainComponent);
}
catch (std::exception const &exc)
{
    fmt::print(stderr, "Unhandled exception: {}", exc.what());
    return -2;
}
catch (...)
{
    fmt::print(stderr, "The application failed due to an unknown exception");
    return -1;
}
