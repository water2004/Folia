#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "visual_window.h"
#include "localization/Localization.h"
#include "settings/SettingsView.h"
#include "theme/ThemeConfig.h"
#include <winrt/Microsoft.UI.Xaml.Automation.Peers.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Provider.h>
#include <microsoft.ui.xaml.window.h>

namespace {
using namespace boost::ut;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace document_tests;
template<class T> T First(DependencyObject const& parent) {
    if (auto value = parent.try_as<T>()) return value;
    for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(parent); ++i)
        if (auto found = First<T>(Media::VisualTreeHelper::GetChild(parent, i))) return found;
    return nullptr;
}
TextBlock Description(DependencyObject const& parent, winrt::hstring const& text) {
    if (auto value = parent.try_as<TextBlock>(); value && value.Text() == text) return value;
    for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(parent); ++i)
        if (auto found = Description(Media::VisualTreeHelper::GetChild(parent, i), text)) return found;
    return nullptr;
}
template<class T> T Field(DependencyObject const& parent, winrt::hstring const& label) {
    if (auto value = parent.try_as<T>(); value && value.Header()
        && winrt::unbox_value<winrt::hstring>(value.Header()) == label) return value;
    for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(parent); ++i)
        if (auto found = Field<T>(Media::VisualTreeHelper::GetChild(parent, i), label)) return found;
    return nullptr;
}
suite settings_style_tests = [] {
    "settings pages share typography and render AI forms without enclosing borders"_test = [] {
        Window window; Grid root; window.Content(root); window.Title(L"Folia settings style test");
        auto view = std::make_shared<winrt::Folia::SettingsView>(winrt::Folia::AppSettings{},
            std::make_shared<winrt::Folia::ThemeCatalog>(), std::make_shared<winrt::Folia::LatexCommandCatalog>(),
            folia::Theme::Light, nullptr, [](auto const&) -> std::optional<winrt::hstring> { return {}; }, [] {});
        struct Cleanup { Window window; std::shared_ptr<winrt::Folia::SettingsView> view;
            ~Cleanup() { view->Detach(); window.Close(); } } cleanup{window, view};
        root.Children().Append(view->Root()); window.Activate();
        Pump([&] { return root.IsLoaded(); });
        auto navigation = view->Root();
        for (auto entry : navigation.MenuItems()) {
            navigation.SelectedItem(entry); root.UpdateLayout();
            auto page = navigation.Content().as<FrameworkElement>();
            auto heading = First<TextBlock>(page); expect(heading != nullptr);
            if (heading) expect(heading.FontSize() == 24.0) << "page " << winrt::to_string(winrt::unbox_value<winrt::hstring>(entry.as<NavigationViewItem>().Tag()));
        }
        for (auto theme : {ElementTheme::Light, ElementTheme::Dark}) {
            auto profile = winrt::Folia::LoadThemeProfile(theme == ElementTheme::Light ? folia::Theme::Light : folia::Theme::Dark).profile;
            auto const& colors = profile.colors;
            auto brush = [&](wchar_t const* key, folia::Color color) {
                winrt::Windows::UI::Color value{color.a, color.r, color.g, color.b};
                auto resources = root.Resources();
                if (resources.HasKey(winrt::box_value(key))) resources.Lookup(winrt::box_value(key)).as<Media::SolidColorBrush>().Color(value);
                else resources.Insert(winrt::box_value(key), Media::SolidColorBrush(value));
            };
            brush(L"FoliaShellForegroundBrush", colors.shell_fg); brush(L"FoliaShellMutedForegroundBrush", colors.shell_muted_fg);
            brush(L"FoliaShellAccentBrush", colors.shell_accent); brush(L"FoliaShellLayerBrush", colors.shell_layer_bg);
            brush(L"FoliaShellBorderBrush", colors.shell_border);
            root.Resources().Insert(winrt::box_value(L"FoliaUiFontFamily"), Media::FontFamily(winrt::to_hstring(profile.typography.ui.family)));
            root.Resources().Insert(winrt::box_value(L"FoliaUiFontSize"), winrt::box_value(double(profile.typography.ui.size)));
            root.Resources().Insert(winrt::box_value(L"FoliaMonoFontFamily"), Media::FontFamily(winrt::to_hstring(profile.typography.ui_monospace.family)));
            root.Background(Media::SolidColorBrush({colors.shell_bg.a, colors.shell_bg.r, colors.shell_bg.g, colors.shell_bg.b}));
            root.RequestedTheme(theme);
            Pump([&] { return root.ActualTheme() == theme; }); root.UpdateLayout();
            for (auto entry : navigation.MenuItems())
                if (winrt::unbox_value<winrt::hstring>(entry.as<NavigationViewItem>().Tag()) == L"completion") navigation.SelectedItem(entry);
            for (int width : {1120, 720}) {
                HWND handle{}; winrt::check_hresult(window.as<::IWindowNative>()->get_WindowHandle(&handle));
                auto scale = double(GetDpiForWindow(handle)) / 96.;
                window.AppWindow().Resize({static_cast<int>(width * scale), static_cast<int>(900 * scale)});
                Pump([&] { return std::abs(root.ActualWidth() - width) < 20.; }); root.UpdateLayout();
                auto scroll = navigation.Content().as<ScrollViewer>();
                auto panel = scroll.Content().as<StackPanel>();
                for (auto child : panel.Children()) expect(!child.try_as<Border>()) << "AI sections must not use a page-sized frame";
                auto description = Description(panel, winrt::Folia::Localize(L"AICompletionDescription"));
                expect(description != nullptr);
                if (description) {
                    auto expected = root.Resources().Lookup(winrt::box_value(L"FoliaShellMutedForegroundBrush")).as<Media::SolidColorBrush>();
                    expect(description.Foreground().as<Media::SolidColorBrush>().Color() == expected.Color()) << "description theme brush";
                }
                auto endpoint = Field<TextBox>(panel, winrt::Folia::Localize(L"AIBaseUrl"));
                auto key = Field<PasswordBox>(panel, winrt::Folia::Localize(L"AIKey"));
                expect(endpoint != nullptr && key != nullptr);
                if (endpoint && key) {
                    expect(endpoint.ActualWidth() > 100.0) << "endpoint visible width " << endpoint.ActualWidth();
                    expect(std::abs(endpoint.ActualWidth() - key.ActualWidth()) < 1.0) << "aligned field widths";
                    auto a = endpoint.TransformToVisual(panel).TransformPoint({0, 0});
                    auto b = key.TransformToVisual(panel).TransformPoint({0, 0});
                    expect(std::abs(a.X - b.X) < 1.0);
                    expect(a.X + endpoint.ActualWidth() <= panel.ActualWidth() + 1.0);
                }
                // Settle native NavigationView selection/theme animations for
                // the artifact; functional layout assertions above are immediate.
                auto settled = std::chrono::steady_clock::now() + std::chrono::milliseconds(350);
                Pump([&] { return std::chrono::steady_clock::now() >= settled; });
                Media::Imaging::RenderTargetBitmap image; Await(image.RenderAsync(root));
                auto pixels = Await(image.GetPixelsAsync());
                auto name = std::string(theme == ElementTheme::Light ? "light-" : "dark-") + std::to_string(width) + ".png";
                visual_tests::SavePng(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path()
                    / "build/tests/settings-styles" / name, image.PixelWidth(), image.PixelHeight(), pixels);
            }
        }
    };
};
}
