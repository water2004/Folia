#pragma once

import folia.platform.editor_interaction;
import folia.core.command;
import folia.core.table_selection;

#include "editor/rendering/EditorRenderResources.h"

namespace winrt::Folia
{
    using folia::platform::editor::EditorInteractionMap;
    using folia::platform::editor::EditorTableAction;
    using folia::platform::editor::EditorTableActionKind;
    using folia::platform::editor::EditorVisualTable;

    struct EditorTableInteraction
    {
        using ExecuteCommand = std::function<bool(folia::Command const&)>;
        struct ClipboardActions { std::function<void()> cut, copy, paste; };
        enum class MenuScope { Cell, Row, Column, Table };
        static Microsoft::UI::Xaml::Controls::MenuFlyout CreateMenu(
            Microsoft::UI::Xaml::XamlRoot const& root, Microsoft::UI::Xaml::Style const& style,
            ExecuteCommand const& executeCommand, const std::optional<folia::TableCellDetails>& cell,
            MenuScope scope, ClipboardActions const& clipboard = {},
            std::optional<folia::TableCellSelection> selection = std::nullopt);
        static bool ShowValuePicker(winrt::Microsoft::UI::Xaml::FrameworkElement const& surface,
            winrt::Windows::Foundation::Point position, ExecuteCommand const& executeCommand,
            const folia::TableCellDetails& cell, Microsoft::UI::Xaml::Style const& style);
        static winrt::fire_and_forget InsertPicker(winrt::Microsoft::UI::Xaml::XamlRoot root, ExecuteCommand execute);
        static void Paint(EditorRenderResources& resources, EditorInteractionMap const& interactionMap, std::optional<D2D1_POINT_2F> pointerPosition, std::optional<EditorTableAction> const& draggedAction, std::optional<std::size_t> dropIndex);
    };
}
