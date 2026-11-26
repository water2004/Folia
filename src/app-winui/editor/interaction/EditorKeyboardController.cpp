#include "pch.h"
#include "editor/interaction/EditorKeyboardController.h"

import folia.platform.editor_input_command;
import folia.platform.editor_shortcuts;
import folia.platform.editor_snippet_session;
import folia.core.table_values;

namespace winrt::Folia
{
    void EditorKeyboardController::Attach(
        EditorSession& session,
        EditorSurfaceRenderer& renderer,
        EditorTextInputController& textInput,
        ExecuteCommand executeCommand,
        Action copy,
        Action cut,
        Action paste,
        ApplicationAction applicationAction,
        std::vector<folia::platform::editor::EditorShortcutBinding> shortcuts,
        Render render)
    {
        Detach();
        session_ = &session;
        renderer_ = &renderer;
        textInput_ = &textInput;
        executeCommand_ = std::move(executeCommand);
        copy_ = std::move(copy);
        cut_ = std::move(cut);
        paste_ = std::move(paste);
        applicationAction_ = std::move(applicationAction);
        shortcuts_ = std::move(shortcuts);
        render_ = std::move(render);
    }

    void EditorKeyboardController::Detach()
    {
        session_ = nullptr;
        renderer_ = nullptr;
        textInput_ = nullptr;
        executeCommand_ = {};
        copy_ = {};
        cut_ = {};
        paste_ = {};
        applicationAction_ = {};
        shortcuts_.clear();
        render_ = {};
        CancelSnippetSession();
        pendingHighSurrogate_.reset();
        ResetCaretGoal();
    }

    void EditorKeyboardController::SetShortcuts(
        std::vector<folia::platform::editor::EditorShortcutBinding> shortcuts)
    {
        shortcuts_ = std::move(shortcuts);
        CancelSnippetSession();
    }

    bool EditorKeyboardController::Character(char16_t character)
    {
        if (!session_ || !executeCommand_) return false;

        char32_t codepoint = character;
        if (character >= 0xd800 && character <= 0xdbff)
        {
            pendingHighSurrogate_ = character;
            return true;
        }
        if (character >= 0xdc00 && character <= 0xdfff)
        {
            if (pendingHighSurrogate_)
            {
                codepoint = 0x10000
                    + ((static_cast<char32_t>(*pendingHighSurrogate_) - 0xd800) << 10)
                    + (static_cast<char32_t>(character) - 0xdc00);
            }
            else
            {
                codepoint = 0xfffd;
            }
        }
        pendingHighSurrogate_.reset();

        if (codepoint == U'\r' || codepoint == U'\n') return false;
        if (codepoint < 0x20 || codepoint == 0x7f) return false;

        std::u32string text(1, codepoint);
        if (textInput_ && textInput_->ConsumeCommittedCoreTextCharacter(text)) return true;
        if (textInput_ && textInput_->IsComposing()) return false;

        auto selection = session_->Selection();
        auto start = session_->TextInputAcpOffset(selection.active);
        if (selection.anchor.container_id == selection.active.container_id)
        {
            start = (std::min)(start, session_->TextInputAcpOffset(selection.anchor));
        }
        auto command = folia::Command::InsertText(text);
        command.typed_input = true;
        if (!executeCommand_(command)) return false;
        if (textInput_) textInput_->RecordCharacterTextUpdate(start, std::move(text));
        return true;
    }

    bool EditorKeyboardController::Key(winrt::Windows::System::VirtualKey key)
    {
        return Key(Gesture(key));
    }

    folia::platform::editor::EditorKeyGesture EditorKeyboardController::Gesture(winrt::Windows::System::VirtualKey key)
    {
        auto ctrl = KeyDown(winrt::Windows::System::VirtualKey::Control)
            || KeyDown(winrt::Windows::System::VirtualKey::LeftControl)
            || KeyDown(winrt::Windows::System::VirtualKey::RightControl);
        auto shift = KeyDown(winrt::Windows::System::VirtualKey::Shift)
            || KeyDown(winrt::Windows::System::VirtualKey::LeftShift)
            || KeyDown(winrt::Windows::System::VirtualKey::RightShift);
        auto alt = KeyDown(winrt::Windows::System::VirtualKey::Menu)
            || KeyDown(winrt::Windows::System::VirtualKey::LeftMenu)
            || KeyDown(winrt::Windows::System::VirtualKey::RightMenu);
        return folia::platform::editor::EditorKeyGesture{
            .key = static_cast<folia::platform::editor::EditorKey>(
                static_cast<std::uint32_t>(key)),
            .control = ctrl,
            .shift = shift,
            .alt = alt,
        };
    }

    bool EditorKeyboardController::Key(folia::platform::editor::EditorKeyGesture gesture)
    {
        if (!session_ || !renderer_ || !executeCommand_) return false;
        if (textInput_ && textInput_->IsComposing()) return false;
        if (textInput_) textInput_->BeginHardwareKey();
        pendingHighSurrogate_.reset();
        if (auto shortcut = folia::platform::editor::resolve_editor_shortcut(
            shortcuts_, gesture, session_->ShortcutScope()))
            return DispatchShortcut(shortcuts_[*shortcut]);
        if (TableKey(gesture)) return true;
        return DispatchInputAction(
            folia::platform::editor::TranslateEditorKeyGesture(gesture));
    }

    bool EditorKeyboardController::InsertNewline()
    {
        if (!executeCommand_ || (textInput_ && textInput_->IsComposing())) return false;
        auto shift = KeyDown(winrt::Windows::System::VirtualKey::Shift)
            || KeyDown(winrt::Windows::System::VirtualKey::LeftShift)
            || KeyDown(winrt::Windows::System::VirtualKey::RightShift);
        const folia::platform::editor::EditorKeyGesture gesture{
                .key = folia::platform::editor::EditorKey::Enter,
                .shift = shift,
        };
        return TableKey(gesture) || DispatchInputAction(folia::platform::editor::TranslateEditorKeyGesture(gesture));
    }

    bool EditorKeyboardController::DispatchInputAction(
        folia::EditorInputAction const& action)
    {
        using folia::EditorInputActionKind;
        if (!executeCommand_) return false;
        switch (action.kind)
        {
            case EditorInputActionKind::None:
                return false;
            case EditorInputActionKind::ExecuteCommand:
                CancelSnippetSession();
                if (action.command.kind == folia::CommandKind::MoveLeft
                    || action.command.kind == folia::CommandKind::MoveRight
                    || action.command.kind == folia::CommandKind::MoveWordLeft
                    || action.command.kind == folia::CommandKind::MoveWordRight
                    || action.command.kind == folia::CommandKind::MoveParagraphStart
                    || action.command.kind == folia::CommandKind::MoveParagraphEnd
                    || action.command.kind == folia::CommandKind::MoveDocumentStart
                    || action.command.kind == folia::CommandKind::MoveDocumentEnd)
                    ResetCaretGoal();
                return executeCommand_(action.command);
            case EditorInputActionKind::ExecuteCommandIfApplied:
                CancelSnippetSession();
                if (!executeCommand_(action.command)) return false;
                if (textInput_
                    && (action.command.kind == folia::CommandKind::InsertNewline
                        || action.command.kind == folia::CommandKind::InsertSoftBreak))
                    textInput_->RecordSemanticLineBreakUpdate();
                return true;
            case EditorInputActionKind::Copy:
                if (!copy_) return false;
                copy_();
                return true;
            case EditorInputActionKind::Cut:
                CancelSnippetSession();
                if (!cut_) return false;
                cut_();
                return true;
            case EditorInputActionKind::Paste:
                CancelSnippetSession();
                if (!paste_) return false;
                paste_();
                return true;
            case EditorInputActionKind::VisualLineUp:
                CancelSnippetSession();
                return MoveCaretVerticalStep(false, action.command.extend_selection);
            case EditorInputActionKind::VisualLineDown:
                CancelSnippetSession();
                return MoveCaretVerticalStep(true, action.command.extend_selection);
            case EditorInputActionKind::VisualPageUp:
                CancelSnippetSession();
                return MoveCaretVerticalStep(false, action.command.extend_selection, true);
            case EditorInputActionKind::VisualPageDown:
                CancelSnippetSession();
                return MoveCaretVerticalStep(true, action.command.extend_selection, true);
            case EditorInputActionKind::VisualLineStart:
            case EditorInputActionKind::VisualLineEnd:
            {
                CancelSnippetSession();
                ResetCaretGoal();
                auto selection = session_->Selection();
                auto position = action.kind == EditorInputActionKind::VisualLineStart
                    ? renderer_->VisualLineStart(selection.active)
                    : renderer_->VisualLineEnd(selection.active);
                if (!position)
                {
                    return executeCommand_(action.command);
                }
                position->affinity = action.kind == EditorInputActionKind::VisualLineStart
                    ? folia::TextAffinity::Downstream
                    : folia::TextAffinity::Upstream;
                session_->SetSelection(
                    action.command.extend_selection ? selection.anchor : *position,
                    *position);
                if (textInput_) textInput_->NotifySelectionChanged();
                if (render_) render_();
                if (renderer_->ScrollToPosition(*position) && render_) render_();
                return true;
            }
            case EditorInputActionKind::TabBackward:
            case EditorInputActionKind::TabForward:
            {
                const auto backward = action.kind == EditorInputActionKind::TabBackward;
                if (MoveSnippetTabStop(backward)) return true;
                return executeCommand_(folia::Command{.kind = backward
                    ? folia::CommandKind::TabBackward : folia::CommandKind::TabForward});
            }
        }
        return false;
    }

    bool EditorKeyboardController::DispatchShortcut(
        folia::platform::editor::EditorShortcutBinding const& shortcut)
    {
        using folia::platform::editor::EditorShortcutActionKind;
        if (shortcut.action_kind == EditorShortcutActionKind::InsertSnippet)
            return InsertSnippet(shortcut.snippet);
        auto action = folia::platform::editor::editor_shortcut_input_action(shortcut.action_id);
        if (action.Handled()) return DispatchInputAction(action);
        CancelSnippetSession();
        if (!applicationAction_) return false;
        return applicationAction_(shortcut.action_id);
    }

    void EditorKeyboardController::ResetCaretGoal()
    {
        caretGoalX_ = -1.0f;
    }

    bool EditorKeyboardController::MoveCaretVerticalStep(bool down, bool extend, bool page)
    {
        if (!session_ || !renderer_) return false;
        auto selection = session_->Selection();
        auto move = page
            ? renderer_->MoveCaretByPage(selection.active, down, caretGoalX_)
            : renderer_->MoveCaretVertically(selection.active, down, caretGoalX_);
        if (extend)
            if (auto owner = session_->TableTextOwner(selection);
                owner && (!move || owner != session_->TableTextOwner(*move))) return true;
        if (!move || *move == selection.active)
        {
            ResetCaretGoal();
            if (!executeCommand_) return false;
            return executeCommand_(folia::Command{.kind = page
                ? (down ? folia::CommandKind::MovePageDown : folia::CommandKind::MovePageUp)
                : (down ? folia::CommandKind::MoveDown : folia::CommandKind::MoveUp),
                .extend_selection = extend});
        }
        session_->SetSelection(extend ? selection.anchor : *move, *move);
        if (textInput_) textInput_->NotifySelectionChanged();
        if (render_) render_();
        if (renderer_->ScrollToPosition(*move) && render_) render_();
        return true;
    }

    bool EditorKeyboardController::TableKey(folia::platform::editor::EditorKeyGesture gesture)
    {
        using Key = folia::platform::editor::EditorKey;
        using Command = folia::CommandKind;
        if (!session_ || !executeCommand_ || gesture.control || gesture.alt) return false;
        const auto selection = session_->Selection();
        const auto grid = selection.selects_cells();
        auto finishEditing = [&] {
            executeCommand_({Command::CommitTableCell});
            const auto active = session_->Selection().active;
            const folia::TextPosition cell{session_->TableTextOwner(active).value_or(active.container_id), 0};
            return folia::TextSelection::cells(cell, cell);
        };
        if (gesture.key == Key::Escape && !gesture.shift) {
            if (grid) {
                if (auto target = session_->TableEscapeSelection()) return SelectTablePosition(*target);
                return true;
            }
            if (session_->TableCellDetails(selection.active)) return SelectTablePosition(finishEditing());
            return false;
        }
        if (gesture.key == Key::Enter || gesture.key == Key::Space || gesture.key == Key::F2) {
            if (gesture.shift || (!grid && gesture.key != Key::Enter)) return false;
            auto cell = session_->TableCellDetails(selection.active);
            if (!cell) return false;
            if (!grid) {
                const auto next = finishEditing();
                session_->SetSelection(next);
                if (!executeCommand_({Command::MoveDown})) SelectTablePosition(next);
            } else if (gesture.key == Key::Space && cell->checkbox) {
                executeCommand_({Command::ToggleTableCheckbox});
            } else if (auto target = session_->TableEnterTarget()) {
                SelectTablePosition(folia::TextSelection::caret(*target));
            }
            return true;
        }
        if (gesture.key == Key::Tab) {
            if (!grid && !session_->TableCellDetails(selection.active)) return false;
            if (MoveSnippetTabStop(gesture.shift)) return true;
            if (!grid) session_->SetSelection(finishEditing());
            executeCommand_({gesture.shift ? Command::TabBackward : Command::TabForward});
            return true;
        }
        const auto move = [&]() -> std::optional<Command> {
            switch (gesture.key) {
                case Key::Left: return Command::MoveLeft;
                case Key::Right: return Command::MoveRight;
                case Key::Up: return Command::MoveUp;
                case Key::Down: return Command::MoveDown;
                default: return std::nullopt;
            }
        }();
        if (!move) return false;
        if (!grid) {
            if (gesture.shift || !selection.is_caret()) return false;
            const auto contents = session_->TableTextContents(selection.active);
            if (!contents) return false;
            const auto caret = selection.active;
            const auto boundary = gesture.key == Key::Left ? contents->anchor : contents->active;
            bool atEdge = caret.container_id == boundary.container_id && caret.source_offset == boundary.source_offset;
            if (gesture.key == Key::Up || gesture.key == Key::Down) {
                float goal = -1.0f;
                const auto visual = renderer_->MoveCaretVertically(caret, gesture.key == Key::Down, goal);
                atEdge = !visual || *visual == caret || session_->TableTextOwner(*visual) != session_->TableTextOwner(caret);
            }
            if (!atEdge) return false;
            session_->SetSelection(finishEditing());
        }
        executeCommand_({.kind = *move, .extend_selection = gesture.shift});
        return true;
    }

    bool EditorKeyboardController::SelectTablePosition(folia::TextSelection selection)
    {
        session_->SetSelection(selection);
        if (textInput_) textInput_->NotifySelectionChanged();
        if (auto cell = session_->TableAddress(selection.active))
            if (auto offset = renderer_->RevealTableColumn(*cell)) session_->SetTableScrollOffset(cell->table_id, *offset);
        if (render_) render_();
        if (renderer_->ScrollToPosition(selection.active) && render_) render_();
        return true;
    }

    bool EditorKeyboardController::KeyDown(winrt::Windows::System::VirtualKey key)
    {
        auto state = winrt::Microsoft::UI::Input::InputKeyboardSource::GetKeyStateForCurrentThread(key);
        return (static_cast<std::uint32_t>(state) & 0x1u) != 0;
    }
}
