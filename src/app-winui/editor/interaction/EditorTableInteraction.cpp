#include "pch.h"
#include "editor/interaction/EditorTableInteraction.h"
#include "localization/Localization.h"
#include <winrt/Windows.Globalization.h>

import folia.markdown.table_metadata;
import folia.markdown.utf;
import folia.core.table_values;

namespace winrt::Folia
{
    namespace
    {
        namespace Controls = winrt::Microsoft::UI::Xaml::Controls;
        using ExecuteCommand = EditorTableInteraction::ExecuteCommand;

        winrt::hstring Wide(std::u32string_view text)
        {
            return winrt::to_hstring(folia::cps_to_utf8(text));
        }

        void SubmitValue(const ExecuteCommand& execute, std::u32string value)
        {
            if (execute) execute({.kind = folia::CommandKind::SetTableCellValue, .text = std::move(value)});
        }

        winrt::fire_and_forget EditFormula(
            winrt::Microsoft::UI::Xaml::XamlRoot root, ExecuteCommand execute, std::u32string original)
        {
            Controls::ContentDialog dialog;
            dialog.XamlRoot(root);
            dialog.Title(winrt::box_value(Localize(L"TableFormula")));
            dialog.PrimaryButtonText(Localize(L"Apply"));
            dialog.CloseButtonText(Localize(L"Cancel"));
            Controls::StackPanel form;
            form.Spacing(8);
            Controls::TextBox formula;
            formula.PlaceholderText(L"{Price} * {Quantity}");
            formula.Text(Wide(original));
            form.Children().Append(formula);
            Controls::TextBlock error;
            error.TextWrapping(winrt::Microsoft::UI::Xaml::TextWrapping::Wrap);
            form.Children().Append(error);
            dialog.Content(form);
            dialog.PrimaryButtonClick([formula, error, execute, original = std::move(original)](auto const&, auto const& args) {
                auto value = folia::utf8_to_cps(winrt::to_string(formula.Text()));
                if (value == original) return;
                folia::Command command{folia::CommandKind::SetTableAppearance};
                command.table_format = folia::TableFormatEdit{.kind = folia::TableFormatKind::Formula, .text = std::move(value)};
                if (!execute || !execute(command)) {
                    error.Text(Localize(L"TableFormulaError"));
                    args.Cancel(true);
                }
            });
            try { co_await dialog.ShowAsync(); }
            catch (winrt::hresult_error const&) {}
        }

        winrt::fire_and_forget PickDate(
            winrt::Microsoft::UI::Xaml::XamlRoot root, ExecuteCommand execute, folia::TableCellDetails cell)
        {
            Controls::CalendarDatePicker picker;
            if (auto date = folia::table_date(cell.source))
                picker.Date(winrt::box_value(std::chrono::time_point_cast<winrt::Windows::Foundation::DateTime::duration>(
                    winrt::clock::from_sys(date->date + std::chrono::hours{12})))
                    .as<winrt::Windows::Foundation::IReference<winrt::Windows::Foundation::DateTime>>());
            Controls::ContentDialog dialog;
            dialog.XamlRoot(root);
            dialog.Title(winrt::box_value(Localize(L"TableTypeDate")));
            dialog.PrimaryButtonText(Localize(L"Apply"));
            dialog.CloseButtonText(Localize(L"Cancel"));
            dialog.Content(picker);
            try {
                if (co_await dialog.ShowAsync() == Controls::ContentDialogResult::Primary) {
                    if (auto date = picker.Date()) {
                        auto calendar = winrt::Windows::Globalization::Calendar();
                        calendar.SetDateTime(date.Value());
                        auto day = std::chrono::year{calendar.Year()}
                            / static_cast<unsigned>(calendar.Month()) / static_cast<unsigned>(calendar.Day());
                        SubmitValue(execute, folia::table_date_text(std::chrono::sys_days{day}, '-', true));
                    } else SubmitValue(execute, {});
                }
            } catch (winrt::hresult_error const&) {}
        }

        winrt::fire_and_forget PickTags(
            winrt::Microsoft::UI::Xaml::XamlRoot root, ExecuteCommand execute, folia::TableCellDetails cell)
        {
            Controls::ContentDialog dialog;
            dialog.XamlRoot(root);
            dialog.Title(winrt::box_value(Localize(L"TableTypeMulti")));
            dialog.PrimaryButtonText(Localize(L"Apply"));
            dialog.CloseButtonText(Localize(L"Cancel"));
            Controls::StackPanel panel;
            std::vector<std::pair<std::u32string, Controls::CheckBox>> choices;
            std::set<std::u32string_view> names;
            const auto tags = folia::table_tags(cell.source);
            for (auto tag : tags) names.insert(tag);
            if (cell.metadata && cell.address.column < cell.metadata->options.size())
                for (const auto& item : cell.metadata->options[cell.address.column]) names.insert(item.first);
            for (const auto name : names) {
                Controls::CheckBox choice;
                choice.Content(winrt::box_value(Wide(name)));
                choice.IsChecked(std::ranges::contains(tags, name));
                panel.Children().Append(choice);
                choices.emplace_back(std::u32string(name), choice);
            }
            Controls::TextBox extra;
            extra.PlaceholderText(Localize(L"TableNewTag"));
            panel.Children().Append(extra);
            Controls::ScrollViewer scroll;
            scroll.MaxHeight(360);
            scroll.Content(panel);
            dialog.Content(scroll);
            try {
                if (co_await dialog.ShowAsync() == Controls::ContentDialogResult::Primary) {
                    std::u32string source;
                    auto append = [&](std::u32string_view tag) {
                        if (tag.empty()) return;
                        if (!source.empty()) source += U", ";
                        source += tag;
                    };
                    for (const auto& [name, choice] : choices)
                        if (auto checked = choice.IsChecked(); checked && checked.Value()) append(name);
                    append(folia::utf8_to_cps(winrt::to_string(extra.Text())));
                    SubmitValue(execute, std::move(source));
                }
            } catch (winrt::hresult_error const&) {}
        }

        winrt::fire_and_forget EditValue(
            winrt::Microsoft::UI::Xaml::XamlRoot root, ExecuteCommand execute, std::u32string value)
        {
            Controls::TextBox entry;
            entry.Text(Wide(value));
            Controls::ContentDialog dialog;
            dialog.XamlRoot(root);
            dialog.Title(winrt::box_value(Localize(L"TableEditValue")));
            dialog.PrimaryButtonText(Localize(L"Apply"));
            dialog.CloseButtonText(Localize(L"Cancel"));
            dialog.Content(entry);
            try {
                if (co_await dialog.ShowAsync() == Controls::ContentDialogResult::Primary)
                    SubmitValue(execute, folia::utf8_to_cps(winrt::to_string(entry.Text())));
            } catch (winrt::hresult_error const&) {}
        }
    }

    winrt::fire_and_forget EditorTableInteraction::InsertPicker(
        winrt::Microsoft::UI::Xaml::XamlRoot root, ExecuteCommand execute)
    {
        Controls::ContentDialog dialog;
        dialog.XamlRoot(root);
        dialog.Title(winrt::box_value(Localize(L"Table")));
        dialog.PrimaryButtonText(Localize(L"Insert"));
        dialog.CloseButtonText(Localize(L"Cancel"));
        Controls::StackPanel fields;
        fields.Spacing(10);
        auto dimension = [&](std::wstring_view header, double initial) {
            Controls::NumberBox field;
            field.Header(winrt::box_value(Localize(header)));
            field.Minimum(1.0);
            field.Maximum(1000.0);
            field.Value(initial);
            field.SpinButtonPlacementMode(Controls::NumberBoxSpinButtonPlacementMode::Inline);
            fields.Children().Append(field);
            return field;
        };
        const auto rows = dimension(L"TableRows", 2);
        const auto columns = dimension(L"TableColumns", 3);
        dialog.Content(fields);
        dialog.PrimaryButtonClick([rows, columns](auto const&, auto const& args) {
            auto valid = [](double value) { return std::isfinite(value) && value >= 1 && value == std::floor(value); };
            args.Cancel(!valid(rows.Value()) || !valid(columns.Value()));
        });
        try {
            if (co_await dialog.ShowAsync() == Controls::ContentDialogResult::Primary && execute)
                execute({.kind = folia::CommandKind::InsertTable,
                    .rows = static_cast<std::size_t>(rows.Value()), .cols = static_cast<std::size_t>(columns.Value())});
        } catch (winrt::hresult_error const&) {}
    }

    Microsoft::UI::Xaml::Controls::MenuFlyout EditorTableInteraction::CreateMenu(
        Microsoft::UI::Xaml::XamlRoot const& root, Microsoft::UI::Xaml::Style const& style,
        ExecuteCommand const& executeCommand, const std::optional<folia::TableCellDetails>& cell,
        MenuScope scope, ClipboardActions const& clipboard, std::optional<folia::TableCellSelection> selection)
    {
        namespace Controls = winrt::Microsoft::UI::Xaml::Controls;
        auto menu = Controls::MenuFlyout();
        if (style) menu.MenuFlyoutPresenterStyle(style);
        auto commandItem = [execute = executeCommand](auto items, std::wstring_view label, folia::Command command) {
            Controls::MenuFlyoutItem item;
            item.Text(Localize(label));
            item.Click([execute, command = std::move(command)](auto const&, auto const&) {
                if (execute) execute(command);
            });
            items.Append(item);
        };
        auto formatItem = [&](auto items, std::wstring_view label, folia::TableFormatEdit format) {
            folia::Command command{folia::CommandKind::SetTableAppearance};
            command.table_format = format;
            commandItem(items, label, std::move(command));
        };
        auto submenu = [&](std::wstring_view label) {
            Controls::MenuFlyoutSubItem item;
            item.Text(Localize(label));
            menu.Items().Append(item);
            return item.Items();
        };
        using Kind = folia::TableFormatKind;
        auto items = menu.Items();
        using Type = folia::TableColumnKind;
        const auto headerRows = cell && cell->metadata ? cell->metadata->header_rows : 1;
        constexpr std::array colors{L"TableGray", L"TableRed", L"TableOrange", L"TableYellow", L"TableGreen", L"TableBlue", L"TablePurple"};
        auto palette = [&](std::wstring_view label, Kind kind) {
            auto choices = submenu(label);
            formatItem(choices, L"TableStyleDefault", {.kind = kind});
            for (std::size_t index = 0; index < colors.size(); ++index)
                formatItem(choices, colors[index], {.kind = kind, .color = static_cast<folia::TableColor>(index)});
        };
        auto rowCommands = [&](auto rows) {
            commandItem(rows, L"TableInsertAbove", {folia::CommandKind::InsertTableRowAbove});
            commandItem(rows, L"TableInsertBelow", {folia::CommandKind::InsertTableRowBelow});
            commandItem(rows, L"TableDuplicateRows", {folia::CommandKind::DuplicateTableRows});
            if (!cell || !cell->header) commandItem(rows, L"TableDeleteRow", {folia::CommandKind::DeleteTableRow});
        };
        auto columnCommands = [&](auto columns) {
            commandItem(columns, L"TableInsertLeft", {folia::CommandKind::InsertTableColumnLeft});
            commandItem(columns, L"TableInsertRight", {folia::CommandKind::InsertTableColumnRight});
            commandItem(columns, L"TableDuplicateColumns", {folia::CommandKind::DuplicateTableColumns});
            commandItem(columns, L"TableDeleteColumn", {folia::CommandKind::DeleteTableColumn});
        };
        auto columnSettings = [&] {
            auto columns = items;
            commandItem(columns, L"TableAutoColumnWidth", {folia::CommandKind::SetTableColumnWidth});
            if (!cell || !cell->metadata || std::ranges::none_of(cell->metadata->merges, [&](const auto& merge) {
                    return merge.first.row != merge.last.row && merge.last.row >= headerRows;
                })) {
                commandItem(columns, L"TableSortAscending", {folia::CommandKind::SortTableRows});
                commandItem(columns, L"TableSortDescending", {.kind = folia::CommandKind::SortTableRows, .table_index = 1});
            }
            auto types = submenu(L"TableColumnType");
            auto addType = [&](std::wstring_view label, folia::TableColumnType type) {
                formatItem(types, label, {.kind = Kind::ColumnType, .column_type = type});
            };
            addType(L"TableTypeText", {});
            addType(L"TableTypeStatus", {.kind = Type::Status});
            addType(L"TableTypeMulti", {.kind = Type::Multi});
            addType(L"TableTypeDate", {.kind = Type::Date});
            addType(L"TableTypeDateSlash", {.kind = Type::Date, .date_format = folia::TableDateFormat::Slash});
            addType(L"TableTypeDateLink", {.kind = Type::Date, .date_format = folia::TableDateFormat::Link});
            addType(L"TableTypeNumber", {.kind = Type::Number});
            addType(L"TableTypeThousands", {.kind = Type::Number, .grouping = true});
            addType(L"TableTypePercent", {.kind = Type::Number, .percent = true});
            addType(L"TableTypeYuan", {.kind = Type::Number, .decimals = 2, .currency = U'¥', .grouping = true});
            addType(L"TableTypeDollar", {.kind = Type::Number, .decimals = 2, .currency = U'$', .grouping = true});
            addType(L"TableTypeProgress", {.kind = Type::Progress});
            auto totals = submenu(L"TableTotal");
            constexpr std::array totalLabels{L"TableTotalNone", L"TableTotalSum", L"TableTotalAverage", L"TableTotalCount", L"TableTotalMax", L"TableTotalMin"};
            for (std::size_t index = 0; index < totalLabels.size(); ++index)
                formatItem(totals, totalLabels[index], {.kind = Kind::Total, .total = static_cast<folia::TableTotal>(index)});
            Controls::MenuFlyoutItem formula;
            formula.Text(Localize(L"TableFormula"));
            auto savedFormula = cell && cell->metadata && cell->address.column < cell->metadata->formulas.size()
                ? cell->metadata->formulas[cell->address.column] : std::u32string{};
            formula.Click([root, executeCommand, savedFormula](auto const&, auto const&) {
                EditFormula(root, executeCommand, savedFormula);
            });
            items.Append(formula);
            if (!savedFormula.empty()) {
                auto decimals = submenu(L"TableFormulaDecimals");
                formatItem(decimals, L"TableStyleDefault", {.kind = Kind::FormulaDecimals});
                for (auto value : {0U, 1U, 2U, 3U}) {
                    Controls::MenuFlyoutItem item;
                    item.Text(winrt::to_hstring(value));
                    item.Click([executeCommand, value](auto const&, auto const&) {
                        if (executeCommand) executeCommand({.kind = folia::CommandKind::SetTableAppearance,
                            .table_format = folia::TableFormatEdit{.kind = Kind::FormulaDecimals, .decimals = value}});
                    });
                    decimals.Append(item);
                }
                formatItem(items, L"TableStopFormula", {.kind = Kind::Formula});
            }
        };
        auto alignmentCommands = [&] {
            auto horizontal = submenu(L"TableHorizontalAlignment");
            using Align = folia::TableAlignment;
            commandItem(horizontal, L"TableAlignLeft", {.kind = folia::CommandKind::SetTableColumnAlignment, .table_alignment = Align::Left});
            commandItem(horizontal, L"TableAlignMiddle", {.kind = folia::CommandKind::SetTableColumnAlignment, .table_alignment = Align::Center});
            commandItem(horizontal, L"TableAlignRight", {.kind = folia::CommandKind::SetTableColumnAlignment, .table_alignment = Align::Right});
            auto alignment = submenu(L"TableVerticalAlignment");
            formatItem(alignment, L"TableAlignTop", {.kind = Kind::VerticalAlignment, .alignment = folia::TableVerticalAlignment::Top});
            formatItem(alignment, L"TableAlignMiddle", {.kind = Kind::VerticalAlignment, .alignment = folia::TableVerticalAlignment::Middle});
            formatItem(alignment, L"TableAlignBottom", {.kind = Kind::VerticalAlignment, .alignment = folia::TableVerticalAlignment::Bottom});
        };
        auto colorsForSelection = [&] {
            palette(L"TableBackground", Kind::Background);
            palette(L"TableForeground", Kind::Foreground);
        };
        switch (scope)
        {
            case MenuScope::Table:
            {
                commandItem(items, L"TableCopy", {folia::CommandKind::CopyTable});
                commandItem(items, L"TableCopyMarkdown", {folia::CommandKind::CopyPlainTable});
                items.Append(Controls::MenuFlyoutSeparator());
                auto presets = submenu(L"TableStyle");
                constexpr std::array presetLabels{L"TableStyleDefault", L"TableStyleZebra", L"TableStyleMinimal", L"TableStyleAcademic"};
                for (std::size_t index = 0; index < presetLabels.size(); ++index)
                    formatItem(presets, presetLabels[index], {.kind = Kind::Preset, .preset = static_cast<folia::TablePreset>(index)});
                auto density = submenu(L"TableDensity");
                formatItem(density, L"TableDensityComfortable", {.kind = Kind::Density});
                formatItem(density, L"TableDensityCompact", {.kind = Kind::Density, .enabled = true});
                auto headers = submenu(L"TableHeaders");
                formatItem(headers, L"TableHeaderOne", {.kind = Kind::HeaderRows, .header_rows = 1});
                formatItem(headers, L"TableHeaderTwo", {.kind = Kind::HeaderRows, .header_rows = 2});
                formatItem(headers, L"TableHeaderThree", {.kind = Kind::HeaderRows, .header_rows = 3});
                formatItem(headers, L"TableHeaderColumn", {.kind = Kind::HeaderColumn, .enabled = true});
                formatItem(headers, L"TableNoHeaderColumn", {.kind = Kind::HeaderColumn});
                formatItem(headers, L"TableFreezeHeader", {.kind = Kind::FreezeHeader, .enabled = true});
                formatItem(headers, L"TableUnfreezeHeader", {.kind = Kind::FreezeHeader});
                auto layout = submenu(L"TableLayout");
                formatItem(layout, L"TableFold", {.kind = Kind::Folded, .enabled = true});
                formatItem(layout, L"TableUnfold", {.kind = Kind::Folded});
                formatItem(layout, L"TableWide", {.kind = Kind::Wide, .enabled = true});
                formatItem(layout, L"TableFit", {.kind = Kind::Wide});
                formatItem(layout, L"TableAutoWidths", {.kind = Kind::AutoWidths});
                formatItem(layout, L"TableClearColors", {.kind = Kind::ClearColors});
                auto tableSize = submenu(L"TableFontWhole");
                formatItem(tableSize, L"TableFontSmall", {.kind = Kind::TableFontScale, .font_scale = 0.85f});
                formatItem(tableSize, L"TableStyleDefault", {.kind = Kind::TableFontScale});
                formatItem(tableSize, L"TableFontLarge", {.kind = Kind::TableFontScale, .font_scale = 1.15f});
                commandItem(items, L"TableDelete", {folia::CommandKind::DeleteTable});
                break;
            }
            case MenuScope::Row:
                rowCommands(items);
                colorsForSelection();
                break;
            case MenuScope::Column:
                columnCommands(items);
                columnSettings();
                alignmentCommands();
                colorsForSelection();
                break;
            case MenuScope::Cell:
            {
                auto clipboardItem = [&](std::wstring_view label, const std::function<void()>& action) {
                    if (!action) return;
                    Controls::MenuFlyoutItem item;
                    item.Text(Localize(label));
                    item.Click([action](auto const&, auto const&) { action(); });
                    items.Append(item);
                };
                clipboardItem(L"Cut", clipboard.cut);
                clipboardItem(L"Copy", clipboard.copy);
                clipboardItem(L"Paste", clipboard.paste);
                if (items.Size()) items.Append(Controls::MenuFlyoutSeparator());
                colorsForSelection();
                const auto range = selection ? std::optional{selection->region} : std::nullopt;
                const bool multiple = range && range->first != range->last && (!cell || range != cell->merged);
                if (multiple && !(range->first.row < headerRows && range->last.row >= headerRows))
                    formatItem(items, L"TableMerge", {.kind = Kind::Merge});
                if (cell && cell->merged) formatItem(items, L"TableUnmerge", {.kind = Kind::Unmerge});
                items.Append(Controls::MenuFlyoutSeparator());
                rowCommands(submenu(L"TableRows"));
                columnCommands(submenu(L"TableColumns"));
                alignmentCommands();
                if (cell && !multiple && !cell->computed)
                    commandItem(items, cell->checkbox ? L"TableRemoveCheckbox" : L"TableMakeCheckbox",
                        {folia::CommandKind::MakeTableCheckbox});
                commandItem(items, L"Clear", {folia::CommandKind::DeleteSelection});
                if (cell && cell->column_type().kind == Type::Status && !cell->source.empty()) {
                    auto labelColors = submenu(L"TableStatusColor");
                    for (std::size_t index = 0; index < colors.size(); ++index)
                        formatItem(labelColors, colors[index], {.kind = Kind::OptionColor,
                            .color = static_cast<folia::TableColor>(index), .text = cell->source});
                }
                auto sizes = submenu(L"TableFontSize");
                formatItem(sizes, L"TableFontSmall", {.kind = Kind::FontScale, .font_scale = 0.85f});
                formatItem(sizes, L"TableStyleDefault", {.kind = Kind::FontScale});
                formatItem(sizes, L"TableFontLarge", {.kind = Kind::FontScale, .font_scale = 1.15f});
                break;
            }
        }
        return menu;
    }

    bool EditorTableInteraction::ShowValuePicker(
        winrt::Microsoft::UI::Xaml::FrameworkElement const& surface,
        winrt::Windows::Foundation::Point position, ExecuteCommand const& execute,
        const folia::TableCellDetails& cell, Microsoft::UI::Xaml::Style const& style)
    {
        const auto type = cell.column_type().kind;
        if (type == folia::TableColumnKind::Date)
        {
            PickDate(surface.XamlRoot(), execute, cell);
            return true;
        }
        if (type == folia::TableColumnKind::Multi)
        {
            PickTags(surface.XamlRoot(), execute, cell);
            return true;
        }
        if (type != folia::TableColumnKind::Status || !cell.metadata
            || cell.address.column >= cell.metadata->options.size() || cell.metadata->options[cell.address.column].empty())
            return false;
        auto menu = Controls::MenuFlyout();
        if (style) menu.MenuFlyoutPresenterStyle(style);
        for (const auto& [name, color] : cell.metadata->options[cell.address.column]) {
            Controls::MenuFlyoutItem item;
            item.Text(Wide(name));
            item.Click([execute, name](auto const&, auto const&) { SubmitValue(execute, name); });
            menu.Items().Append(item);
        }
        Controls::MenuFlyoutItem clear;
        clear.Text(Localize(L"TableTotalNone"));
        clear.Click([execute](auto const&, auto const&) { SubmitValue(execute, {}); });
        menu.Items().Append(clear);
        Controls::MenuFlyoutItem edit;
        edit.Text(Localize(L"TableEditValue"));
        edit.Click([root = surface.XamlRoot(), execute, value = cell.source](auto const&, auto const&) {
            EditValue(root, execute, value);
        });
        menu.Items().Append(edit);
        winrt::Microsoft::UI::Xaml::Controls::Primitives::FlyoutShowOptions options;
        options.Position(position);
        menu.ShowAt(surface, options);
        return true;
    }

    void EditorTableInteraction::Paint(
        EditorRenderResources& resources, EditorInteractionMap const& map,
        std::optional<D2D1_POINT_2F> pointer, std::optional<EditorTableAction> const& drag,
        std::optional<std::size_t> dropIndex)
    {
        using Kind = EditorTableActionKind;
        auto* context = resources.d2dContext.Get();
        auto line = [&](float left, float top, float right, float bottom, ID2D1Brush* brush, float width = 1.0f) {
            context->DrawLine(D2D1::Point2F(left, top), D2D1::Point2F(right, bottom), brush, width);
        };
        if (drag)
        {
            for (auto const& table : map.tables)
            {
                if (table.id != drag->tableId) continue;
                if (drag->kind == Kind::Fill && pointer && table.selection)
                {
                    if (auto target = map.TableFillTargetAt(table.id, pointer->x, pointer->y))
                    {
                        auto area = *table.selection;
                        auto distance = [](std::size_t at, std::size_t first, std::size_t last) {
                            return at < first ? first - at : at > last ? at - last : 0;
                        };
                        if (distance(target->row, area.first.row, area.last.row) >= distance(target->column, area.first.column, area.last.column))
                            target->column = area.last.column;
                        else target->row = area.last.row;
                        area.include({*target, *target});
                        const auto bounds = D2D1::RectF(table.columnBoundaries[area.first.column],
                            table.rowBoundaries[area.first.row], table.columnBoundaries[(std::min)(table.columnCount, area.last.column + 1)],
                            table.rowBoundaries[(std::min)(table.rowCount, area.last.row + 1)]);
                        context->DrawRectangle(bounds, resources.accentBrush.Get(), 1.5f);
                    }
                }
                else if (drag->kind == Kind::ResizeColumn && drag->index + 1 < table.columnBoundaries.size())
                {
                    auto x = table.columnBoundaries[drag->index + 1];
                    line(x, table.rect.top, x, table.rect.bottom, resources.accentBrush.Get(), 1.5f);
                }
                else if (dropIndex && drag->kind == Kind::DragRow && *dropIndex < table.rowBoundaries.size())
                {
                    auto y = table.rowBoundaries[*dropIndex];
                    line(table.rect.left, y, table.rect.right, y, resources.accentBrush.Get(), 2.0f);
                }
                else if (dropIndex && drag->kind == Kind::DragColumn && *dropIndex < table.columnBoundaries.size())
                {
                    auto x = table.columnBoundaries[*dropIndex];
                    line(x, table.rect.top, x, table.rect.bottom, resources.accentBrush.Get(), 2.0f);
                }
                return;
            }
        }
        for (const auto& table : map.tables)
        {
            if (!table.selectionHandle) continue;
            const auto [x, y] = *table.selectionHandle;
            context->FillRoundedRectangle(D2D1::RoundedRect({x - 4, y - 4, x + 4, y + 4}, 1.5f, 1.5f),
                resources.accentBrush.Get());
        }
        if (!pointer) return;
        const auto controls = map.TableControlsAt(pointer->x, pointer->y);
        const auto* hit = controls.Hit();
        for (auto const& control : controls.Visible())
        {
            const auto bounds = control.bounds;
            const auto x = std::midpoint(bounds.left, bounds.right);
            const auto y = std::midpoint(bounds.top, bounds.bottom);
            std::wstring_view glyph;
            switch (control.action.kind)
            {
                case Kind::ResizeColumn: glyph = L"table-resize"; break;
                case Kind::InsertRow:
                case Kind::InsertColumn: glyph = L"add"; break;
                case Kind::DragRow: glyph = L"table-row-grip"; break;
                case Kind::DragColumn: glyph = L"table-column-grip"; break;
                case Kind::Menu: glyph = L"more"; break;
                default: continue;
            }
            const auto button = D2D1::RoundedRect({x - 12, y - 12, x + 12, y + 12}, 5, 5);
            context->FillRoundedRectangle(button, resources.previewBackgroundBrush.Get());
            if (&control == hit) context->FillRoundedRectangle(button, resources.selectionBrush.Get());
            resources.chromeIcons.Draw(context, glyph, {x - 9, y - 9, x + 9, y + 9},
                (&control == hit ? resources.accentBrush : resources.textBrush)->GetColor());
        }
    }
}
