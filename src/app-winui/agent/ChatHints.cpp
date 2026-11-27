#include "pch.h"
#include "agent/ChatHints.h"
#include <winrt/Microsoft.UI.Xaml.Automation.h>

namespace winrt::Folia::chat {
    using namespace Microsoft::UI::Xaml;
    using namespace Controls;
    using Automation::AutomationProperties;

    namespace {
        // Keep a hint in its owner's XAML island. Closing a separate tooltip window can
        // restore the arrow after the pointer has already entered selectable content.
        struct Hint : std::enable_shared_from_this<Hint> {
            inline static thread_local std::weak_ptr<Hint> active;
            Microsoft::UI::Dispatching::DispatcherQueueTimer delay{nullptr};
            Primitives::Popup popup{nullptr};

            void Close() {
                if (delay) { delay.Stop(); delay = nullptr; }
                if (popup) { popup.IsOpen(false); popup = nullptr; }
            }

            void Schedule(FrameworkElement const& owner) {
                if (auto previous = active.lock()) previous->Close();
                if (AutomationProperties::GetHelpText(owner).empty()) return;
                active = weak_from_this();
                UINT hoverTime = ::GetDoubleClickTime();
                ::SystemParametersInfoW(SPI_GETMOUSEHOVERTIME, 0, &hoverTime, 0);
                delay = owner.DispatcherQueue().CreateTimer();
                delay.Interval(std::chrono::milliseconds(hoverTime));
                delay.IsRepeating(false);
                delay.Tick([weak = weak_from_this(), target = make_weak(owner)](auto const& timer, auto const&) {
                    auto self = weak.lock();
                    auto element = target.get();
                    if (!self || self->delay != timer || !element || !element.IsLoaded()) return;
                    self->delay = nullptr;
                    self->Open(element);
                });
                delay.Start();
            }

            void Open(FrameworkElement const& owner) {
                auto text = AutomationProperties::GetHelpText(owner);
                if (text.empty()) return;
                if (auto previous = active.lock(); previous && previous.get() != this) previous->Close();
                active = weak_from_this();
                auto hint = Application::Current().Resources().Lookup(box_value(L"ChatHintTemplate")).as<DataTemplate>()
                    .LoadContent().as<Border>();
                hint.Child().as<TextBlock>().Text(text);
                hint.RequestedTheme(owner.ActualTheme());
                popup = Primitives::Popup{};
                popup.IsHitTestVisible(false);
                popup.ShouldConstrainToRootBounds(true);
                popup.XamlRoot(owner.XamlRoot());
                popup.PlacementTarget(owner);
                popup.DesiredPlacement(Primitives::PopupPlacementMode::BottomEdgeAlignedLeft);
                popup.Child(hint);
                popup.IsOpen(true);
            }
        };
    }

    void SetHint(FrameworkElement const& owner, hstring const& text) {
        if (text == AutomationProperties::GetHelpText(owner)) return;
        auto property = AutomationProperties::HelpTextProperty();
        if (owner.ReadLocalValue(property) == DependencyProperty::UnsetValue()) {
            auto hint = std::make_shared<Hint>();
            auto dismiss = [hint](auto const&, auto const&) { hint->Close(); };
            owner.PointerEntered([hint](auto const& sender, Input::PointerRoutedEventArgs const& e) {
                if (e.Pointer().PointerDeviceType() == Microsoft::UI::Input::PointerDeviceType::Mouse)
                    hint->Schedule(sender.template as<FrameworkElement>());
            });
            owner.GotFocus([hint](auto const& sender, auto const&) {
                auto element = sender.template as<FrameworkElement>();
                if (element.FocusState() == FocusState::Keyboard) hint->Schedule(element);
            });
            owner.PointerExited(dismiss);
            owner.AddHandler(UIElement::PointerPressedEvent(), box_value(Input::PointerEventHandler(dismiss)), true);
            owner.Holding([hint](auto const& sender, Input::HoldingRoutedEventArgs const& e) {
                hint->Close();
                if (e.HoldingState() == Microsoft::UI::Input::HoldingState::Started)
                    hint->Open(sender.template as<FrameworkElement>());
            });
            owner.LostFocus(dismiss);
            owner.Unloaded(dismiss);
            owner.RegisterPropertyChangedCallback(property, dismiss);
        }
        AutomationProperties::SetHelpText(owner, text);
    }
}
