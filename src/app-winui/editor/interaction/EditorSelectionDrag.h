#pragma once

#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/interaction/EditorScrollController.h"
#include "editor/interaction/EditorTextInputController.h"

import folia.platform.editor_selection_drag_model;

namespace winrt::Folia
{
    // Owns one pointer gesture for extending or moving the selection.
    struct EditorSelectionDrag
    {
        void Attach(
            EditorSession& session,
            EditorSurfaceRenderer& renderer,
            EditorScrollController& scroll,
            EditorTextInputController& textInput);
        void Detach();
        bool Active() const;
        bool Moving() const { return move.has_value(); }
        void Begin(folia::TextSelection initial, float x, float y);
        void BeginMove(folia::TextPosition position, float x, float y);
        bool Update(float x, float y);
        bool Finish(float x, float y, std::function<bool(folia::Command const&)> const& execute = {});
        void Cancel();

    private:
        bool UpdatePosition(float x, float y, bool updateAutoScroll);
        void StopAutoScroll();
        bool MoveStillValid() const;

        EditorSession* session = nullptr;
        EditorSurfaceRenderer* renderer = nullptr;
        EditorScrollController* scroll = nullptr;
        EditorTextInputController* textInput = nullptr;
        folia::TextPosition anchor{};
        struct Move {
            folia::TextPosition press;
            folia::TextSelection selection;
            std::uint64_t revision;
        };
        std::optional<Move> move;
        std::optional<folia::TextPosition> drop;
        folia::platform::editor::EditorSelectionDragGesture gesture;
        float pointerX = 0.0f;
        float pointerY = 0.0f;
        bool active = false;
        bool autoScrolling = false;
    };
}
