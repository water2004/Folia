#pragma once

namespace winrt::Folia::chat {
    struct ChatPalette;
    class ChatModelPicker {
    public:
        ChatModelPicker(std::function<void(int)> selectModel, std::function<void(std::string)> selectEffort,
                        std::function<void()> configureModels);

        Microsoft::UI::Xaml::Controls::Button Element() const { return chip_; }
        int SelectedIndex() const;
        void Open() { flyout_.ShowAt(chip_); }
        void SetEnabled(bool enabled);
        void SetPalette(ChatPalette const&);
        void SetModels(std::span<hstring const> names, int selected);
        void SetReasoning(std::span<std::string const> choices, std::string_view defaultEffort, std::string_view overrideEffort);

    private:
        void ShowModels(bool show);
        void RefreshSelection();
        void DrawScale(bool animate = false);
        void PresentReasoning(std::string_view overrideEffort, bool animate = false);

        Microsoft::UI::Xaml::Controls::Button chip_{nullptr}, modelLink_{nullptr}, back_{nullptr}, configure_{nullptr};
        Microsoft::UI::Xaml::Controls::Flyout flyout_;
        Microsoft::UI::Xaml::Controls::StackPanel reasoning_{nullptr}, models_{nullptr}, missingModels_{nullptr};
        Microsoft::UI::Xaml::Controls::ListView list_{nullptr};
        Microsoft::UI::Xaml::Controls::Slider efforts_{nullptr};
        Microsoft::UI::Xaml::Controls::Grid track_{nullptr}, scale_{nullptr};
        Microsoft::UI::Xaml::Controls::Canvas ticks_{nullptr};
        Microsoft::UI::Xaml::Controls::Border fill_{nullptr}, knob_{nullptr}, rail_{nullptr};
        Microsoft::UI::Xaml::Media::Brush onAccent_{nullptr}, muted_{nullptr};
        Microsoft::UI::Xaml::Controls::TextBlock effortTitle_{nullptr};
        Microsoft::UI::Xaml::Controls::TextBlock chipName_{nullptr}, chipEffort_{nullptr}, currentModel_{nullptr};
        std::vector<Microsoft::UI::Xaml::Controls::FontIcon> checks_;
        int checked_ = -1;
        std::vector<std::string> levels_;
        std::string defaultEffort_;
        Microsoft::UI::Composition::CompositionPropertySet scaleMotion_{nullptr};
        bool updating_ = false;
        Microsoft::UI::Xaml::Controls::Primitives::FlyoutBase::Opening_revoker opening_;
        Microsoft::UI::Xaml::Controls::Primitives::Selector::SelectionChanged_revoker modelChanged_;
        Microsoft::UI::Xaml::Controls::Primitives::RangeBase::ValueChanged_revoker effortChanged_;
        Microsoft::UI::Xaml::FrameworkElement::SizeChanged_revoker trackSize_;
        Microsoft::UI::Xaml::Controls::Primitives::ButtonBase::Click_revoker pickClick_, backClick_, settingsClick_;
    };
}
