#pragma once

#include "ui/ControlHelpers.h"

#include <winrt/Microsoft.UI.Xaml.Automation.h>
import folia.core.theme;

namespace winrt::Folia::settings_ui
{
    namespace Xaml = Microsoft::UI::Xaml;
    namespace Controls = Microsoft::UI::Xaml::Controls;

    struct ChoiceBox : Controls::ComboBoxT<ChoiceBox>
    {
        void OnPointerWheelChanged(Xaml::Input::PointerRoutedEventArgs const& args)
        {
            // Closed choices leave the wheel to the page's ScrollViewer.
            if (IsDropDownOpen()) Controls::ComboBoxT<ChoiceBox>::OnPointerWheelChanged(args);
        }
    };

    inline constexpr std::array<std::string_view, 3> LanguageIds{
        "system", "en-US", "zh-CN"};

    inline std::int32_t LanguageIndex(std::string_view languageId)
    {
        auto found = std::ranges::find(LanguageIds, languageId);
        return found == LanguageIds.end()
            ? 0
            : static_cast<std::int32_t>(std::distance(LanguageIds.begin(), found));
    }

    inline Xaml::Media::SolidColorBrush Brush(folia::Color color)
    {
        return Xaml::Media::SolidColorBrush(
            winrt::Windows::UI::Color{color.a, color.r, color.g, color.b});
    }

    inline Xaml::Media::FontFamily Font(std::string const& family)
    {
        return Xaml::Media::FontFamily(winrt::to_hstring(family));
    }

    using ui::ResourceStyle;

    inline Controls::ComboBox Choice()
    {
        auto control = winrt::make<ChoiceBox>().as<Controls::ComboBox>();
        control.Style(ResourceStyle(L"SettingsComboBoxStyle"));
        return control;
    }

    inline double Metric(wchar_t const* key)
    {
        return winrt::unbox_value<double>(Xaml::Application::Current().Resources().Lookup(winrt::box_value(key)));
    }

    enum class TextInput { Text, Number };

    inline void TextField(Controls::TextBox const& field, hstring const& label, TextInput input = TextInput::Text)
    {
        field.Style(ResourceStyle(L"SettingsTextBoxStyle"));
        if (!label.empty()) field.Header(box_value(label));
        if (input == TextInput::Number) {
            Xaml::Input::InputScope scope;
            Xaml::Input::InputScopeName name;
            name.NameValue(Xaml::Input::InputScopeNameValue::Number);
            scope.Names().Append(name);
            field.InputScope(scope);
        }
    }

    inline void TextField(Controls::PasswordBox const& field, hstring const& label)
    {
        field.Style(ResourceStyle(L"SettingsPasswordBoxStyle"));
        field.Header(box_value(label));
    }

    inline Controls::TextBlock Text(winrt::hstring const& value, double size = 0.0)
    {
        Controls::TextBlock text;
        text.Text(value);
        text.Style(ResourceStyle(L"SettingsBodyTextStyle"));
        if (size > 0.0) text.FontSize(size);
        return text;
    }

    inline Controls::TextBlock Hint(winrt::hstring const& value)
    {
        auto text = Text(value);
        text.Style(ResourceStyle(L"SettingsHintTextStyle"));
        return text;
    }

    inline Controls::TextBlock SectionHeading(winrt::hstring const& value)
    {
        auto text = Text(value);
        text.Style(ResourceStyle(L"SettingsSectionHeadingStyle"));
        return text;
    }

    inline Controls::TextBlock PageHeading(winrt::hstring const& value)
    {
        auto text = Text(value);
        text.Style(ResourceStyle(L"SettingsPageHeadingStyle"));
        return text;
    }

    inline Controls::StackPanel SectionPanel()
    {
        Controls::StackPanel panel;
        panel.Style(ResourceStyle(L"SettingsSectionPanelStyle"));
        return panel;
    }

    inline Controls::StackPanel ButtonRow()
    {
        Controls::StackPanel panel;
        panel.Style(ResourceStyle(L"SettingsButtonRowStyle"));
        return panel;
    }

    inline Controls::Button SecondaryAction(hstring const& label)
    {
        Controls::Button button;
        button.Style(ResourceStyle(L"SettingsSecondaryButtonStyle"));
        button.Content(box_value(label));
        return button;
    }

    inline Controls::Border ListRow(Xaml::UIElement const& content)
    {
        Controls::Border row;
        row.Style(ResourceStyle(L"SettingsListSeparatorStyle"));
        row.Child(content);
        return row;
    }

    inline Controls::StackPanel PageHeader(winrt::hstring const& title, winrt::hstring const& description = {})
    {
        Controls::StackPanel header;
        header.Style(ResourceStyle(L"SettingsPageHeaderStyle"));
        header.Children().Append(PageHeading(title));
        if (!description.empty()) header.Children().Append(Hint(description));
        return header;
    }

    inline Controls::Grid PageViewport(Xaml::UIElement const& page)
    {
        auto viewport = Xaml::Application::Current().Resources().Lookup(
            winrt::box_value(L"SettingsPageViewportTemplate")).as<Xaml::DataTemplate>()
            .LoadContent().as<Controls::Grid>();
        viewport.FindName(L"PageContent").as<Controls::ContentPresenter>().Content(page);
        return viewport;
    }

    inline Controls::ScrollViewer PageScroll(Xaml::UIElement const& page)
    {
        Controls::ScrollViewer scroll;
        scroll.Style(ResourceStyle(L"SettingsPageScrollStyle"));
        scroll.Content(PageViewport(page));
        return scroll;
    }

    // The list owns scrolling and receives a finite viewport. Wrapping it in a
    // StackPanel/ScrollViewer would measure every row and defeat virtualization.
    inline Xaml::UIElement ListPage(Controls::StackPanel const& header,
        Controls::ListView const& list, Controls::TextBlock const& status)
    {
        header.Style(ResourceStyle(L"SettingsListHeaderStyle"));
        list.Header(PageViewport(header));
        list.ItemTemplate(Xaml::Application::Current().Resources().Lookup(
            winrt::box_value(L"SettingsListRowTemplate")).as<Xaml::DataTemplate>());
        list.ItemsPanel(Xaml::Application::Current().Resources().Lookup(
            winrt::box_value(L"SettingsListItemsPanel")).as<Controls::ItemsPanelTemplate>());
        list.ItemContainerStyle(ResourceStyle(L"SettingsListRowStyle"));
        list.SelectionMode(Controls::ListViewSelectionMode::None);
        list.IsItemClickEnabled(false);
        Controls::ScrollViewer::SetHorizontalScrollMode(list, Controls::ScrollMode::Disabled);
        Controls::ScrollViewer::SetHorizontalScrollBarVisibility(list, Controls::ScrollBarVisibility::Disabled);
        Controls::ScrollViewer::SetVerticalScrollBarVisibility(list, Controls::ScrollBarVisibility::Auto);
        Xaml::Controls::Grid page;
        Xaml::Controls::RowDefinition content;
        content.Height(Xaml::GridLengthHelper::FromValueAndType(1, Xaml::GridUnitType::Star));
        page.RowDefinitions().Append(content);
        Xaml::Controls::RowDefinition footer;
        footer.Height(Xaml::GridLengthHelper::Auto());
        page.RowDefinitions().Append(footer);
        page.Children().Append(list);
        Xaml::Controls::Grid::SetRow(status, 1);
        status.TextWrapping(Xaml::TextWrapping::Wrap);
        status.Margin(Xaml::Thickness{24, 0, 24, 0});
        status.MaxWidth(Metric(L"SettingsContentWidth"));
        status.HorizontalAlignment(Xaml::HorizontalAlignment::Stretch);
        page.Children().Append(status);
        return page;
    }

    inline void SetStatus(Controls::TextBlock const& text, winrt::hstring const& message, bool error)
    {
        text.Style(ResourceStyle(L"SettingsHintTextStyle"));
        text.ClearValue(Controls::TextBlock::ForegroundProperty());
        text.Text(message);
        if (error) text.Foreground(Xaml::Application::Current().Resources().Lookup(
            winrt::box_value(L"SystemFillColorCriticalBrush")).as<Xaml::Media::Brush>());
    }

    inline winrt::Windows::Foundation::Uri FileUri(std::filesystem::path path)
    {
        path = std::filesystem::absolute(path);
        return winrt::Windows::Foundation::Uri(
            L"file:///" + winrt::hstring(path.generic_wstring()));
    }


    inline Controls::StackPanel PagePanel()
    {
        Controls::StackPanel panel;
        panel.Style(ResourceStyle(L"SettingsPageStyle"));
        return panel;
    }

}
