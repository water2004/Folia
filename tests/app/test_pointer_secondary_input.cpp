#include "pch.h"
#include "support/folia_test.hpp"
#include "support/DocumentControllerFixture.h"
#include "editor/interaction/EditorPointerController.h"
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
import folia.markdown.instrumentation;

namespace {
    using namespace boost::ut;
    using namespace document_tests;
    namespace Input = winrt::Microsoft::UI::Input;
    namespace Xaml = winrt::Microsoft::UI::Xaml;

    // Deterministic WinRT input at the production handler boundary. These
    // events never inject input into a desktop window or move the system mouse.
    struct SecondaryProperties : winrt::implements<SecondaryProperties, Input::IPointerPointProperties> {
        Input::PointerUpdateKind kind;
        explicit SecondaryProperties(Input::PointerUpdateKind value) : kind(value) {}
        bool IsLeftButtonPressed() const { return false; }
        bool IsRightButtonPressed() const { return kind == Input::PointerUpdateKind::RightButtonPressed; }
        bool IsMiddleButtonPressed() const { return kind == Input::PointerUpdateKind::MiddleButtonPressed; }
        bool IsBarrelButtonPressed() const { return false; }
        bool IsCanceled() const { return false; }
        bool IsEraser() const { return false; }
        bool IsHorizontalMouseWheel() const { return false; }
        bool IsInRange() const { return true; }
        bool IsInverted() const { return false; }
        bool IsPrimary() const { return true; }
        bool IsXButton1Pressed() const { return kind == Input::PointerUpdateKind::XButton1Pressed; }
        bool IsXButton2Pressed() const { return false; }
        bool TouchConfidence() const { return true; }
        int MouseWheelDelta() const { return 0; }
        float Orientation() const { return 0; }
        float Pressure() const { return 0; }
        float Twist() const { return 0; }
        float XTilt() const { return 0; }
        float YTilt() const { return 0; }
        winrt::Windows::Foundation::Rect ContactRect() const { return {}; }
        Input::PointerUpdateKind PointerUpdateKind() const { return kind; }
    };
    struct SecondaryPoint : winrt::implements<SecondaryPoint, Input::IPointerPoint> {
        Input::PointerPointProperties properties{nullptr};
        explicit SecondaryPoint(Input::PointerUpdateKind kind) {
            auto value = winrt::make<SecondaryProperties>(kind);
            properties = {winrt::detach_abi(value), winrt::take_ownership_from_abi};
        }
        unsigned FrameId() const { return 1; }
        unsigned PointerId() const { return 1; }
        std::uint64_t Timestamp() const { return 0; }
        bool IsInContact() const { return true; }
        Input::PointerDeviceType PointerDeviceType() const { return Input::PointerDeviceType::Mouse; }
        Input::PointerPointProperties Properties() const { return properties; }
        winrt::Windows::Foundation::Point Position() const { return {20, 20}; }
        Input::PointerPoint GetTransformedPoint(Input::IPointerPointTransform const&) const { throw winrt::hresult_not_implemented(); }
    };
    struct SecondaryPress : winrt::implements<SecondaryPress, Xaml::Input::IPointerRoutedEventArgs> {
        Input::PointerPoint point{nullptr};
        bool handled = false;
        explicit SecondaryPress(Input::PointerUpdateKind kind) {
            auto value = winrt::make<SecondaryPoint>(kind);
            point = {winrt::detach_abi(value), winrt::take_ownership_from_abi};
        }
        Xaml::Input::Pointer Pointer() const { throw winrt::hresult_not_implemented(); }
        winrt::Windows::System::VirtualKeyModifiers KeyModifiers() const { return winrt::Windows::System::VirtualKeyModifiers::None; }
        bool Handled() const { return handled; }
        void Handled(bool value) { handled = value; }
        bool IsGenerated() const { return false; }
        Input::PointerPoint GetCurrentPoint(Xaml::UIElement const&) const { return point; }
        winrt::Windows::Foundation::Collections::IVector<Input::PointerPoint> GetIntermediatePoints(Xaml::UIElement const&) const { throw winrt::hresult_not_implemented(); }
    };

    suite pointer_secondary_tests = [] {
        "secondary pointer press preserves source selection history and semantic controls"_test = [] {
            auto xaml = Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
            for (bool sourceMode : {false, true}) for (auto kind : {Input::PointerUpdateKind::RightButtonPressed,
                    Input::PointerUpdateKind::MiddleButtonPressed, Input::PointerUpdateKind::XButton1Pressed}) {
                Fixture f(false, L"abc **def** ghi\n\n- [ ] task\n\n| a | b |\n| - | - |\n| x | y |\n", false);
                if (sourceMode) expect(f.session.EnterSourceMode());
                expect(f.session.ExecuteCommand(folia::Command::InsertText(U"prefix ")));
                auto start = f.session.Selection().active, end = start;
                start.source_offset = 1; end.source_offset = 5;
                f.session.SetSelection(start, end);
                auto selection = f.session.Selection();
                auto selected = f.session.SelectedSource();
                auto text = f.session.TextUtf8();
                auto state = f.session.ContentState();
                int effects = 0;
                Xaml::Controls::SwapChainPanel surface;
                winrt::Folia::EditorScrollController scroll;
                winrt::Folia::EditorPointerController pointer;
                pointer.Attach(f.session, f.renderer, scroll, f.input, surface,
                    [&](auto const&) { ++effects; return false; }, [&] { ++effects; },
                    [&](auto) { ++effects; }, [&](auto, auto) { ++effects; }, [&] { ++effects; });
                auto value = winrt::make_self<SecondaryPress>(kind);
                auto eventInterface = value.as<Xaml::Input::IPointerRoutedEventArgs>();
                Xaml::Input::PointerRoutedEventArgs args{winrt::detach_abi(eventInterface), winrt::take_ownership_from_abi};
                folia::reset_core_operation_counters();
                pointer.PointerPressed(args);
                auto counters = folia::read_core_operation_counters();
                expect(f.session.Selection() == selection);
                expect(f.session.SelectedSource() == selected);
                expect(f.session.TextUtf8() == text);
                expect(f.session.ContentState() == state);
                expect(effects == 0_i);
                expect(!value->handled);
                expect(counters.full_document_parses == 0_u);
                expect(counters.full_document_serializations == 0_u);
                pointer.Detach();
                expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            }
        };
    };
}
