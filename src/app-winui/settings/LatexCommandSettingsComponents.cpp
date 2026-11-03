#include "pch.h"
#include "settings/LatexCommandSettingsComponents.h"
#include "settings/SettingsViewSupport.h"
#include "localization/Localization.h"

import folia.markdown.utf;
import folia.core.snippet_template;

namespace
{
    winrt::hstring Description(folia::LatexCommandDefinition const& command)
    {
        if (!command.built_in) return winrt::to_hstring(command.description);
        auto key = winrt::to_hstring(command.description);
        return winrt::Folia::Localize(std::wstring_view{key.c_str(), key.size()});
    }

    winrt::hstring Trigger(folia::LatexCommandDefinition const& command)
    {
        return L"\\" + winrt::to_hstring(folia::cps_to_utf8(command.trigger));
    }
}

namespace winrt::Folia
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    using settings_ui::Brush;
    using settings_ui::Text;

    FrameworkElement BuildLatexCommandSettingsRow(
        folia::LatexCommandDefinition const& command,
        double recentScore,
        std::function<void(std::string)> edit,
        std::function<void(std::string)> remove)
    {
        Grid row;
        row.MinHeight(54);
        row.Padding(Thickness{10, 6, 8, 6});
        row.ColumnSpacing(12);
        for (auto width : {0.75, 1.15, 1.8})
        {
            ColumnDefinition column;
            column.Width(GridLengthHelper::FromValueAndType(width, GridUnitType::Star));
            row.ColumnDefinitions().Append(column);
        }
        ColumnDefinition actionColumn;
        actionColumn.Width(GridLengthHelper::Auto());
        row.ColumnDefinitions().Append(actionColumn);

        StackPanel identity;
        identity.Spacing(4);
        auto trigger = Text(Trigger(command));
        trigger.Style(settings_ui::ResourceStyle(L"SettingsCodeLabelStyle"));
        identity.Children().Append(trigger);
        Border source;
        source.Style(settings_ui::ResourceStyle(L"SettingsBadgeStyle"));
        source.Child(settings_ui::Hint(Localize(command.built_in ? L"BuiltIn" : L"Custom")));
        identity.Children().Append(source);
        row.Children().Append(identity);

        StackPanel detail;
        detail.Spacing(3);
        auto description = Text(Description(command), 14);
        description.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
        detail.Children().Append(description);
        detail.Children().Append(settings_ui::Hint(recentScore > 0.01
            ? LocalizeFormat(L"LatexRecentScore", {
                winrt::to_hstring(std::format("{:.1f}", recentScore))})
            : Localize(L"LatexNotRecentlyUsed")));
        Grid::SetColumn(detail, 1);
        row.Children().Append(detail);

        auto snippetLiteral = folia::encode_snippet_literal(command.snippet);
        auto snippet = Text(winrt::to_hstring(folia::cps_to_utf8(snippetLiteral)));
        snippet.Style(settings_ui::ResourceStyle(L"SettingsCodeTextStyle"));
        snippet.TextTrimming(TextTrimming::CharacterEllipsis);
        snippet.MaxLines(1);
        snippet.VerticalAlignment(VerticalAlignment::Center);
        Grid::SetColumn(snippet, 2);
        row.Children().Append(snippet);

        if (!command.built_in)
        {
            Button more;
            FontIcon icon;
            icon.Glyph(L"\xE712");
            more.Content(icon);
            more.Padding(Thickness{8, 4, 8, 4});
            ToolTipService::SetToolTip(more, box_value(Localize(L"MoreActions")));
            MenuFlyout menu;
            MenuFlyoutItem editItem;
            editItem.Text(Localize(L"Edit"));
            editItem.Click([edit = std::move(edit), id = command.id](auto const&, auto const&)
            {
                if (edit) edit(id);
            });
            menu.Items().Append(editItem);
            MenuFlyoutItem removeItem;
            removeItem.Text(Localize(L"Remove"));
            removeItem.Click([remove = std::move(remove), id = command.id](auto const&, auto const&)
            {
                if (remove) remove(id);
            });
            menu.Items().Append(removeItem);
            more.Flyout(menu);
            more.HorizontalAlignment(HorizontalAlignment::Right);
            more.VerticalAlignment(VerticalAlignment::Center);
            Grid::SetColumn(more, 3);
            row.Children().Append(more);
        }

        Border separator;
        separator.Style(settings_ui::ResourceStyle(L"SettingsListSeparatorStyle"));
        separator.Child(row);
        return separator;
    }
}
