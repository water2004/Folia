#pragma once

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
        list.Header(header);
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
        page.Style(ResourceStyle(L"SettingsGridPageStyle"));
        Xaml::Controls::RowDefinition content;
        content.Height(Xaml::GridLengthHelper::FromValueAndType(1, Xaml::GridUnitType::Star));
        page.RowDefinitions().Append(content);
        Xaml::Controls::RowDefinition footer;
        footer.Height(Xaml::GridLengthHelper::Auto());
        page.RowDefinitions().Append(footer);
        page.Children().Append(list);
        Xaml::Controls::Grid::SetRow(status, 1);
        status.TextWrapping(Xaml::TextWrapping::Wrap);
        page.Children().Append(status);
        return PageViewport(page);
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

    inline winrt::Windows::Foundation::Collections::IObservableVector<
        winrt::Windows::Foundation::IInspectable>
        ReadUtf8Lines(std::filesystem::path const& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) throw std::runtime_error("cannot open text asset");
        std::string source(
            std::istreambuf_iterator<char>{stream},
            std::istreambuf_iterator<char>{});
        if (source.starts_with("\xEF\xBB\xBF")) source.erase(0, 3);

        auto lines = winrt::single_threaded_observable_vector<
            winrt::Windows::Foundation::IInspectable>();
        std::size_t start = 0;
        while (start <= source.size())
        {
            auto end = source.find('\n', start);
            if (end == std::string::npos) end = source.size();
            auto length = end - start;
            if (length != 0 && source[start + length - 1] == '\r') --length;
            lines.Append(winrt::box_value(winrt::to_hstring(
                std::string_view{source}.substr(start, length))));
            if (end == source.size()) break;
            start = end + 1;
        }
        return lines;
    }

    inline Controls::NavigationViewItem NavigationItem(
        winrt::hstring const& label,
        winrt::hstring const& tag,
        wchar_t const* glyph)
    {
        Controls::NavigationViewItem item;
        item.Content(winrt::box_value(label));
        item.Tag(winrt::box_value(tag));
        Controls::FontIcon icon;
        icon.Glyph(glyph);
        item.Icon(icon);
        return item;
    }

    inline Controls::StackPanel PagePanel()
    {
        Controls::StackPanel panel;
        panel.Style(ResourceStyle(L"SettingsPageStyle"));
        return panel;
    }
}
