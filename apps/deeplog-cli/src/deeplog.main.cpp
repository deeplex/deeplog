
#include <ranges>

#include <fmt/format.h>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <parallel_hashmap/phmap.h>

#include <dplx/dlog/argument_transmorpher_fmt.hpp>
#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/detail/file_stream.hpp>
#include <dplx/dlog/detail/iso8601.hpp>
#include <dplx/dlog/file_database.hpp>
#include <dplx/dlog/record_container.hpp>
#include <dplx/dlog/tui/log_display_grid.hpp>
#include <dplx/dlog/tui/theme.hpp>

namespace dplx::dlog::tui
{

auto current_theme = theme_carbon_grey90();

struct options
{
    phmap::flat_hash_map<std::string, bool> enabled_containers;
    log_clock::epoch_info display_epoch;
};

class OptionsComponent : public ftxui::ComponentBase
{
    file_database_handle &mFileDb;
    options &mValue;

    ftxui::Component mFileSelection;

    phmap::flat_hash_map<std::string, bool> mEnabledContainersBuilder;

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

        phmap::flat_hash_map<file_sink_id, ftxui::Component> resourcesView;

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

            ftxui::Component *conti;
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
        for (auto &sink : resourcesView)
        {
            auto name = fmt::format("sink {}", sink.first);
            mFileSelection->Add(ftxui::Renderer(
                    sink.second,
                    [name = std::move(name), component = sink.second] {
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

    phmap::flat_hash_map<std::string, record_container> mClosedContainers;

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

        auto joined
                = mClosedContainers | std::views::values
                | std::views::transform([](record_container &container)
                                                -> std::pmr::vector<record> &
                                        { return container.records; })
                | std::views::join
                | std::views::transform([](record &v) -> record *
                                        { return &v; });

        mDisplayRecords.assign(joined.begin(), joined.end());
        std::ranges::stable_sort(mDisplayRecords, [](record *l, record *r)
                                 { return l->timestamp < r->timestamp; });
    }

    auto Render() -> ftxui::Element override
    {
        return mLogGrid->Render();
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

        dlog::argument_transmorpher argumentTransmorpher;
        dlog::record_attribute_reviver parseAttrs;
        (void)parseAttrs.register_attribute<attr::file>();
        (void)parseAttrs.register_attribute<attr::line>();

        dp::basic_decoder<record> decode_record{argumentTransmorpher,
                                                parseAttrs};
        dp::basic_decoder<record_container> decode{decode_record};

        record_container value;
        dp::parse_context ctx{inStream};
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

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
{
    using namespace ftxui;
    using namespace dplx;
    namespace llfio = dlog::llfio;

    if (argc < 2)
    {
        return -1;
    }

    llfio::path_view dbPath(argv[1]);
    auto db = dlog::file_database_handle::file_database({}, dbPath, "").value();

    auto screen = ScreenInteractive::Fullscreen();
    auto mainComponent
            = std::make_shared<dlog::tui::MainComponent>(std::move(db));
    screen.Loop(mainComponent);
}
