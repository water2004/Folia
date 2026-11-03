#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "visual_window.h"
#include "settings/SettingsView.h"
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
        auto view = std::make_shared<winrt::Folia::SettingsView>(winrt::Folia::AppSettings{},
            std::make_shared<winrt::Folia::ThemeCatalog>(), latex, folia::Theme::Light,
            nullptr, [](auto const&) -> std::optional<winrt::hstring> { return {}; }, [] {});
        auto elapsed = [&] { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count(); };
        timings << "settings construction ms=" << elapsed() << ", latex commands=" << latex->Commands().size() << std::endl;
        struct Cleanup { Window window; std::shared_ptr<winrt::Folia::SettingsView> view;
            ~Cleanup() { view->Detach(); window.Close(); } } cleanup{window, view};
        root.Children().Append(view->Root()); window.AppWindow().Resize({1120, 900}); window.Activate();
        Pump([&] { return root.IsLoaded(); });
        auto navigation = view->Root();
        auto select = [&](wchar_t const* tag) {
            for (auto entry : navigation.MenuItems())
                if (winrt::unbox_value<winrt::hstring>(entry.as<NavigationViewItem>().Tag()) == tag)
                    navigation.SelectedItem(entry);
            root.UpdateLayout();
            if (auto list = First<ListView>(navigation.Content().as<UIElement>())) {
                Pump([&] {
                    root.UpdateLayout();
                    auto content = Row(list, 0);
                    return content && content.IsLoaded() && content.ActualHeight() > 0.0;
                });
            }
        };
        for (auto const* tag : {L"shortcuts", L"latex", L"shortcuts", L"latex"}) {
            started = std::chrono::steady_clock::now();
            select(tag);
            auto page = navigation.Content().as<FrameworkElement>();
            timings << winrt::to_string(tag) << " switch/layout ms=" << elapsed()
                << ", grids=" << Count<Grid>(page) << ", combos=" << Count<ComboBox>(page)
                << ", textblocks=" << Count<TextBlock>(page) << std::endl;
            expect(page.ActualHeight() > 0.0);
            auto list = First<ListView>(page);
            expect(list != nullptr);
            if (!list) return;
            expect(Realized(list) > 0_u);
            if (std::wstring_view(tag) == L"shortcuts") expect(Count<ComboBox>(page) > 0_u);
            expect(Realized(list) < list.Items().Size()) << "only the viewport and a small cache should be realized";
            timings << "realized=" << Realized(list) << '/' << list.Items().Size() << std::endl;
            auto source = list.ItemsSource();
            auto first = Row(list, 0);
            view->Reset(winrt::Folia::AppSettings{}, folia::Theme::Light);
            select(L"general"); select(tag);
            expect(source == list.ItemsSource()) << "unchanged model must reuse its data source";
            expect(first == Row(list, 0)) << "unchanged navigation must reuse its controls";
        }
        auto list = First<ListView>(navigation.Content().as<UIElement>());
        auto last = list.Items().Size() - 1;
        auto source = list.ItemsSource();
        std::vector<winrt::weak_ref<FrameworkElement>> oldRows;
        for (std::uint32_t index = 0; index < list.Items().Size(); ++index)
            if (auto row = Row(list, index)) oldRows.push_back(winrt::make_weak(row));
        int recycled = 0;
        auto recycling = list.ContainerContentChanging(winrt::auto_revoke, [&](auto const&, ContainerContentChangingEventArgs const& args) {
            if (args.InRecycleQueue()) ++recycled;
        });
        for (auto index : {last, std::uint32_t{0}, last, std::uint32_t{0}}) {
            list.ScrollIntoView(list.Items().GetAt(index), ScrollIntoViewAlignment::Leading);
            root.UpdateLayout();
            Pump([&] {
                root.UpdateLayout();
                auto content = Row(list, index);
                return content && content.IsLoaded() && content.ActualHeight() > 0.0;
            });
            expect(Realized(list) < list.Items().Size() / 2);
            expect(Row(list, index).try_as<Border>() != nullptr)
                << "recycled row must show the command UI, never a boxed index";
        }
        // WinUI may retain its focused/anchor item. Other offscreen content
        // must be released, and the total realized count above stays bounded.
        expect(recycled > 0_i);
        expect(std::ranges::any_of(oldRows, [](auto const& row) { return !row.get(); }))
            << "scroll recycling releases old command visuals";
        expect(source == list.ItemsSource()) << "scrolling must not rebuild the data source";
        // Recording usage changes the catalog, but revisiting an unchanged
        // catalog above did not. No user configuration or disk write is needed.
        latex->RecordUsage(latex->Commands().front().id);
        select(L"general"); select(L"latex");
        expect(source != list.ItemsSource()) << "usage changes invalidate command descriptions";
        auto shortcuts = [&] { select(L"shortcuts"); return First<ListView>(navigation.Content().as<UIElement>()); }();
        auto row = Row(shortcuts, 0);
        auto capture = First<Button>(row);
        expect(capture != nullptr);
        if (capture) {
            auto label = winrt::unbox_value<winrt::hstring>(capture.Content());
            using namespace winrt::Microsoft::UI::Xaml::Automation;
            Peers::ButtonAutomationPeer(capture).GetPattern(Peers::PatternInterface::Invoke).as<Provider::IInvokeProvider>().Invoke();
            Pump([&] { return winrt::unbox_value<winrt::hstring>(capture.Content()) == winrt::Folia::Localize(L"PressShortcut"); });
            select(L"general");
            expect(winrt::unbox_value<winrt::hstring>(capture.Content()) == label) << "leaving shortcuts cancels capture";
        }
        // Keep the timing fixture's physical size comparable to its baseline;
        // use DPI-aware logical sizes for separate layout artifacts.
        HWND handle{}; winrt::check_hresult(window.as<::IWindowNative>()->get_WindowHandle(&handle));
        auto scale = double(GetDpiForWindow(handle)) / 96.;
        root.RequestedTheme(ElementTheme::Light);
        root.Background(Application::Current().Resources().Lookup(winrt::box_value(L"ApplicationPageBackgroundThemeBrush")).as<Media::Brush>());
        for (int width : {1120, 720}) {
            window.AppWindow().Resize({int(width * scale), int(900 * scale)});
            Pump([&] { return std::abs(root.ActualWidth() - width) < 20.; });
            for (auto const* tag : {L"shortcuts", L"latex"}) {
                select(tag);
                // Settle native item/selection transitions only for PNGs;
                // timings and functional assertions above do not wait for this.
                auto settled = std::chrono::steady_clock::now() + std::chrono::milliseconds(350);
                Pump([&] { return std::chrono::steady_clock::now() >= settled; });
                Media::Imaging::RenderTargetBitmap image;
                Await(image.RenderAsync(root));
                auto pixels = Await(image.GetPixelsAsync());
                visual_tests::SavePng(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path()
                    / "build/tests/settings-lists" / (winrt::to_string(tag) + '-' + std::to_string(width) + ".png"),
                    image.PixelWidth(), image.PixelHeight(), pixels);
            }
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
        auto view = std::make_shared<winrt::Folia::SettingsView>(settings,
            std::make_shared<winrt::Folia::ThemeCatalog>(), std::make_shared<winrt::Folia::LatexCommandCatalog>(),
            folia::Theme::Light, nullptr, [&](auto const& value) -> std::optional<winrt::hstring> {
                ++applied;
                if (failApply) return L"test persistence failure";
                saved = value; return {};
            }, [] {});
        struct Cleanup { Window window; std::shared_ptr<winrt::Folia::SettingsView> view;
            ~Cleanup() { view->Detach(); window.Close(); } } cleanup{window, view};
        root.Children().Append(view->Root()); window.AppWindow().Resize({1120, 900}); window.Activate();
        Pump([&] { return root.IsLoaded(); });
        auto navigation = view->Root();
        for (auto entry : navigation.MenuItems())
            if (winrt::unbox_value<winrt::hstring>(entry.as<NavigationViewItem>().Tag()) == L"shortcuts") navigation.SelectedItem(entry);
        root.UpdateLayout();
        auto list = First<ListView>(navigation.Content().as<UIElement>());
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
