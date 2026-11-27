#pragma once

#include "localization/Localization.h"

namespace winrt::Folia::ui {
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    inline Style ResourceStyle(wchar_t const* key) {
        return Application::Current().Resources().Lookup(box_value(key)).as<Style>();
    }

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

    inline std::string FieldText(TextBox const& field) {
        auto bytes = to_string(field.Text());
        auto start = bytes.find_first_not_of(" \t\r\n");
        if (start == bytes.npos) return {};
        return bytes.substr(start, bytes.find_last_not_of(" \t\r\n") - start + 1);
    }

}
