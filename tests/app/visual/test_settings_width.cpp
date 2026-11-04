#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "settings/SettingsView.h"

namespace {
    using namespace boost::ut;
    using namespace document_tests;
    using namespace winrt::Microsoft::UI::Xaml;
    using namespace winrt::Microsoft::UI::Xaml::Controls;
    TextBlock Heading(DependencyObject const& element) {
        if (auto text = element.try_as<TextBlock>(); text && text.FontSize() == 24.) return text;
        for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(element); ++i)
            if (auto text = Heading(Media::VisualTreeHelper::GetChild(element, i))) return text;
        return nullptr;
    }
    suite settings_width_tests = [] {
        "all settings pages keep their left anchor when the window grows beyond the reading width"_test = [] {
            Window window; Grid host, root; host.Children().Append(root); window.Content(host);
            // Keep wide layout cases independent of the monitor's work-area
            // limit. The nested native viewport can exceed the visible window.
            root.HorizontalAlignment(HorizontalAlignment::Left);
            root.Width(1120);
            auto view = std::make_shared<winrt::Folia::SettingsView>(winrt::Folia::AppSettings{},
                std::make_shared<winrt::Folia::ThemeCatalog>(), std::make_shared<winrt::Folia::LatexCommandCatalog>(),
                folia::Theme::Light, nullptr, [](auto const&) -> std::optional<winrt::hstring> { return {}; }, [] {});
            struct Cleanup { Window window; std::shared_ptr<winrt::Folia::SettingsView> view;
                ~Cleanup() { view->Detach(); window.Close(); } } cleanup{window, view};
            root.Children().Append(view->Root()); window.Activate();
            Pump([&] { return root.IsLoaded(); });
            auto navigation = view->Root();
            std::string stage;
            try {
                for (auto entry : navigation.MenuItems()) {
                    stage = "resize baseline";
                    root.Width(1120);
                    navigation.SelectedItem(entry);
                    Pump([&] { return std::abs(root.ActualWidth() - 1120) < 20.; });
                    root.UpdateLayout();
                    auto page = navigation.Content().as<FrameworkElement>();
                    stage = "realize page heading";
                    Pump([&] { root.UpdateLayout(); return Heading(page) && Heading(page).ActualWidth() > 0; });
                    auto settled = std::chrono::steady_clock::now() + std::chrono::milliseconds(350);
                    Pump([&] { return std::chrono::steady_clock::now() >= settled; });
                    auto heading = Heading(page);
                    const auto anchor = heading.TransformToVisual(navigation).TransformPoint({0, 0}).X;
                    for (auto width : {1800, 2400, 720, 1120}) {
                        stage = "resize to " + std::to_string(width);
                        root.Width(width);
                        Pump([&] { return std::abs(root.ActualWidth() - width) < 20.; });
                        root.UpdateLayout();
                        const auto point = heading.TransformToVisual(navigation).TransformPoint({0, 0});
                        auto name = winrt::to_string(winrt::unbox_value<winrt::hstring>(entry.as<NavigationViewItem>().Tag()));
                        expect(std::abs(point.X - anchor) < 2.) << name << " at width " << width;
                        expect(heading.ActualWidth() <= 960. + 1.) << name;
                        expect(point.X >= 0 && point.X + heading.ActualWidth() <= navigation.ActualWidth() + 1.) << name;
                    }
                }
            } catch (...) {
                std::cerr << "Settings layout stage=" << stage << ", root width=" << root.ActualWidth()
                    << ", navigation width=" << navigation.ActualWidth() << '\n';
                throw;
            }
        };
    };
}
