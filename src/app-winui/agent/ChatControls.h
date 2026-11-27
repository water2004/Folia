#pragma once

#include "settings/storage/ModelCatalog.h"
#include "localization/Localization.h"
#include "settings/views/SettingsViewSupport.h"

namespace winrt::Folia::chat {
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    inline Button Action(wchar_t const* label) {
        Button button;
        button.Content(box_value(Localize(label)));
        return button;
    }

    inline void Put(Grid const& grid, FrameworkElement const& element, int row, int column = 0) {
        Grid::SetRow(element, row);
        Grid::SetColumn(element, column);
        grid.Children().Append(element);
    }

    inline void Rows(Grid const& grid, std::initializer_list<bool> star) {
        for (bool fill : star) {
            RowDefinition row;
            row.Height(fill ? GridLengthHelper::FromValueAndType(1, GridUnitType::Star) : GridLengthHelper::Auto());
            grid.RowDefinitions().Append(row);
        }
    }

    inline void Columns(Grid const& grid, std::initializer_list<bool> star) {
        for (bool fill : star) {
            ColumnDefinition column;
            column.Width(fill ? GridLengthHelper::FromValueAndType(1, GridUnitType::Star) : GridLengthHelper::Auto());
            grid.ColumnDefinitions().Append(column);
        }
    }

    inline void SetEffortChoices(ComboBox const& field, std::span<std::string const> choices, std::string_view selection,
                                 wchar_t const* defaultLabel = L"ChatModelDefaultEffort") {
        field.Items().Clear();
        field.Items().Append(box_value(Localize(defaultLabel)));
        int selected = 0;
        for (auto const& level : choices) {
            field.Items().Append(box_value(to_hstring(level)));
            if (level == selection) selected = static_cast<int>(field.Items().Size()) - 1;
        }
        field.SelectedIndex(selected);
        field.Visibility(choices.empty() ? Visibility::Collapsed : Visibility::Visible);
    }

    inline std::string SelectedEffort(ComboBox const& field) {
        return field.SelectedIndex() > 0 ? to_string(unbox_value<hstring>(field.SelectedItem())) : std::string{};
    }
    inline std::string FieldText(TextBox const& field) {
        auto bytes = to_string(field.Text());
        auto start = bytes.find_first_not_of(" \t\r\n");
        if (start == bytes.npos) return {};
        return bytes.substr(start, bytes.find_last_not_of(" \t\r\n") - start + 1);
    }

}
