#include "pch.h"
#include "editor/interaction/EditorTablePointerController.h"
#include "editor/interaction/EditorTableInteraction.h"
#include "editor/interaction/EditorScrollController.h"
#include "editor/interaction/EditorTextInputController.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/session/EditorSession.h"

import folia.platform.editor_selection_drag_model;

namespace winrt::Folia
{
    using Kind = folia::platform::editor::EditorTableActionKind;
    using Phase = folia::platform::editor::EditorTableGesture::Phase;

    void EditorTablePointerController::Attach(EditorSession& valueSession, EditorSurfaceRenderer& valueRenderer,
        EditorScrollController& valueScroll, EditorTextInputController& valueInput,
        Microsoft::UI::Xaml::FrameworkElement const& valueSurface, Microsoft::UI::Xaml::Style const& style,
        EditorTableInteraction::ClipboardActions actions, Execute handler, OpenLink link,
        std::function<void()> renderNow, std::function<void()> invalidate)
    {
        Detach();
        session = &valueSession;
        renderer = &valueRenderer;
        scroll = &valueScroll;
        textInput = &valueInput;
        surface = valueSurface;
        menuStyle = style;
        clipboard = std::move(actions);
        execute = std::move(handler);
        openLink = std::move(link);
        render = std::move(renderNow);
        requestFrame = std::move(invalidate);
        lifetime = std::make_shared<int>(0);
    }

    void EditorTablePointerController::Detach()
    {
        lifetime.reset();
        Cancel();
        surface = nullptr;
        menuStyle = nullptr;
        clipboard = {};
        session = nullptr;
        renderer = nullptr;
        textInput = nullptr;
        scroll = nullptr;
        execute = {};
        openLink = {};
        render = {};
        requestFrame = {};
    }

    Microsoft::UI::Xaml::Controls::MenuFlyout EditorTablePointerController::ContextMenuAt(std::optional<Point> point)
    {
        if (!session || !renderer || !surface) return nullptr;
        const auto prior = session->Selection();
        auto position = prior.active;
        if (point)
        {
            const auto hit = renderer->TableCellAt(point->X, point->Y);
            if (!hit) return nullptr;
            position = {hit->container_id, 0};
        }
        if (const auto owner = session->TableTextOwner(prior); owner && (!point || owner == position.container_id))
            return nullptr;
        const auto cell = session->TableCellDetails(position);
        if (!cell) return nullptr;
        const folia::TextPosition start{position.container_id, 0};
        if (!session->TableSelectionIncludes(start))
            ChangeSelection(folia::TextSelection::cells(start, start), SelectionUpdate::Immediate);
        auto valid = SelectionGuard();
        auto guard = [&](std::function<void()> const& action) -> std::function<void()> {
            if (!action) return {};
            return [valid, action] { if (valid()) action(); };
        };
        return EditorTableInteraction::CreateMenu(surface.XamlRoot(), menuStyle, CurrentCommand(), cell,
            Menu::Cell, {guard(clipboard.cut), guard(clipboard.copy), guard(clipboard.paste)}, session->TableSelection());
    }

    EditorTablePointerController::ControlSet EditorTablePointerController::Controls(Point point) const
    {
        return renderer ? renderer->TableControlsAt(point.X, point.Y) : ControlSet{};
    }

    bool EditorTablePointerController::ChangeSelection(folia::TextSelection selection, SelectionUpdate update)
    {
        if (selection == session->Selection()) return false;
        session->SetSelection(selection);
        pendingSelectionNotification = update == SelectionUpdate::Drag;
        if (update == SelectionUpdate::Immediate && render) render();
        else if (requestFrame) requestFrame();
        if (!pendingSelectionNotification) textInput->NotifySelectionChanged();
        return true;
    }

    bool EditorTablePointerController::Press(PointerArgs const& args, Point point)
    {
        if (press) return true;
        const auto controls = Controls(point);
        const auto pointer = args.Pointer();
        const auto prior = session->Selection();
        auto finishEditing = [&] {
            if (!prior.selects_cells() && session->CanCopyTable() && execute)
                execute({folia::CommandKind::CommitTableCell});
        };
        if (const auto* control = controls.Hit())
        {
            finishEditing();
            if (!surface.CapturePointer(pointer)) return true;
            press.emplace(Pressed{{pointer.PointerId(), point.X, point.Y}, session->Revision(), control->action});
            return true;
        }
        const auto cell = renderer->TableCellAt(point.X, point.Y);
        if (!cell) {
            finishEditing();
            return false;
        }
        const folia::TextPosition position{cell->container_id, 0};
        const bool editing = !prior.selects_cells() && session->TableTextOwner(prior.active) == position.container_id;
        if (editing) return false; // Regular source caret and text selection inside the active cell.
        if (openLink)
            if (auto hit = renderer->HitTestTextUnit(point.X, point.Y); hit && session->TableTextOwner(*hit) == position.container_id)
                if (auto interaction = session->InteractionAt(*hit); interaction && interaction->link)
                {
                    openLink(std::move(*interaction->link));
                    return true;
                }
        using Modifiers = Windows::System::VirtualKeyModifiers;
        const bool shift = (args.KeyModifiers() & Modifiers::Shift) == Modifiers::Shift;
        if (shift && prior.selects_cells() && session->SelectionInSameTable(position))
        {
            ChangeSelection(folia::TextSelection::cells(prior.anchor, position), SelectionUpdate::Immediate);
            return true;
        }
        finishEditing();
        ChangeSelection(folia::TextSelection::cells(position, position), SelectionUpdate::Immediate);
        if (auto address = session->TableAddress(position); address && surface.CapturePointer(pointer))
            press.emplace(Pressed{{pointer.PointerId(), point.X, point.Y}, session->Revision(), Range{position, address->table_id}});
        return true;
    }

    void EditorTablePointerController::ExtendRange(Range const& range, Point point, bool updateScroll)
    {
        const auto projection = folia::platform::editor::ProjectEditorSelectionPointer(
            point.X, point.Y, renderer->ViewportWidth(), renderer->ViewportHeight());
        if (updateScroll)
        {
            autoScrolling = projection.autoScrollVelocity != 0.0f;
            if (autoScrolling)
                scroll->BeginSelectionAutoScroll(projection.autoScrollVelocity, [this] {
                    if (press && press->revision == session->Revision())
                        if (auto active = std::get_if<Range>(&press->target)) ExtendRange(*active, lastPoint, false);
                });
            else scroll->EndSelectionAutoScroll();
        }
        if (auto cell = renderer->TableCellAt(projection.hitX, projection.hitY))
        {
            const folia::TextPosition position{cell->container_id, 0};
            if (auto address = session->TableAddress(position); address && address->table_id == range.table)
                ChangeSelection(folia::TextSelection::cells(range.anchor, position), SelectionUpdate::Drag);
        }
    }

    bool EditorTablePointerController::Move(PointerArgs const& args, Point point)
    {
        if (!press || !press->gesture.Owns(args.Pointer().PointerId())) return false;
        if (press->revision != session->Revision()) { Cancel(); return true; }
        lastPoint = point;
        if (auto range = std::get_if<Range>(&press->target))
        {
            ExtendRange(*range, point, true);
            return true;
        }
        if (press->gesture.Update(point.X, point.Y) != Phase::Dragging) return true;
        auto action = std::get<Action>(press->target);
        if (!action.draggable) return true;
        switch (action.kind)
        {
            case Kind::DragRow:
            case Kind::DragColumn:
                renderer->SetTableDrag(action, std::nullopt);
                dropIndex = renderer->TableDropIndexAt(point.X, point.Y, action.kind == Kind::DragRow);
                break;
            case Kind::Fill:
                break;
            case Kind::ResizeColumn:
                action.width = (std::clamp)(action.width + press->gesture.DeltaX(point.X), 40.0f, 1000000.0f);
                break;
            default: return true;
        }
        renderer->SetTableDrag(action, dropIndex);
        if (requestFrame) requestFrame();
        return true;
    }

    bool EditorTablePointerController::Release(PointerArgs const& args, Point point)
    {
        if (!press || !press->gesture.Owns(args.Pointer().PointerId())) return false;
        Move(args, point);
        if (!press)
        {
            surface.ReleasePointerCapture(args.Pointer());
            return true;
        }
        auto taken = *press;
        auto drop = dropIndex;
        Cancel();
        surface.ReleasePointerCapture(args.Pointer());
        const auto action = std::get_if<Action>(&taken.target);
        if (!action) return true;
        if (!action->draggable && taken.gesture.Update(point.X, point.Y) == Phase::Dragging) return true;
        if (taken.gesture.Update(point.X, point.Y) != Phase::Dragging)
        {
            const auto controls = Controls(point);
            const auto* hit = controls.Hit();
            if (!hit || hit->action != *action) return true;
            if (action->kind == Kind::DragRow || action->kind == Kind::DragColumn)
            {
                if (auto selection = renderer->TableAxisSelection(*action)) ChangeSelection(*selection);
                ShowMenu(point, action->kind == Kind::DragRow ? Menu::Row : Menu::Column,
                    action->sourcePosition, true);
            }
            else if (action->kind == Kind::Menu)
                ShowMenu(point, Menu::Table, action->sourcePosition, session->SelectionInSameTable(action->sourcePosition));
            else ApplyControl(*action);
            return true;
        }
        folia::Command command;
        switch (action->kind)
        {
            case Kind::Fill:
                command.kind = folia::CommandKind::FillTableSelection;
                command.table_fill_target = renderer->TableFillTargetAt(action->tableId, point.X, point.Y);
                command.table_copy_fill = (args.KeyModifiers() & Windows::System::VirtualKeyModifiers::Menu)
                    == Windows::System::VirtualKeyModifiers::Menu;
                if (command.table_fill_target && execute) execute(command);
                return true;
            case Kind::ResizeColumn:
                command.kind = folia::CommandKind::SetTableAppearance;
                command.table_format = renderer->TableResizeFormat(*action,
                    (std::clamp)(action->width + taken.gesture.DeltaX(point.X), 40.0f, 1000000.0f));
                if (!command.table_format) return true;
                break;
            case Kind::DragRow:
            case Kind::DragColumn:
                if (!drop) return true;
                command.kind = action->kind == Kind::DragRow
                    ? folia::CommandKind::MoveTableRowTo : folia::CommandKind::MoveTableColumnTo;
                command.table_index = *drop > action->index ? *drop - 1 : *drop;
                if (command.table_index == action->index) return true;
                break;
            default: return true;
        }
        ChangeSelection(folia::TextSelection::cells(action->sourcePosition, action->sourcePosition));
        if (execute) execute(command);
        return true;
    }

    bool EditorTablePointerController::ApplyControl(Action const& action)
    {
        auto index = action.index;
        folia::Command command;
        command.table_index = index;
        switch (action.kind)
        {
            case Kind::InsertRow: command.kind = folia::CommandKind::InsertTableRowAt; break;
            case Kind::InsertColumn: command.kind = folia::CommandKind::InsertTableColumnAt; break;
            case Kind::Checkbox: command.kind = folia::CommandKind::ToggleTableCheckbox; break;
            case Kind::Unfold:
                command.kind = folia::CommandKind::SetTableAppearance;
                command.table_format = {.kind = folia::TableFormatKind::Folded};
                break;
            default: return false;
        }
        ChangeSelection(folia::TextSelection::cells(action.sourcePosition, action.sourcePosition));
        return execute && execute(command);
    }

    bool EditorTablePointerController::DoubleTap(Point point)
    {
        const auto controls = Controls(point);
        if (const auto* control = controls.Hit())
        {
            if (control->action.kind != Kind::ResizeColumn) return true;
            const auto position = control->action.sourcePosition;
            ChangeSelection(folia::TextSelection::cells(position, position));
            if (execute) execute({folia::CommandKind::SetTableColumnWidth});
            return true;
        }
        const auto cell = renderer->TableCellAt(point.X, point.Y);
        if (!cell) return false;
        const auto prior = session->Selection();
        if (!prior.selects_cells() && session->TableTextOwner(prior.active) == cell->container_id) return false;
        const folia::TextPosition position{cell->container_id, 0};
        ChangeSelection(folia::TextSelection::cells(position, position));
        if (const auto details = session->TableCellDetails(position))
            if (EditorTableInteraction::ShowValuePicker(surface, point, CurrentCommand(), *details, menuStyle)) return true;
        if (auto target = session->TableEnterTarget())
        {
            auto hit = renderer->HitTest(point.X, point.Y);
            if (hit && session->TableTextOwner(*hit) == position.container_id) target = hit;
            ChangeSelection(folia::TextSelection::caret(*target), SelectionUpdate::Immediate);
        }
        return true;
    }

    std::function<bool()> EditorTablePointerController::SelectionGuard() const
    {
        return [weak = std::weak_ptr{lifetime}, session = session,
            revision = session->Revision(), selection = session->Selection()] {
            return !weak.expired() && session->Revision() == revision && session->Selection() == selection;
        };
    }

    EditorTablePointerController::Execute EditorTablePointerController::CurrentCommand() const
    {
        return [valid = SelectionGuard(), execute = execute](folia::Command const& command) {
            return valid() && execute && execute(command);
        };
    }

    void EditorTablePointerController::ShowMenu(Point point, Menu menu, folia::TextPosition position, bool preserveSelection)
    {
        if (!preserveSelection) ChangeSelection(folia::TextSelection::cells(position, position));
        auto flyout = EditorTableInteraction::CreateMenu(surface.XamlRoot(), menuStyle, CurrentCommand(),
            session->TableCellDetails(position), menu);
        Microsoft::UI::Xaml::Controls::Primitives::FlyoutShowOptions options;
        options.Position(point);
        flyout.ShowAt(surface, options);
    }

    bool EditorTablePointerController::Cancel()
    {
        if (autoScrolling && scroll) scroll->EndSelectionAutoScroll();
        autoScrolling = false;
        if (std::exchange(pendingSelectionNotification, false) && textInput) textInput->NotifySelectionChanged();
        if (!press) return false;
        const bool paint = std::holds_alternative<Action>(press->target);
        press.reset();
        dropIndex.reset();
        if (renderer) renderer->SetTableDrag({}, {});
        if (paint && requestFrame) requestFrame();
        return true;
    }
}
