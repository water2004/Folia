#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "settings/views/SettingsView.h"
#include "visual_window.h"

namespace {
    using namespace boost::ut;
    using namespace document_tests;
    using namespace winrt::Microsoft::UI::Xaml;
    using namespace winrt::Microsoft::UI::Xaml::Controls;

    template<class T> T First(DependencyObject const& element, std::wstring_view name = {}) {
        if (auto value = element.try_as<T>(); value && (name.empty() || value.Name() == name)) return value;
        for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(element); ++i)
            if (auto value = First<T>(Media::VisualTreeHelper::GetChild(element, i), name)) return value;
        return nullptr;
    }

    suite settings_width_tests = [] {
        "settings center the shared content column and keep native list scrollbars at the page edge"_test = [] {
            Window window;
            Grid host, root;
            host.Children().Append(root);
            window.Content(host);
            root.RequestedTheme(ElementTheme::Light);
            root.Background(Application::Current().Resources().Lookup(
                winrt::box_value(L"ApplicationPageBackgroundThemeBrush")).as<Media::Brush>());
            // Exercise wide logical viewports independently of the monitor size.
            root.HorizontalAlignment(HorizontalAlignment::Left);
            root.VerticalAlignment(VerticalAlignment::Top);
            root.Width(1120);
            root.Height(800);
            NavigationView settingsMenu;
            auto view = std::make_shared<winrt::Folia::SettingsView>(settingsMenu, settingsMenu, winrt::Folia::AppSettings{},
                std::make_shared<winrt::Folia::ThemeCatalog>(), std::make_shared<winrt::Folia::LatexCommandCatalog>(),
                folia::Theme::Light, nullptr, [](auto const&) -> std::optional<winrt::hstring> { return {}; }, [] {});
            struct Cleanup {
                Window window;
                std::shared_ptr<winrt::Folia::SettingsView> view;
                ~Cleanup() { view->Detach(); window.Close(); }
            } cleanup{window, view};
            root.Children().Append(view->Root());
            window.Activate();
            Pump([&] { return root.IsLoaded(); });
            auto navigation = view->Root();
            for (auto entry : navigation.MenuItems()) {
                navigation.SelectedItem(entry);
                const auto name = winrt::to_string(winrt::unbox_value<winrt::hstring>(entry.as<NavigationViewItem>().Tag()));
                for (const auto width : {1120, 1800, 2400, 720, 1120}) {
                    root.Width(width);
                    Pump([&] { return std::abs(root.ActualWidth() - width) < 1.; });
                    root.UpdateLayout();
                    auto page = navigation.Content().as<FrameworkElement>();
                    Pump([&] { root.UpdateLayout(); return First<ContentPresenter>(page, L"PageContent") != nullptr; });
                    auto content = First<ContentPresenter>(page, L"PageContent");
                    const auto origin = content.TransformToVisual(page).TransformPoint({0, 0});
                    expect(std::abs(content.ActualWidth() - (std::min)(1008., page.ActualWidth())) < 2.) << name << width;
                    expect(std::abs(origin.X + content.ActualWidth() / 2 - page.ActualWidth() / 2) < 2.) << name << width;
                    if (name == "shortcuts" || name == "latex") {
                        auto list = First<ListView>(page);
                        Pump([&] { root.UpdateLayout(); return list.ContainerFromIndex(0) != nullptr; });
                        auto scroll = First<ScrollViewer>(list);
                        expect(fatal(scroll != nullptr));
                        const auto scrollPoint = scroll.TransformToVisual(page).TransformPoint({0, 0});
                        expect(std::abs(scrollPoint.X) < 2.);
                        expect(std::abs(scrollPoint.X + scroll.ActualWidth() - page.ActualWidth()) < 2.) << name;
                        expect(scroll.ActualHeight() > page.ActualHeight() - 48.) << name;
                        auto row = list.ContainerFromIndex(0).as<ListViewItem>().ContentTemplateRoot().as<ContentPresenter>();
                        const auto rowPoint = row.TransformToVisual(page).TransformPoint({0, 0});
                        expect(std::abs(row.ActualWidth() - content.ActualWidth() + 48.) < 2.) << name << width;
                        expect(std::abs(rowPoint.X - origin.X - 24.) < 2.) << name << width;
                    }
                }
                if (name == "themes") {
                    root.Width(1800);
                    root.Height(700);
                    root.UpdateLayout();
                    auto page = navigation.Content().as<FrameworkElement>();
                    auto list = First<ListView>(page);
                    const auto listHeight = list.ActualHeight();
                    root.Height(1000);
                    root.UpdateLayout();
                    expect(list.ActualHeight() > listHeight + 250.) << "theme list follows the viewport height";
                    auto button = First<Button>(page);
                    expect(fatal(button != nullptr));
                    for (const auto width : {1800, 720, 1800}) {
                        root.Width(width);
                        for (const auto height : {700, 480, 360, 700}) {
                            root.Height(height);
                            root.UpdateLayout();
                            for (auto const& action : button.Parent().as<StackPanel>().Children()) {
                                auto control = action.as<Button>();
                                const auto bounds = control.TransformToVisual(page).TransformBounds({0, 0,
                                    static_cast<float>(control.ActualWidth()), static_cast<float>(control.ActualHeight())});
                                expect(bounds.X >= 0 && bounds.X + bounds.Width <= page.ActualWidth() + 1.);
                                expect(bounds.Y >= 0 && bounds.Y + bounds.Height <= page.ActualHeight() + 1.)
                                    << "theme actions leave the viewport at " << width << 'x' << height;
                            }
                        }
                    }
                }
                // Save native rendering for the pages with distinct scroll layouts.
                if (name == "general" || name == "themes" || name == "shortcuts" || name == "latex") {
                    window.AppWindow().Resize({1600, 1000});
                    root.Width(host.ActualWidth());
                    root.Height(host.ActualHeight());
                    root.UpdateLayout();
                    Media::Imaging::RenderTargetBitmap image;
                    Await(image.RenderAsync(root));
                    visual_tests::SavePng(std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path()
                        / "build/tests/settings-layout" / (name + ".png"),
                        image.PixelWidth(), image.PixelHeight(), Await(image.GetPixelsAsync()));
                }
            }
        };
    };
}
