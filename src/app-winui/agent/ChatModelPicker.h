#pragma once

namespace winrt::Folia::chat {
    class ChatModelPicker {
    public:
        ChatModelPicker(std::function<void(int)> selectModel, std::function<void(std::string)> selectEffort,
                        std::function<void()> configureModels);

        Microsoft::UI::Xaml::Controls::Button Element() const { return chip_; }
        int SelectedIndex() const;
        void SetEnabled(bool enabled);
        void SetModels(std::span<hstring const> names, int selected);
        void SetReasoning(std::span<std::string const> choices, std::string_view defaultEffort, std::string_view overrideEffort);

    private:
        void ShowModels(bool show);
        void RefreshSelection();
        void PresentReasoning(std::string_view overrideEffort);

        Microsoft::UI::Xaml::Controls::Button chip_{nullptr}, modelLink_{nullptr}, back_{nullptr}, configure_{nullptr};
        Microsoft::UI::Xaml::Controls::Flyout flyout_;
        Microsoft::UI::Xaml::Controls::StackPanel reasoning_{nullptr}, models_{nullptr}, missingModels_{nullptr};
        Microsoft::UI::Xaml::Controls::ListView list_{nullptr}, efforts_{nullptr};
        Microsoft::UI::Xaml::Controls::TextBlock chipName_{nullptr}, chipEffort_{nullptr}, currentModel_{nullptr};
        std::vector<Microsoft::UI::Xaml::Controls::FontIcon> checks_;
        int checked_ = -1;
        std::vector<std::string> levels_;
        std::string defaultEffort_;
        bool updating_ = false;
        Microsoft::UI::Xaml::Controls::Primitives::FlyoutBase::Opening_revoker opening_;
        Microsoft::UI::Xaml::Controls::Primitives::Selector::SelectionChanged_revoker modelChanged_;
        Microsoft::UI::Xaml::Controls::Primitives::Selector::SelectionChanged_revoker effortChanged_;
        Microsoft::UI::Xaml::Controls::Primitives::ButtonBase::Click_revoker pickClick_, backClick_, settingsClick_;
    };
}
