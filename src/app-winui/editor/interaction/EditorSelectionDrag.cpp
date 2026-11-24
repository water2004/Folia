#include "pch.h"
#include "editor/interaction/EditorSelectionDrag.h"

import folia.platform.editor_selection_drag_model;

namespace winrt::Folia
{
    void EditorSelectionDrag::Attach(
        EditorSession& valueSession,
        EditorSurfaceRenderer& valueRenderer,
        EditorScrollController& valueScroll,
        EditorTextInputController& valueTextInput)
    {
        Detach();
        session = &valueSession;
        renderer = &valueRenderer;
        scroll = &valueScroll;
        textInput = &valueTextInput;
    }

    void EditorSelectionDrag::Detach()
    {
        Cancel();
        textInput = nullptr;
        scroll = nullptr;
        renderer = nullptr;
        session = nullptr;
    }

    bool EditorSelectionDrag::Active() const
    {
        return active;
    }

    EditorSelectionDrag::MoveFeedback EditorSelectionDrag::Feedback() const
    {
        if (!move || !gesture.Dragging()) return MoveFeedback::None;
        return drop ? MoveFeedback::Allowed : MoveFeedback::Blocked;
    }

    void EditorSelectionDrag::Begin(folia::TextSelection initial, float x, float y)
    {
        if (!session) return;
        Cancel();
        active = true;
        anchor = initial.anchor;
        pointerX = x;
        pointerY = y;
        gesture.Begin(x, y);
        session->SetSelection(initial);
        if (textInput) textInput->NotifySelectionChanged();
    }

    void EditorSelectionDrag::BeginMove(folia::TextPosition position, float x, float y)
    {
        if (!session) return;
        Cancel();
        move = Move{position, session->Selection(), session->Revision()};
        pointerX = x;
        pointerY = y;
        gesture.Begin(x, y);
        active = true;
    }

    bool EditorSelectionDrag::MoveStillValid() const
    {
        return move && session && session->Revision() == move->revision
            && session->Selection() == move->selection;
    }

    bool EditorSelectionDrag::Update(float x, float y)
    {
        pointerX = x;
        pointerY = y;
        return active && gesture.Update(x, y) && UpdatePosition(x, y, true);
    }

    bool EditorSelectionDrag::Finish(float x, float y, std::function<bool(folia::Command const&)> const& execute)
    {
        if (!active) return false;
        pointerX = x;
        pointerY = y;
        auto dragged = gesture.Update(x, y);
        if (move)
        {
            auto valid = MoveStillValid();
            auto point = move->press;
            if (valid && dragged)
                UpdatePosition(x, y, false);
            else
                drop.reset();
            auto destination = drop;
            Cancel();
            if (destination && execute)
                return !execute({.kind = folia::CommandKind::MoveSelection, .move_destination = destination});
            if (valid && !dragged)
            {
                session->SetSelection(folia::TextSelection::caret(point));
                if (textInput) textInput->NotifySelectionChanged();
            }
            return true;
        }
        auto changed = dragged && UpdatePosition(x, y, false);
        Cancel();
        return changed;
    }

    void EditorSelectionDrag::Cancel()
    {
        StopAutoScroll();
        active = false;
        move.reset();
        drop.reset();
        if (renderer) renderer->SetSelectionDrop({});
    }

    bool EditorSelectionDrag::UpdatePosition(float x, float y, bool updateAutoScroll)
    {
        if (!active || !session || !renderer) return false;
        auto viewportHeight = renderer->ViewportHeight();
        auto width = renderer->ViewportWidth();
        auto projection = folia::platform::editor::ProjectEditorSelectionPointer(
            x,
            y,
            width,
            viewportHeight);
        if (updateAutoScroll)
        {
            auto velocity = projection.autoScrollVelocity;
            if (velocity != 0.0f && scroll)
            {
                autoScrolling = true;
                scroll->BeginSelectionAutoScroll(velocity, [this]
                {
                    UpdatePosition(pointerX, pointerY, false);
                });
            }
            else
            {
                StopAutoScroll();
            }
        }

        const auto insideViewport = x >= 0.0f && x <= width && y >= 0.0f && y <= viewportHeight;
        auto hit = !move || insideViewport ? renderer->HitTest(projection.hitX, projection.hitY) : std::nullopt;
        if (move)
        {
            drop = hit && MoveStillValid() && !session->SelectionCovers(*hit) ? hit : std::nullopt;
            return renderer->SetSelectionDrop(drop);
        }
        if (!hit) return false;
        auto selection = session->Selection();
        if (selection.anchor == anchor && selection.active == *hit) return false;
        session->SetSelection(anchor, *hit);
        if (textInput) textInput->NotifySelectionChanged();
        return true;
    }

    void EditorSelectionDrag::StopAutoScroll()
    {
        if (!autoScrolling) return;
        autoScrolling = false;
        if (scroll) scroll->EndSelectionAutoScroll();
    }
}
