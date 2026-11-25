#pragma once

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
    inline Controls::ComboBox Choice() { return winrt::make<ChoiceBox>().as<Controls::ComboBox>(); }

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

    inline Xaml::Style ResourceStyle(wchar_t const* key)
    {
        return Xaml::Application::Current().Resources().Lookup(winrt::box_value(key)).as<Xaml::Style>();
    }

    inline double Metric(wchar_t const* key)
    {
        return winrt::unbox_value<double>(Xaml::Application::Current().Resources().Lookup(winrt::box_value(key)));
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


    inline Controls::Border SectionCard(Xaml::UIElement const& child)
    {
        Controls::Border card;
        card.Style(ResourceStyle(L"SettingsSectionCardStyle"));
        card.Child(child);
        return card;
    }

    inline Controls::Grid OptionRow(winrt::hstring const& title, Xaml::FrameworkElement const& control,
        winrt::hstring const& description = {})
    {
        Controls::Grid row;
        row.Style(ResourceStyle(L"SettingsOptionRowStyle"));
        Controls::ColumnDefinition labels, value;
        labels.Width(Xaml::GridLengthHelper::FromValueAndType(3, Xaml::GridUnitType::Star));
        value.Width(Xaml::GridLengthHelper::FromValueAndType(2, Xaml::GridUnitType::Star));
        value.MaxWidth(Metric(L"SettingsOptionControlWidth"));
        row.ColumnDefinitions().Append(labels);
        row.ColumnDefinitions().Append(value);
        Controls::StackPanel copy;
        copy.VerticalAlignment(Xaml::VerticalAlignment::Center);
        copy.Children().Append(Text(title));
        if (!description.empty()) copy.Children().Append(Hint(description));
        row.Children().Append(copy);
        Controls::Grid::SetColumn(control, 1);
        control.VerticalAlignment(Xaml::VerticalAlignment::Center);
        Xaml::Automation::AutomationProperties::SetName(control, title);
        row.Children().Append(control);
        return row;
    }

    inline Controls::Expander OptionGroup(winrt::hstring const& title, Xaml::UIElement const& options)
    {
        Controls::Expander group;
        group.Style(ResourceStyle(L"SettingsOptionGroupStyle"));
        group.Header(Text(title));
        group.Content(options);
        group.IsExpanded(true);
        return group;
    }

    inline Controls::StackPanel PagePanel()
    {
        Controls::StackPanel panel;
        panel.Style(ResourceStyle(L"SettingsPageStyle"));
        return panel;
    }
}
