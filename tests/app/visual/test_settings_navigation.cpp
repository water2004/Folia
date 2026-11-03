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
FrameworkElement FindBack(DependencyObject const& parent) {
    if (auto element = parent.try_as<FrameworkElement>(); element && element.Name() == L"NavigationViewBackButton") return element;
    for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(parent); ++i)
        if (auto found = FindBack(Media::VisualTreeHelper::GetChild(parent, i))) return found;
    return nullptr;
}
void Invoke(Button const& button) {
    Automation::Peers::ButtonAutomationPeer peer(button);
    peer.GetPattern(Automation::Peers::PatternInterface::Invoke).as<Automation::Provider::IInvokeProvider>().Invoke();
}
suite settings_navigation_tests = [] {
    "settings opens directly from the application menu and returns through native Back"_test = [] {
        auto window = visual_tests::Window();
        auto root = window.Content().as<Grid>();
        Pump([&] { return root.IsLoaded() && root.ActualWidth() > 0; });
        auto menuButton = root.FindName(L"MainMenuButton").as<Button>();
        auto menu = menuButton.Flyout().as<MenuFlyout>();
        auto settings = root.FindName(L"SettingsButton").as<MenuFlyoutItem>();
        expect(menu.Items().Size() == 3_u);
        expect(menu.Items().GetAt(2) == settings);
        expect(!root.FindName(L"ViewMenu"));
        auto host = root.FindName(L"SettingsViewHost").as<Grid>();
        auto workspace = root.FindName(L"DocumentWorkspace").as<SplitView>();
        auto status = root.FindName(L"StatusBar").as<Grid>();
        auto tabs = root.FindName(L"DocumentTabs").as<TabView>();
        auto selected = tabs.SelectedItem();
        bool flyoutClosed = true;
        auto closed = menu.Closed(winrt::auto_revoke, [&](auto const&, auto const&) { flyoutClosed = true; });
        auto open = [&] {
            std::cout << "settings navigation: show menu\n";
            flyoutClosed = false;
            menu.ShowAt(menuButton);
            Pump([&] { return settings.IsLoaded(); });
            std::cout << "settings navigation: invoke settings\n";
            Automation::Peers::MenuFlyoutItemAutomationPeer peer(settings);
            peer.GetPattern(Automation::Peers::PatternInterface::Invoke).as<Automation::Provider::IInvokeProvider>().Invoke();
            Pump([&] { return host.Visibility() == Visibility::Visible; });
            Pump([&] { return flyoutClosed; });
            std::cout << "settings navigation: settings visible\n";
        };
        for (int iteration = 0; iteration < 3; ++iteration) {
            open();
            auto navigation = host.Children().GetAt(0).as<NavigationView>();
            expect(navigation.IsBackButtonVisible() == NavigationViewBackButtonVisible::Visible);
            expect(navigation.IsBackEnabled());
            for (auto item : navigation.MenuItems()) {
                auto entry = item.as<NavigationViewItem>();
                if (winrt::unbox_value<winrt::hstring>(entry.Tag()) == L"completion") navigation.SelectedItem(entry);
            }
            open(); // An entry named Settings opens it; it must not toggle it closed.
            expect(host.Children().GetAt(0) == navigation);
            root.UpdateLayout();
            auto back = FindBack(navigation).as<Button>();
            expect(back.Visibility() == Visibility::Visible && back.IsEnabled());
            Invoke(back);
            Pump([&] { return host.Visibility() == Visibility::Collapsed; });
            expect(workspace.Visibility() == Visibility::Visible);
            expect(status.Visibility() == Visibility::Visible);
            expect(tabs.SelectedItem() == selected);
        }
    };
};
}
