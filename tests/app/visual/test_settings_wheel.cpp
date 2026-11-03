#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "settings/SettingsViewSupport.h"

namespace {
using namespace boost::ut;
using namespace winrt::Microsoft::UI::Xaml;
using namespace document_tests;

// Supply only event data to the public override. This does not move or inject
// desktop input. Closed choices must neither read a wheel delta nor consume it.
struct WheelEvent : winrt::implements<WheelEvent, Input::IPointerRoutedEventArgs, IRoutedEventArgs> {
    bool handled = false;
    int reads = 0;
    Input::Pointer Pointer() const { return nullptr; }
    winrt::Windows::System::VirtualKeyModifiers KeyModifiers() const { return winrt::Windows::System::VirtualKeyModifiers::None; }
    bool Handled() const { return handled; }
    void Handled(bool value) { handled = value; }
    bool IsGenerated() const { return true; }
    winrt::Windows::Foundation::IInspectable OriginalSource() const { return nullptr; }
    winrt::Microsoft::UI::Input::PointerPoint GetCurrentPoint(UIElement const&) { ++reads; throw winrt::hresult_not_implemented(); }
    winrt::Windows::Foundation::Collections::IVector<winrt::Microsoft::UI::Input::PointerPoint> GetIntermediatePoints(UIElement const&) { ++reads; throw winrt::hresult_not_implemented(); }
};
suite settings_wheel_tests = [] {
    "closed settings choices preserve selection and leave wheel routing to the page"_test = [] {
        Window window; Controls::Grid root; window.Content(root);
        struct Close { Window window; ~Close() { window.Close(); } } close{window};
        auto choice = winrt::Folia::settings_ui::Choice();
        for (auto item : {L"first", L"second", L"third"}) choice.Items().Append(winrt::box_value(item));
        choice.SelectedIndex(1); root.Children().Append(choice); window.Activate();
        Pump([&] { return choice.IsLoaded(); });
        expect(choice.Focus(FocusState::Programmatic)); expect(!choice.IsDropDownOpen());
        auto event = winrt::make_self<WheelEvent>();
        auto args = event.as<Input::PointerRoutedEventArgs>();
        auto overrides = choice.as<Controls::IControlOverrides>();
        for (int i = 0; i < 3; ++i) overrides.OnPointerWheelChanged(args);
        expect(choice.SelectedIndex() == 1_i); expect(!event->handled); expect(event->reads == 0_i);
        choice.SelectedIndex(2); expect(choice.SelectedIndex() == 2_i);
    };
};
}
