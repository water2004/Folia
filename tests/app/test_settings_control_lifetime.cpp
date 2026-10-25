#include "pch.h"
#include "support/folia_test.hpp"
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include "settings/SettingsView.h"

using namespace boost::ut;
using namespace winrt::Folia;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace
{
    void SettleSettingsControls()
    {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        do
        {
            MSG message{};
            for (int i = 0; i < 64 && ::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE); ++i)
            {
                ::TranslateMessage(&message);
                ::DispatchMessageW(&message);
            }
            ::MsgWaitForMultipleObjectsEx(0, nullptr, 1, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        } while (std::chrono::steady_clock::now() < deadline);
    }

    ComboBox FindScope(UIElement const& element)
    {
        if (auto combo = element.try_as<ComboBox>())
            return Grid::GetColumn(combo) == 1 ? combo : nullptr;
        if (auto panel = element.try_as<Panel>())
            for (auto const& child : panel.Children())
                if (auto found = FindScope(child)) return found;
        if (auto content = element.try_as<ContentControl>())
            if (auto child = content.Content().try_as<UIElement>()) return FindScope(child);
        if (auto scroll = element.try_as<ScrollViewer>())
            if (auto child = scroll.Content().try_as<UIElement>()) return FindScope(child);
        if (auto border = element.try_as<Border>())
            if (auto child = border.Child()) return FindScope(child);
        return nullptr;
    }

    suite settings_control_lifetime_tests = []
    {
        "settings reset releases replaced shortcut scope controls"_test = []
        {
          try
          {
            Hosting::WindowsXamlManager xaml{Hosting::WindowsXamlManager::InitializeForCurrentThread()};
            auto themes = std::make_shared<ThemeCatalog>();
            auto latex = std::make_shared<LatexCommandCatalog>();
            AppSettings settings;
            auto view = std::make_shared<SettingsView>(settings, themes, latex, folia::Theme::Light,
                nullptr, [](auto const&) -> std::optional<winrt::hstring> { return std::nullopt; });
            struct DetachView
            {
                std::shared_ptr<SettingsView> const& view;
                ~DetachView() { view->Detach(); }
            } detach{view};
            auto navigation = view->Root();
            for (auto const& item : navigation.MenuItems())
            {
                auto entry = item.as<NavigationViewItem>();
                if (winrt::unbox_value<winrt::hstring>(entry.Tag()) == L"shortcuts")
                {
                    navigation.SelectedItem(entry);
                    break;
                }
            }
            SettleSettingsControls();
            auto content = navigation.Content().try_as<UIElement>();
            expect(static_cast<bool>(content)) << "settings navigation must expose content";
            if (!content) return;
            for (int iteration = 0; iteration < 2; ++iteration)
            {
                auto scope = FindScope(content);
                expect(static_cast<bool>(scope)) << "shortcuts page must expose a scope ComboBox in column 1";
                if (!scope) return;
                expect(scope.Items().Size() == 3_u) << "scope must offer global, code and math";
                expect(Grid::GetColumn(scope) == 1_i) << "scope must occupy shortcuts column 1";
                auto oldScope = winrt::make_weak(scope);
                scope = nullptr; // Only the old visual tree and its handler can retain it.
                view->Reset(settings, folia::Theme::Light);
                SettleSettingsControls();
                expect(!oldScope.get()) << "replaced scope retained after reset " << iteration;
                expect(static_cast<bool>(FindScope(content))) << "reset must replace the scope control";
            }
          }
          catch (winrt::hresult_error const& error)
          {
              throw std::runtime_error(std::format("settings lifetime fixture: HRESULT 0x{:08X}: {}",
                  static_cast<std::uint32_t>(error.code().value), winrt::to_string(error.message())));
          }
        };
    };
}
