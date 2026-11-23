// Portable input intent and command fallback policy. Native hosts provide
// gestures, measured visual navigation and clipboard services only.
export module folia.core.input_command;
import std;
import folia.markdown.text_edit;
export import folia.core.command;
export import folia.core.input;

export namespace folia {

enum class EditorInputActionKind {
    None, ExecuteCommand, ExecuteCommandIfApplied, Copy, Cut, Paste,
    VisualLineStart, VisualLineEnd, VisualLineUp, VisualLineDown,
    VisualPageUp, VisualPageDown, TabForward, TabBackward,
};

struct EditorInputAction {
    EditorInputActionKind kind = EditorInputActionKind::None;
    Command command{};
    bool Handled() const noexcept { return kind != EditorInputActionKind::None; }
};

inline TextSelection editor_pointer_selection(
    TextSelection const& current, TextPosition target, bool extend) {
    return extend ? TextSelection{current.anchor, target} : TextSelection::caret(target);
}

inline EditorInputAction translate_editor_editing_key(KeyEvent const& key) {
    // Modified shortcuts belong to the host's configurable binding table. In
    // particular, an unset shortcut must not be revived by a second mapping,
    // and AltGr must remain available for text input.
    if (key.modifiers.ctrl || key.modifiers.alt || key.modifiers.meta) return {};
    auto action = EditorInputAction{};
    action.kind = EditorInputActionKind::ExecuteCommand;
    action.command.extend_selection = key.modifiers.shift;
    switch (key.key_code) {
        case KeyCode::Backspace: action.command.kind = CommandKind::DeleteBackward; break;
        case KeyCode::Delete: action.command.kind = CommandKind::DeleteForward; break;
        case KeyCode::Enter:
            action.kind = EditorInputActionKind::ExecuteCommandIfApplied;
            action.command.kind = key.modifiers.shift ? CommandKind::InsertSoftBreak : CommandKind::InsertNewline;
            break;
        case KeyCode::Left: action.command.kind = CommandKind::MoveLeft; break;
        case KeyCode::Right: action.command.kind = CommandKind::MoveRight; break;
        case KeyCode::Up:
            action.kind = EditorInputActionKind::VisualLineUp;
            action.command.kind = CommandKind::MoveUp; break;
        case KeyCode::Down:
            action.kind = EditorInputActionKind::VisualLineDown;
            action.command.kind = CommandKind::MoveDown; break;
        case KeyCode::Home:
            action.kind = EditorInputActionKind::VisualLineStart;
            action.command.kind = CommandKind::MoveLineStart; break;
        case KeyCode::End:
            action.kind = EditorInputActionKind::VisualLineEnd;
            action.command.kind = CommandKind::MoveLineEnd; break;
        case KeyCode::PageUp:
            action.kind = EditorInputActionKind::VisualPageUp;
            action.command.kind = CommandKind::MovePageUp; break;
        case KeyCode::PageDown:
            action.kind = EditorInputActionKind::VisualPageDown;
            action.command.kind = CommandKind::MovePageDown; break;
        case KeyCode::Tab:
            action.kind = key.modifiers.shift ? EditorInputActionKind::TabBackward : EditorInputActionKind::TabForward;
            action.command.kind = key.modifiers.shift ? CommandKind::TabBackward : CommandKind::TabForward;
            break;
        default: return {};
    }
    return action;
}

// Default bindings for clients without a configurable shortcut service. The
// WinUI host resolves its user-editable bindings instead of invoking this.
inline std::optional<Command> default_editor_key_command(KeyEvent const& key) {
    if (key.modifiers.alt || key.modifiers.meta) return std::nullopt;
    Command command;
    command.extend_selection = key.modifiers.shift;
    if (key.modifiers.ctrl) {
        switch (key.key_code) {
            case KeyCode::Left: command.kind = CommandKind::MoveWordLeft; return command;
            case KeyCode::Right: command.kind = CommandKind::MoveWordRight; return command;
            case KeyCode::Up: command.kind = CommandKind::MoveParagraphStart; return command;
            case KeyCode::Down: command.kind = CommandKind::MoveParagraphEnd; return command;
            case KeyCode::Home: command.kind = CommandKind::MoveDocumentStart; return command;
            case KeyCode::End: command.kind = CommandKind::MoveDocumentEnd; return command;
            case KeyCode::Backspace: command.kind = CommandKind::DeleteWordBackward; return command;
            case KeyCode::Delete: command.kind = CommandKind::DeleteWordForward; return command;
            case KeyCode::Char:
                switch (key.ch) {
                    case U'b': case U'B': command.kind = CommandKind::ToggleStrong; return command;
                    case U'i': case U'I': command.kind = CommandKind::ToggleEmphasis; return command;
                    case U'z': case U'Z': command.kind = key.modifiers.shift ? CommandKind::Redo : CommandKind::Undo; return command;
                    case U'y': case U'Y': command.kind = CommandKind::Redo; return command;
                    case U'c': case U'C': command.kind = CommandKind::Copy; return command;
                    case U'x': case U'X': command.kind = CommandKind::Cut; return command;
                    case U'v': case U'V': command.kind = CommandKind::Paste; return command;
                    case U'a': case U'A': command.kind = CommandKind::SelectAll; return command;
                    default: return std::nullopt;
                }
            default: return std::nullopt;
        }
    }
    if (key.key_code == KeyCode::Char) {
        if (key.ch < 0x20 || key.ch == 0x7f) return std::nullopt;
        command = Command::InsertText(std::u32string(1, key.ch));
        command.typed_input = true;
        return command;
    }
    auto action = translate_editor_editing_key(key);
    return action.Handled() ? std::optional{std::move(action.command)} : std::nullopt;
}

template <typename Execute>
bool execute_editor_tab(bool backward, Execute&& execute) {
    const std::array kinds = backward
        ? std::array{CommandKind::OutdentListItem, CommandKind::MoveTableCellPrevious, CommandKind::OutdentText}
        : std::array{CommandKind::IndentListItem, CommandKind::MoveTableCellNext, CommandKind::IndentText};
    for (auto kind : kinds) {
        if (std::invoke(execute, Command{.kind = kind})) return true;
    }
    return false;
}

} // namespace folia
