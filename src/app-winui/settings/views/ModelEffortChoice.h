#pragma once

#include "localization/Localization.h"

namespace winrt::Folia::settings_ui {
    using Microsoft::UI::Xaml::Controls::ComboBox;
    using Microsoft::UI::Xaml::Visibility;

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
}
