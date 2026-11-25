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
    "settings rail opens one tab and shares the workspace sidebar"_test = [] {
        auto window = visual_tests::Window();
        auto root = window.Content().as<Grid>();
        Pump([&] { return root.IsLoaded() && root.ActualWidth() > 0; });
        auto settings = root.FindName(L"SettingsButton").as<Button>();
        auto host = root.FindName(L"SettingsViewHost").as<ContentControl>();
        auto navigation = root.FindName(L"SettingsNavigation").as<ListView>();
        auto sidebar = root.FindName(L"WorkspaceSidebar").as<Grid>();
        auto documentNav = root.FindName(L"DocumentNavigation").as<Grid>();
        auto toolbar = root.FindName(L"DocumentToolbar").as<Grid>();
        auto tabs = root.FindName(L"DocumentTabs").as<TabView>();
        auto document = tabs.SelectedItem();
        const auto initialVisibility = sidebar.Visibility();
        Invoke(settings);
        Pump([&] { return host.Visibility() == Visibility::Visible; });
        expect(navigation.Parent() == sidebar);
        expect(sidebar.Visibility() == Visibility::Visible);
        expect(documentNav.Visibility() == Visibility::Collapsed);
        expect(toolbar.Visibility() == Visibility::Collapsed);
        expect(navigation.Items().Size() == 7_u);
        for (auto item : navigation.Items()) {
            if (winrt::unbox_value<winrt::hstring>(item.as<ListViewItem>().Tag()) == L"completion")
                navigation.SelectedItem(item);
        }
        auto settingsTab = tabs.SelectedItem();
        auto settingsPage = navigation.SelectedItem();
        Invoke(settings);
        expect(tabs.SelectedItem() == settingsTab && navigation.SelectedItem() == settingsPage);
        tabs.SelectedItem(document);
        Pump([&] { return host.Visibility() == Visibility::Collapsed && sidebar.Visibility() == initialVisibility; });
        expect(documentNav.Visibility() == Visibility::Visible);
        expect(sidebar.Visibility() == initialVisibility);
        expect(toolbar.Visibility() == Visibility::Visible);
        Invoke(settings);
        Pump([&] { return host.Visibility() == Visibility::Visible; });
        expect(tabs.SelectedItem() == settingsTab && navigation.SelectedItem() == settingsPage);
        tabs.SelectedItem(document);
    };
};
}
