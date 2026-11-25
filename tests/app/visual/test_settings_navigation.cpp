#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "visual_window.h"
#include <winrt/Microsoft.UI.Xaml.Automation.Peers.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Provider.h>

namespace {
using namespace boost::ut;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace document_tests;
void Invoke(Button const& button) {
    Automation::Peers::ButtonAutomationPeer peer(button);
    peer.GetPattern(Automation::Peers::PatternInterface::Invoke).as<Automation::Provider::IInvokeProvider>().Invoke();
}
suite settings_navigation_tests = [] {
    "settings navigation stays in one tab and workspace controls return to the document"_test = [] {
        auto window = visual_tests::Window();
        auto root = window.Content().as<Grid>();
        Pump([&] { return root.IsLoaded() && root.ActualWidth() > 0; });
        auto settings = root.FindName(L"SettingsButton").as<Button>();
        auto host = root.FindName(L"SettingsViewHost").as<ContentControl>();
        auto sidebar = root.FindName(L"WorkspaceSidebar").as<Grid>();
        auto sidebarToggle = root.FindName(L"SidebarButton").as<Primitives::ToggleButton>();
        auto toolbar = root.FindName(L"DocumentToolbar").as<Grid>();
        auto tabs = root.FindName(L"DocumentTabs").as<TabView>();
        auto document = tabs.SelectedItem();
        const auto initialVisibility = sidebar.Visibility();
        const auto size = window.AppWindow().Size();
        Invoke(settings);
        Pump([&] { return host.Visibility() == Visibility::Visible; });
        expect(sidebar.Visibility() == initialVisibility && window.AppWindow().Size() == size);
        expect(toolbar.Visibility() == Visibility::Collapsed);
        auto find = [&](auto&& self, DependencyObject const& node, std::wstring_view name) -> FrameworkElement {
            if (auto element = node.try_as<FrameworkElement>(); element && element.Name() == name) return element;
            for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(node); ++i)
                if (auto child = self(self, Media::VisualTreeHelper::GetChild(node, i), name)) return child;
            return nullptr;
        };
        auto go = [&](std::wstring_view name) {
            Button target{nullptr};
            Pump([&] {
                target = find(find, host, name).try_as<Button>();
                return target && target.IsLoaded() && target.ActualHeight() > 0;
            });
            Invoke(target);
        };
        go(L"SettingsCategoryLicenses");
        go(L"SettingsHomeButton");
        go(L"SettingsCategoryAICompletion");
        auto settingsTab = tabs.SelectedItem();
        Invoke(settings);
        expect(tabs.SelectedItem() == settingsTab);
        tabs.SelectedItem(document);
        Pump([&] { return host.Visibility() == Visibility::Collapsed && sidebar.Visibility() == initialVisibility; });
        expect(sidebar.Visibility() == initialVisibility);
        expect(toolbar.Visibility() == Visibility::Visible);
        Invoke(settings);
        Pump([&] { return host.Visibility() == Visibility::Visible; });
        expect(tabs.SelectedItem() == settingsTab);
        sidebarToggle.IsChecked(initialVisibility != Visibility::Visible);
        Pump([&] { return tabs.SelectedItem() == document && sidebar.Visibility() != initialVisibility; });
        sidebarToggle.IsChecked(initialVisibility == Visibility::Visible);
    };
};
}
