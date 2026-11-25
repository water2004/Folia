#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "visual_window.h"
#include "settings/views/SettingsView.h"
#include "localization/Localization.h"
#include <winrt/Microsoft.UI.Xaml.Automation.Peers.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Provider.h>
#include <microsoft.ui.xaml.window.h>

namespace {
using namespace boost::ut;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace document_tests;

template<class T> std::size_t Count(DependencyObject const& root) {
    std::size_t count = root.try_as<T>() ? 1 : 0;
    for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(root); ++i)
        count += Count<T>(Media::VisualTreeHelper::GetChild(root, i));
    return count;
}
template<class T> T First(DependencyObject const& root) {
    if (auto value = root.try_as<T>()) return value;
    for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(root); ++i)
        if (auto value = First<T>(Media::VisualTreeHelper::GetChild(root, i))) return value;
    return nullptr;
}
FrameworkElement Row(ListView const& list, std::uint32_t index) {
    auto container = list.ContainerFromIndex(index).try_as<ListViewItem>();
    auto presenter = container ? container.ContentTemplateRoot().try_as<ContentPresenter>() : nullptr;
    return presenter ? presenter.Content().try_as<FrameworkElement>() : nullptr;
}
std::size_t Realized(ListView const& list) {
    std::size_t count = 0;
    for (std::uint32_t i = 0; i < list.Items().Size(); ++i) if (list.ContainerFromIndex(i)) ++count;
    return count;
}

suite settings_list_tests = [] {
    "settings lists realize only viewport rows and reuse unchanged pages"_test = [] {
        Window window; Grid root; window.Content(root); window.Title(L"Folia settings list test");
        auto latex = std::make_shared<winrt::Folia::LatexCommandCatalog>();
        std::ofstream timings(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path()
            / "build/settings-lists-timing.log");
        auto started = std::chrono::steady_clock::now();
        ContentControl pageHost;
        auto view = std::make_shared<winrt::Folia::SettingsView>(pageHost, winrt::Folia::AppSettings{},
            std::make_shared<winrt::Folia::ThemeCatalog>(), latex, folia::Theme::Light,
            nullptr, [](auto const&) -> std::optional<winrt::hstring> { return {}; });
        auto elapsed = [&] { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count(); };
        timings << "settings construction ms=" << elapsed() << ", latex commands=" << latex->Commands().size() << std::endl;
        struct Cleanup { Window window; std::shared_ptr<winrt::Folia::SettingsView> view;
            ~Cleanup() { view->Detach(); window.Close(); } } cleanup{window, view};
        root.Children().Append(pageHost);
        window.Activate();
        Pump([&] { return root.IsLoaded(); });
        const auto scale = root.XamlRoot().RasterizationScale();
        window.AppWindow().Resize({static_cast<int>(1120 * scale), static_cast<int>(900 * scale)});
        Pump([&] { return root.ActualHeight() >= 700; });
        using Page = winrt::Folia::SettingsView::Page;
        auto select = [&](Page page) {
            view->Navigate(page);
            root.UpdateLayout();
            if (auto list = First<ListView>(view->PageContent())) {
                list.ScrollIntoView(list.Items().GetAt(0), ScrollIntoViewAlignment::Leading);
                Pump([&] { auto row = Row(list, 0); return row && row.IsLoaded() && row.ActualHeight() > 0.0; });
            }
        };
        for (auto category : {Page::Shortcuts, Page::Latex, Page::Shortcuts, Page::Latex}) {
            started = std::chrono::steady_clock::now();
            select(category);
            auto page = view->PageContent().as<FrameworkElement>();
            timings << (category == Page::Shortcuts ? "shortcuts" : "latex") << " switch/layout ms=" << elapsed()
                << ", grids=" << Count<Grid>(page) << ", combos=" << Count<ComboBox>(page)
                << ", textblocks=" << Count<TextBlock>(page) << ", page height=" << page.ActualHeight() << std::endl;
            expect(page.ActualHeight() > 0.0);
            auto list = First<ListView>(page);
            expect(list != nullptr);
            if (!list) return;
            expect(Realized(list) > 0_u);
            if (category == Page::Shortcuts) expect(Count<ComboBox>(page) > 0_u);
            expect(Realized(list) < list.Items().Size()) << "only the viewport and a small cache should be realized";
            timings << "realized=" << Realized(list) << '/' << list.Items().Size() << std::endl;
            auto source = list.ItemsSource();
            auto first = Row(list, 0);
            view->Reset(winrt::Folia::AppSettings{}, folia::Theme::Light);
            select(Page::General); select(category);
            expect(source == list.ItemsSource()) << "unchanged model must reuse its data source";
            expect(first == Row(list, 0)) << "unchanged navigation must reuse its controls";
        }
        auto list = First<ListView>(view->PageContent());
        auto last = list.Items().Size() - 1;
        auto source = list.ItemsSource();
        Pump([&] { return Realized(list) > 1u; });
        std::vector<winrt::weak_ref<FrameworkElement>> oldRows;
        for (std::uint32_t index = 0; index < list.Items().Size(); ++index)
            if (auto row = Row(list, index)) oldRows.push_back(winrt::make_weak(row));
        int recycled = 0;
        auto recycling = list.ContainerContentChanging(winrt::auto_revoke, [&](auto const&, ContainerContentChangingEventArgs const& args) {
            if (args.InRecycleQueue()) ++recycled;
        });
        bool releasedOffscreenRow = false;
        for (auto index : {last, std::uint32_t{0}, last, std::uint32_t{0}}) {
            list.ScrollIntoView(list.Items().GetAt(index), ScrollIntoViewAlignment::Leading);
            root.UpdateLayout();
            Pump([&] {
                auto content = Row(list, index);
                return content && content.IsLoaded() && content.ActualHeight() > 0.0;
            });
            expect(Realized(list) < list.Items().Size() / 2);
            expect(Row(list, index).try_as<Border>() != nullptr)
                << "recycled row must show the command UI, never a boxed index";
            if (index == last) releasedOffscreenRow |= std::ranges::any_of(oldRows, [](auto const& row) { return !row.get(); });
        }
        // WinUI may retain its focused/anchor item. Other offscreen content
        // must be released, and the total realized count above stays bounded.
        expect(recycled > 0_i);
        expect(releasedOffscreenRow) << "scroll recycling releases old command visuals";
        expect(source == list.ItemsSource()) << "scrolling must not rebuild the data source";
        // Recording usage changes the catalog, but revisiting an unchanged
        // catalog above did not. No user configuration or disk write is needed.
        latex->RecordUsage(latex->Commands().front().id);
        select(Page::General); select(Page::Latex);
        expect(source != list.ItemsSource()) << "usage changes invalidate command descriptions";
        auto shortcuts = [&] { select(Page::Shortcuts); return First<ListView>(view->PageContent()); }();
        auto row = Row(shortcuts, 0);
        auto capture = First<Button>(row);
        expect(capture != nullptr);
        if (capture) {
            auto label = winrt::unbox_value<winrt::hstring>(capture.Content());
            using namespace winrt::Microsoft::UI::Xaml::Automation;
            Peers::ButtonAutomationPeer(capture).GetPattern(Peers::PatternInterface::Invoke).as<Provider::IInvokeProvider>().Invoke();
            Pump([&] { return winrt::unbox_value<winrt::hstring>(capture.Content()) == winrt::Folia::Localize(L"PressShortcut"); });
            select(Page::General);
            expect(winrt::unbox_value<winrt::hstring>(capture.Content()) == label) << "leaving shortcuts cancels capture";
        }
    };
    "virtual shortcut controls preserve conflicts and failed-save rollback"_test = [] {
        winrt::Folia::AppSettings settings;
        settings.shortcutBindings.resize(2);
        settings.shortcutBindings[1].gesture = settings.shortcutBindings[0].gesture;
        settings.shortcutBindings[1].scope = folia::platform::editor::EditorShortcutScope::Code;
        int applied = 0;
        bool failApply = false;
        auto saved = settings;
        Window window; Grid root; window.Content(root);
        ContentControl pageHost;
        auto view = std::make_shared<winrt::Folia::SettingsView>(pageHost, settings,
            std::make_shared<winrt::Folia::ThemeCatalog>(), std::make_shared<winrt::Folia::LatexCommandCatalog>(),
            folia::Theme::Light, nullptr, [&](auto const& value) -> std::optional<winrt::hstring> {
                ++applied;
                if (failApply) return L"test persistence failure";
                saved = value; return {};
            });
        struct Cleanup { Window window; std::shared_ptr<winrt::Folia::SettingsView> view;
            ~Cleanup() { view->Detach(); window.Close(); } } cleanup{window, view};
        root.Children().Append(pageHost); window.AppWindow().Resize({1120, 900}); window.Activate();
        Pump([&] { return root.IsLoaded(); });
        view->Navigate(winrt::Folia::SettingsView::Page::Shortcuts);
        root.UpdateLayout();
        auto list = First<ListView>(view->PageContent());
        Pump([&] { root.UpdateLayout(); return Row(list, 0) && First<ComboBox>(Row(list, 0)); });
        auto scope = First<ComboBox>(Row(list, 0));
        auto source = list.ItemsSource();
        scope.SelectedIndex(1); // Same gesture already exists in Code scope.
        expect(applied == 0_i);
        expect(scope.SelectedIndex() == 0_i);
        expect(source == list.ItemsSource());
        scope.SelectedIndex(2); // Math scope is available.
        root.UpdateLayout();
        Pump([&] { return Row(list, 0) && First<ComboBox>(Row(list, 0)); });
        expect(applied == 1_i);
        expect(saved.shortcutBindings[0].scope == folia::platform::editor::EditorShortcutScope::Math);
        expect(source != list.ItemsSource());
        scope = First<ComboBox>(Row(list, 0));
        expect(scope.SelectedIndex() == 2_i);
        source = list.ItemsSource(); failApply = true;
        scope.SelectedIndex(0);
        expect(applied == 2_i);
        expect(scope.SelectedIndex() == 2_i);
        expect(source == list.ItemsSource());
    };
};
}
