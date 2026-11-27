#pragma once

#include "settings/views/SettingsViewSupport.h"
#include "workspace/WorkspaceIcons.h"

namespace winrt::Folia::settings_ui::cards {
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    struct Label {
        hstring title;
        hstring description{};
        FrameworkElement icon{nullptr};
    };

    inline Viewbox Icon(WorkspaceIcon id) {
        auto glyph = MakeWorkspaceIcon(id, L"FoliaShellForegroundBrush");
        glyph.ClearValue(FrameworkElement::WidthProperty());
        glyph.ClearValue(FrameworkElement::HeightProperty());
        glyph.Style(ResourceStyle(L"SettingsCardIconStyle"));
        return glyph;
    }

    inline FontIcon Icon(wchar_t const* glyph) {
        FontIcon icon;
        icon.Glyph(glyph);
        icon.Style(ResourceStyle(L"SettingsCardFontIconStyle"));
        return icon;
    }

    inline TextBlock Title(hstring const& value) {
        auto text = Text(value);
        text.Style(ResourceStyle(L"SettingsCardTitleStyle"));
        return text;
    }

    inline Grid Layout(Label const& label) {
        Grid row;
        row.Style(ResourceStyle(L"SettingsCardLayoutStyle"));
        for (auto width : {GridLengthHelper::FromValueAndType(Metric(L"SettingsCardIconColumn"), GridUnitType::Pixel),
                           GridLengthHelper::FromValueAndType(1, GridUnitType::Star),
                           GridLengthHelper::Auto(), GridLengthHelper::Auto()}) {
            ColumnDefinition column;
            column.Width(width);
            row.ColumnDefinitions().Append(column);
        }
        if (label.icon) {
            label.icon.VerticalAlignment(VerticalAlignment::Center);
            row.Children().Append(label.icon);
        }
        StackPanel copy;
        copy.Style(ResourceStyle(L"SettingsCardCopyStyle"));
        copy.Children().Append(Title(label.title));
        if (!label.description.empty()) copy.Children().Append(Hint(label.description));
        Grid::SetColumn(copy, 1);
        row.Children().Append(copy);
        return row;
    }

    inline FontIcon Next() {
        FontIcon arrow;
        arrow.Glyph(L"\uE76C");
        arrow.Style(ResourceStyle(L"SettingsNavigationArrowStyle"));
        return arrow;
    }

    inline void AppendNext(Grid const& row) {
        auto arrow = Next();
        Grid::SetColumn(arrow, 3);
        row.Children().Append(arrow);
    }

    inline Border Surface(UIElement const& element) {
        Border card;
        auto normal = ResourceStyle(L"SettingsCardSurfaceStyle");
        auto hover = ResourceStyle(L"SettingsCardHoverStyle");
        card.Style(normal);
        card.Child(element);
        card.PointerEntered([hover](auto const& sender, auto const&) { sender.template as<Border>().Style(hover); });
        card.PointerExited([normal](auto const& sender, auto const&) { sender.template as<Border>().Style(normal); });
        return card;
    }

    inline StackPanel List() {
        StackPanel list;
        list.Style(ResourceStyle(L"SettingsCardListStyle"));
        return list;
    }

    inline Grid SectionHeader(hstring const& heading, FrameworkElement const& action) {
        Grid header;
        header.ColumnSpacing(Metric(L"SettingsCardColumnGap"));
        ColumnDefinition titleColumn, actionColumn;
        actionColumn.Width(GridLengthHelper::Auto());
        header.ColumnDefinitions().Append(titleColumn);
        header.ColumnDefinitions().Append(actionColumn);
        auto title = SectionHeading(heading);
        title.VerticalAlignment(VerticalAlignment::Center);
        action.HorizontalAlignment(HorizontalAlignment::Right);
        Grid::SetColumn(action, 1);
        header.Children().Append(title);
        header.Children().Append(action);
        return header;
    }

    inline StackPanel Section(hstring const& heading, UIElement const& content, Button const& action = nullptr) {
        StackPanel section;
        section.Style(ResourceStyle(L"SettingsCardSectionStyle"));
        if (action) section.Children().Append(SectionHeader(heading, action));
        else if (!heading.empty()) section.Children().Append(SectionHeading(heading));
        section.Children().Append(content);
        return section;
    }

    enum class Placement { Inline, Wrap, Below };

    inline Grid Row(Label const& label, FrameworkElement const& control, Placement placement = Placement::Inline) {
        auto row = Layout(label);
        row.Style(ResourceStyle(L"SettingsCardRowStyle"));
        Automation::AutomationProperties::SetName(control, label.title);
        control.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(control, 2);
        Grid::SetColumnSpan(control, 2);
        row.Children().Append(control);
        if (placement == Placement::Inline) return row;

        RowDefinition caption, field;
        caption.Height(GridLengthHelper::Auto());
        field.Height(GridLengthHelper::Auto());
        row.RowDefinitions().Append(caption);
        row.RowDefinitions().Append(field);
        const auto gap = Metric(L"SettingsCardFieldGap");
        auto place = [gap](Grid const& grid, FrameworkElement const& value, bool below) {
            if (below == (Grid::GetRow(value) == 1)) return;
            grid.RowSpacing(below ? gap : 0);
            Grid::SetColumn(value, below ? 1 : 2);
            Grid::SetColumnSpan(value, below ? 3 : 2);
            Grid::SetRow(value, below ? 1 : 0);
            value.HorizontalAlignment(below ? HorizontalAlignment::Stretch : HorizontalAlignment::Right);
        };
        if (placement == Placement::Below) place(row, control, true);
        else {
            const auto minimum = Metric(L"SettingsCardInlineWidth");
            row.SizeChanged([place, minimum, value = make_weak(control)](auto const& sender, SizeChangedEventArgs const& args) {
                if (auto field = value.get()) place(sender.template as<Grid>(), field, args.NewSize().Width < minimum);
            });
        }
        return row;
    }

    inline Border Setting(Label const& label, FrameworkElement const& control, Placement placement = Placement::Inline) {
        return Surface(Row(label, control, placement));
    }

    inline void NameNavigation(Button const& button, Label const& label) {
        Automation::AutomationProperties::SetName(button, label.title);
        Automation::AutomationProperties::SetHelpText(button, label.description);
    }

    inline Button Link(Label const& label) {
        Button card;
        card.Style(ResourceStyle(L"SettingsCardLinkStyle"));
        auto row = Layout(label);
        AppendNext(row);
        card.Content(row);
        NameNavigation(card, label);
        return card;
    }

    template<class Action>
    Button Link(Label const& label, Action navigate) {
        auto card = Link(label);
        card.Click([navigate = std::move(navigate)](auto const&, auto const&) { navigate(); });
        return card;
    }

    template<class Action>
    Border LinkWithSwitch(Label const& label, ToggleSwitch const& value, Action navigate) {
        auto row = Layout(label);
        row.Style(ResourceStyle(L"SettingsCardCompoundStyle"));
        AppendNext(row);
        // The switch sits above the navigation button and receives its own input.
        Button link;
        link.Style(ResourceStyle(L"SettingsCardNavigationTargetStyle"));
        Grid::SetColumnSpan(link, 4);
        NameNavigation(link, label);
        link.Click([navigate = std::move(navigate)](auto const&, auto const&) { navigate(); });
        row.Children().Append(link);
        Grid::SetColumn(value, 2);
        row.Children().Append(value);
        return Surface(row);
    }

    inline ToggleSwitch Switch(bool on = false) {
        ToggleSwitch control;
        control.Style(ResourceStyle(L"SettingsCardSwitchStyle"));
        control.IsOn(on);
        return control;
    }

    inline void ChoiceStyle(ComboBox const& field) {
        field.Style(ResourceStyle(L"SettingsCardChoiceStyle"));
    }

    inline Button MoreActions(hstring const& label, MenuFlyout const& menu) {
        Button button;
        button.Style(ResourceStyle(L"SettingsCardInlineButtonStyle"));
        button.Content(MakeWorkspaceIcon(WorkspaceIcon::More, L"FoliaShellForegroundBrush"));
        button.Flyout(menu);
        ToolTipService::SetToolTip(button, box_value(label));
        Automation::AutomationProperties::SetName(button, label);
        return button;
    }

    inline void Status(TextBlock const& text, hstring const& value, bool error = false) {
        SetStatus(text, value, error);
        text.Visibility(value.empty() ? Visibility::Collapsed : Visibility::Visible);
    }
}
