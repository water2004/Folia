#include "pch.h"

import std;

#include "ui/TextLayoutControls.h"
#include "ui/ControlHelpers.h"
#include "workspace/WorkspaceIcons.h"
#include <winrt/Microsoft.UI.Xaml.Automation.h>

namespace winrt::Folia::ui
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    namespace
    {
        TextBlock Label(hstring const& text, wchar_t const* style)
        {
            TextBlock label;
            label.Style(ResourceStyle(style));
            label.Text(text);
            return label;
        }
    }

    TextLayoutControls::TextLayoutControls(hstring const& heading, TextLayoutOptions const& options, Change changed, bool compact)
        : changed_(std::move(changed))
    {
        constexpr wchar_t const* labels[]{L"TextLayoutFont", L"TextLayoutLineSpacing", L"TextLayoutMargin"};
        constexpr wchar_t const* names[]{L"TextLayoutFontSlider", L"TextLayoutLineSpacingSlider", L"TextLayoutMarginSlider"};
        panel_.Spacing(compact ? 9 : 5);
        panel_.UseLayoutRounding(true);
        if (compact) panel_.Width(272);

        auto caption = [&](hstring const& text, wchar_t const* style) {
            auto label = Label(text, style);
            if (compact) label.Foreground(ink_);
            return label;
        };
        Grid header;
        Columns(header, {true, false});
        header.ColumnSpacing(12);
        auto title = caption(heading, L"TextLayoutTitleStyle");
        Put(header, title, 0);
        reset_.Style(ResourceStyle(L"TextLayoutResetStyle"));
        reset_.Content(box_value(Localize(L"TextLayoutReset")));
        Put(header, reset_, 0, 1);
        panel_.Children().Append(header);

        for (std::size_t index = 0; index < sliders_.size(); ++index)
        {
            auto const& range = textLayoutRanges[index];
            auto label = caption(Localize(labels[index]), L"TextLayoutLabelStyle");
            auto const& slider = sliders_[index];
            slider.Name(names[index]);
            slider.Style(ResourceStyle(L"TextLayoutSliderStyle"));
            slider.Minimum(range.minimum);
            slider.Maximum(range.maximum);
            slider.StepFrequency(textLayoutStep);
            slider.SmallChange(textLayoutStep);
            slider.LargeChange(2 * textLayoutStep);
            slider.IsThumbToolTipEnabled(false);
            Automation::AutomationProperties::SetLabeledBy(slider, label);

            auto const& readout = readouts_[index];
            readout.Style(ResourceStyle(L"TextLayoutValueStyle"));
            if (compact) {
                readout.Foreground(muted_);
                slider.Foreground(accent_);
                slider.BorderBrush(border_);
            }
            Grid row;
            if (compact) {
                Rows(row, {false, false});
                Columns(row, {true, false});
                Put(row, label, 0);
                Put(row, readout, 0, 1);
                Grid::SetColumnSpan(slider, 2);
                Put(row, slider, 1);
                panel_.Children().Append(row);
            } else {
                row.Style(ResourceStyle(L"SettingsCardRowStyle"));
                Rows(row, {true, false});
                Columns(row, {true, true, false});
                row.ColumnDefinitions().GetAt(1).MaxWidth(320);
                Put(row, label, 0);
                Put(row, slider, 0, 1);
                Put(row, readout, 0, 2);
                row.SizeChanged([track = make_weak(slider), label = make_weak(label)](auto const&, SizeChangedEventArgs const& args) {
                    auto field = track.get();
                    auto text = label.get();
                    if (!field || !text) return;
                    const bool stacked = args.NewSize().Width < 420;
                    Grid::SetRow(field, stacked ? 1 : 0);
                    Grid::SetColumn(field, stacked ? 0 : 1);
                    Grid::SetColumnSpan(field, stacked ? 3 : 1);
                    Grid::SetColumnSpan(text, stacked ? 2 : 1);
                });
                Border card;
                card.Style(ResourceStyle(L"SettingsCardSurfaceStyle"));
                card.Child(row);
                panel_.Children().Append(card);
            }
            sliderEvents_[index] = slider.ValueChanged(auto_revoke, [this, range](auto const&, Primitives::RangeBaseValueChangedEventArgs const& args) {
                if (updating_) return;
                auto value = options_;
                value.*range.value = TextLayoutPercent(args.NewValue(), range);
                Publish(value);
            });
        }
        SetOptions(options);
        resetClick_ = reset_.Click(auto_revoke, [this](auto const&, auto const&) { Publish({}); });
    }

    void TextLayoutControls::SetTheme(folia::ThemeProfile const& theme)
    {
        auto set = [](Media::SolidColorBrush const& brush, folia::Color color) {
            brush.Color({255, color.r, color.g, color.b});
        };
        auto const& colors = theme.colors;
        set(surface_, colors.shell_layer_bg);
        set(border_, colors.shell_border);
        set(ink_, colors.shell_fg);
        set(muted_, colors.shell_muted_fg);
        set(accent_, colors.shell_accent);
        panel_.RequestedTheme(theme.variant == folia::Theme::Light ? ElementTheme::Light
            : theme.variant == folia::Theme::Dark ? ElementTheme::Dark : ElementTheme::Default);
    }

    Style TextLayoutControls::PresenterStyle() const
    {
        // A popup has a separate visual tree, so the window's scoped resource
        // brushes are supplied explicitly and updated in place as themes change.
        auto base = ResourceStyle(L"TextLayoutPresenterStyle");
        Style style;
        style.TargetType(base.TargetType());
        style.BasedOn(base);
        style.Setters().Append(Setter(Control::BackgroundProperty(), surface_));
        style.Setters().Append(Setter(Control::BorderBrushProperty(), border_));
        return style;
    }

    void TextLayoutControls::SetOptions(TextLayoutOptions const& value)
    {
        options_ = value;
        updating_ = true;
        for (std::size_t index = 0; index < sliders_.size(); ++index) {
            auto number = value.*textLayoutRanges[index].value;
            sliders_[index].Value(number);
            readouts_[index].Text(hstring{std::to_wstring(number) + L"%"});
        }
        reset_.IsEnabled(value != TextLayoutOptions{});
        updating_ = false;
    }

    void TextLayoutControls::Publish(TextLayoutOptions const& value)
    {
        if (value == options_) return;
        SetOptions(value);
        changed_(value);
    }

    std::unique_ptr<TextLayoutControls> AttachTextLayoutFlyout(
        Button const& button, TextLayoutOptions const& options, TextLayoutControls::Change changed)
    {
        auto panel = std::make_unique<TextLayoutControls>(Localize(L"TextLayout"), options, std::move(changed), true);
        button.Content(MakeWorkspaceIcon(WorkspaceIcon::TextLayout, L"FoliaShellForegroundBrush"));
        auto label = Localize(L"TextLayout");
        Automation::AutomationProperties::SetName(button, label);
        ToolTipService::SetToolTip(button, box_value(label));
        Flyout flyout;
        flyout.Placement(Primitives::FlyoutPlacementMode::BottomEdgeAlignedRight);
        flyout.FlyoutPresenterStyle(panel->PresenterStyle());
        flyout.Content(panel->Element());
        button.Flyout(flyout);
        return panel;
    }
}
