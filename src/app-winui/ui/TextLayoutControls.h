#pragma once

import folia.app.text_layout_options;
import folia.core.theme;

namespace winrt::Folia::ui
{
    // Both the toolbar and settings edit the same options through a single control.
    class TextLayoutControls
    {
    public:
        using Change = std::function<void(TextLayoutOptions const&)>;

        TextLayoutControls(hstring const& heading, TextLayoutOptions const&, Change changed, bool compact);
        Microsoft::UI::Xaml::Controls::StackPanel Element() const { return panel_; }
        void SetOptions(TextLayoutOptions const&);
        void SetTheme(folia::ThemeProfile const&);
        Microsoft::UI::Xaml::Style PresenterStyle() const;

    private:
        using Slider = Microsoft::UI::Xaml::Controls::Slider;
        using TextBlock = Microsoft::UI::Xaml::Controls::TextBlock;
        using Button = Microsoft::UI::Xaml::Controls::Button;
        void Publish(TextLayoutOptions const&);

        Microsoft::UI::Xaml::Controls::StackPanel panel_;
        std::array<Slider, textLayoutRanges.size()> sliders_;
        std::array<TextBlock, textLayoutRanges.size()> readouts_;
        Button reset_;
        Microsoft::UI::Xaml::Media::SolidColorBrush surface_, border_, ink_, muted_, accent_;
        TextLayoutOptions options_;
        Change changed_;
        bool updating_ = false;
        std::array<Slider::ValueChanged_revoker, textLayoutRanges.size()> sliderEvents_;
        Button::Click_revoker resetClick_;
    };

    // The owner keeps these controls alive for as long as the button can open its flyout.
    std::unique_ptr<TextLayoutControls> AttachTextLayoutFlyout(
        Microsoft::UI::Xaml::Controls::Button const&,
        TextLayoutOptions const&, TextLayoutControls::Change);
}
