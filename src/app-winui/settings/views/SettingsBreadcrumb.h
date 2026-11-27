#pragma once

namespace winrt::Folia::settings_ui {
    class SettingsBreadcrumb {
    public:
        struct Item {
            hstring title;
            std::function<void()> open;
        };
        using Path = std::vector<Item>;

        SettingsBreadcrumb();
        auto Element() const { return root_; }
        void SetPath(Path path);
        void Focus() const;
        void Detach();

    private:
        void Open(std::size_t index) const;
        Microsoft::UI::Xaml::Controls::Grid root_{nullptr};
        Microsoft::UI::Xaml::Controls::Button back_{nullptr};
        Windows::Foundation::Collections::IObservableVector<Windows::Foundation::IInspectable> labels_ =
            single_threaded_observable_vector<Windows::Foundation::IInspectable>();
        Path path_;
        Microsoft::UI::Xaml::Controls::Button::Click_revoker backClick_;
        Microsoft::UI::Xaml::Controls::BreadcrumbBar::ItemClicked_revoker itemClick_;
    };
}
