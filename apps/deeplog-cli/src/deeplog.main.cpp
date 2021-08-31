
#include <ranges>

#include <fmt/format.h>

#include <parallel_hashmap/phmap.h>

#include <dplx/dlog/definitions.hpp>
#include <dplx/dlog/file_database.hpp>
#include <dplx/dlog/record_container.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>

namespace dplx::dlog::tui
{

struct options
{
    phmap::flat_hash_map<std::string, bool> enabled_containers;
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

class LogDisplayGridComponent : public ftxui::ComponentBase
{
    std::vector<record> &mRecords;
    std::size_t mSelected;

    ftxui::Elements mLevel;

public:
    LogDisplayGridComponent(std::vector<record> &records)
        : ComponentBase()
        , mRecords(records)
        , mSelected(0)
        , mLevel{
                  ftxui::text(" N/A ") | ftxui::color(ftxui::Color::GrayLight),
                  ftxui::text("CRIT") | ftxui::color(ftxui::Color::Red),
                  ftxui::text("ERROR") | ftxui::color(ftxui::Color::RedLight),
                  ftxui::text("WARN") | ftxui::color(ftxui::Color::Yellow),
                  ftxui::text("info") | ftxui::color(ftxui::Color::Blue),
                  ftxui::text("debug") | ftxui::color(ftxui::Color::Cyan),
                  ftxui::text("trace") | ftxui::color(ftxui::Color::White),
                  ftxui::text("INVAL") | ftxui::color(ftxui::Color::Violet),
          }
    {
    }

    auto Render() -> ftxui::Element override
    {
    }
};

class LogDisplayComponent : public ftxui::ComponentBase
{
    file_database_handle &mFileDb;
    options &mOptions;

    std::vector<record> mDisplayRecords;
    std::shared_ptr<LogDisplayGridComponent> mLogGrid;

public:
    LogDisplayComponent(file_database_handle &fileDb, options &opts)
        : ComponentBase()
        , mFileDb(fileDb)
        , mOptions(opts)
        , mLogGrid(std::make_shared<LogDisplayGridComponent>(mDisplayRecords))
    {
        Add(mLogGrid);
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
        }

        return ftxui::vbox({mTabToggle->Render(), ftxui::separator(), detail});
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
