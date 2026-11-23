#include "support/folia_test.hpp"

import folia.core.input_command;
import folia.core.editor;

using namespace boost::ut;
using namespace folia;

suite input_command_policy_tests = [] {

"portable input policy preserves semantic editing and selection gestures"_test = [] {
    struct Gesture { KeyCode key; CommandKind command; EditorInputActionKind action; };
    const Gesture gestures[] = {
        {KeyCode::Backspace, CommandKind::DeleteBackward, EditorInputActionKind::ExecuteCommand},
        {KeyCode::Delete, CommandKind::DeleteForward, EditorInputActionKind::ExecuteCommand},
        {KeyCode::Left, CommandKind::MoveLeft, EditorInputActionKind::ExecuteCommand},
        {KeyCode::Right, CommandKind::MoveRight, EditorInputActionKind::ExecuteCommand},
        {KeyCode::Up, CommandKind::MoveUp, EditorInputActionKind::VisualLineUp},
        {KeyCode::Down, CommandKind::MoveDown, EditorInputActionKind::VisualLineDown},
        {KeyCode::Home, CommandKind::MoveLineStart, EditorInputActionKind::VisualLineStart},
        {KeyCode::End, CommandKind::MoveLineEnd, EditorInputActionKind::VisualLineEnd},
        {KeyCode::PageUp, CommandKind::MovePageUp, EditorInputActionKind::VisualPageUp},
        {KeyCode::PageDown, CommandKind::MovePageDown, EditorInputActionKind::VisualPageDown},
    };
    for (auto gesture : gestures) for (bool extend : {false, true}) {
        KeyEvent key{.key_code = gesture.key, .modifiers = {.shift = extend}};
        auto action = translate_editor_editing_key(key);
        expect(action.kind == gesture.action);
        expect(action.command.kind == gesture.command);
        expect(action.command.extend_selection == extend);
        auto command = default_editor_key_command(key);
        expect(fatal(command.has_value()));
        expect(command->kind == gesture.command);
        expect(command->extend_selection == extend);
    }
    expect(translate_editor_editing_key({.key_code = KeyCode::Enter}).command.kind
        == CommandKind::InsertNewline);
    expect(translate_editor_editing_key({.key_code = KeyCode::Enter,
        .modifiers = {.shift = true}}).command.kind == CommandKind::InsertSoftBreak);
    expect(translate_editor_editing_key({.key_code = KeyCode::Tab}).command.kind
        == CommandKind::TabForward);
    expect(translate_editor_editing_key({.key_code = KeyCode::Tab,
        .modifiers = {.shift = true}}).command.kind == CommandKind::TabBackward);
};

"portable input policy leaves custom shortcuts and AltGr to their owner"_test = [] {
    for (auto key : {KeyCode::Left, KeyCode::Enter, KeyCode::Tab, KeyCode::Backspace}) {
        expect(!translate_editor_editing_key({.key_code = key,
            .modifiers = {.ctrl = true}}).Handled());
        expect(!translate_editor_editing_key({.key_code = key,
            .modifiers = {.alt = true}}).Handled());
        expect(!translate_editor_editing_key({.key_code = key,
            .modifiers = {.meta = true}}).Handled());
    }
    expect(!default_editor_key_command({.key_code = KeyCode::Char,
        .modifiers = {.ctrl = true, .alt = true}, .ch = U'@'}));
    expect(!default_editor_key_command({.key_code = KeyCode::Char,
        .modifiers = {.alt = true}, .ch = U'x'}));
    expect(!default_editor_key_command({.key_code = KeyCode::Char, .ch = U'\0'}));
    expect(!default_editor_key_command({.key_code = KeyCode::Unknown}));
};

"portable input policy routes modified movement and deletion to text commands"_test = [] {
    const std::pair<KeyCode, CommandKind> commands[] = {
        {KeyCode::Left, CommandKind::MoveWordLeft},
        {KeyCode::Right, CommandKind::MoveWordRight},
        {KeyCode::Up, CommandKind::MoveParagraphStart},
        {KeyCode::Down, CommandKind::MoveParagraphEnd},
        {KeyCode::Backspace, CommandKind::DeleteWordBackward},
        {KeyCode::Delete, CommandKind::DeleteWordForward},
        {KeyCode::Home, CommandKind::MoveDocumentStart},
        {KeyCode::End, CommandKind::MoveDocumentEnd},
    };
    for (auto [key, kind] : commands) for (bool extend : {false, true}) {
        auto command = default_editor_key_command({.key_code = key,
            .modifiers = {.ctrl = true, .shift = extend}});
        expect(fatal(command.has_value()));
        expect(command->kind == kind);
        expect(command->extend_selection == extend);
    }
};

"portable input policy only consumes a tab after a command accepts it"_test = [] {
    for (bool backward : {false, true}) {
        std::vector<CommandKind> attempted;
        auto apply = [&](Command const& command) {
            attempted.push_back(command.kind);
            return command.kind == (backward ? CommandKind::OutdentText : CommandKind::IndentText);
        };
        expect(execute_editor_tab(backward, apply));
        expect(attempted.size() == 3_u);
        expect(attempted[0] == (backward ? CommandKind::OutdentListItem : CommandKind::IndentListItem));
        expect(attempted[1] == (backward ? CommandKind::MoveTableCellPrevious : CommandKind::MoveTableCellNext));
        attempted.clear();
        expect(!execute_editor_tab(backward, [&](Command const& command) {
            attempted.push_back(command.kind);
            return false;
        }));
        expect(attempted.size() == 3_u);
        attempted.clear();
        expect(execute_editor_tab(backward, [&](Command const& command) {
            attempted.push_back(command.kind);
            return true;
        }));
        expect(attempted.size() == 1_u);
    }
};

"portable input policy dispatches tab and backtab through the production editor"_test = [] {
    Editor editor("alpha\nbeta");
    expect(editor.enter_source_mode());
    editor.set_source_selection({0, 6});
    const auto before = editor.text_utf8();
    auto tab = editor.handle_input({.kind = EditorInputEvent::Kind::KeyDown,
        .key = {.key_code = KeyCode::Tab}});
    expect(fatal(tab.has_value()));
    expect(fatal(editor.execute_command(*tab)));
    expect(editor.text_utf8() == "    alpha\nbeta");
    auto backtab = editor.handle_input({.kind = EditorInputEvent::Kind::KeyDown,
        .key = {.key_code = KeyCode::Tab, .modifiers = {.shift = true}}});
    expect(fatal(backtab.has_value()));
    expect(fatal(editor.execute_command(*backtab)));
    expect(editor.text_utf8() == before);
    expect(editor.undo());
    expect(editor.text_utf8() == "    alpha\nbeta");
    expect(editor.undo());
    expect(editor.text_utf8() == before);
};

};
