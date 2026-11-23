#include "support/folia_test.hpp"

import folia.platform.editor_shortcuts;

using namespace boost::ut;
using namespace folia;
using namespace folia::platform::editor;

suite editor_shortcut_tests = [] {

"unset shortcut is disabled and exact scope overrides global"_test = [] {
    auto bindings = std::vector<EditorShortcutBinding>{
        {.id = "disabled", .action_id = "edit.copy"},
        {.id = "global", .action_id = "edit.copy", .gesture = EditorKeyGesture{EditorKey::B, true}},
        {.id = "math", .action_id = "custom", .scope = EditorShortcutScope::Math,
            .gesture = EditorKeyGesture{EditorKey::B, true}},
    };
    auto gesture = EditorKeyGesture{EditorKey::B, true};
    auto global = resolve_editor_shortcut(bindings, gesture, EditorShortcutScope::Global);
    auto code = resolve_editor_shortcut(bindings, gesture, EditorShortcutScope::Code);
    auto math = resolve_editor_shortcut(bindings, gesture, EditorShortcutScope::Math);
    expect(fatal(global.has_value()));
    expect(fatal(code.has_value()));
    expect(fatal(math.has_value()));
    expect(*global == 1_u);
    expect(*code == 1_u);
    expect(*math == 2_u);
    expect(!resolve_editor_shortcut(bindings, {EditorKey::C, true}, EditorShortcutScope::Global));
};

"conflicts are reported only inside the same scope"_test = [] {
    auto bindings = std::vector<EditorShortcutBinding>{
        {.id = "global", .gesture = EditorKeyGesture{EditorKey::B, true}},
        {.id = "code", .scope = EditorShortcutScope::Code,
            .gesture = EditorKeyGesture{EditorKey::B, true}},
    };
    auto gesture = EditorKeyGesture{EditorKey::B, true};
    auto global = find_editor_shortcut_conflict(
        bindings, gesture, EditorShortcutScope::Global);
    auto code = find_editor_shortcut_conflict(
        bindings, gesture, EditorShortcutScope::Code);
    expect(fatal(global.has_value()));
    expect(fatal(code.has_value()));
    expect(*global == 0_u);
    expect(*code == 1_u);
    expect(!find_editor_shortcut_conflict(bindings, gesture, EditorShortcutScope::Math));
    expect(!find_editor_shortcut_conflict(
        bindings, gesture, EditorShortcutScope::Global, std::size_t{0}));
};

"default shortcut catalog maps built in actions without legacy translation"_test = [] {
    auto bindings = default_editor_shortcuts();
    auto copy = resolve_editor_shortcut(
        bindings, {EditorKey::C, true}, EditorShortcutScope::Global);
    expect(fatal(copy.has_value()));
    expect(editor_shortcut_input_action(bindings[*copy].action_id).kind
        == EditorInputActionKind::Copy);
    expect(editor_shortcut_input_action("block.heading2").command.level == 2_u);
    auto inlineMath = std::ranges::find(
        bindings, std::string{"math.inline"}, &EditorShortcutBinding::action_id);
    expect(fatal(inlineMath != bindings.end()));
    expect(!inlineMath->gesture);
    expect(editor_shortcut_input_action(inlineMath->action_id).command.kind
        == folia::CommandKind::InsertMathInline);
    auto format = std::ranges::find(
        bindings, std::string{"format.document"}, &EditorShortcutBinding::action_id);
    expect(fatal(format != bindings.end()));
    expect(format->gesture == std::optional{EditorKeyGesture{
        EditorKey::F, false, true, true}});
    expect(editor_shortcut_input_action(format->action_id).command.kind
        == folia::CommandKind::FormatDocument);
    expect(!TranslateEditorKeyGesture({EditorKey::C, true}).Handled());
};

"workspace application shortcuts have distinct configurable gestures"_test = [] {
    auto bindings = default_editor_shortcuts();
    auto check = [&](EditorKeyGesture gesture, std::string_view action) {
        auto resolved = resolve_editor_shortcut(bindings, gesture, EditorShortcutScope::Global);
        expect(fatal(resolved.has_value()));
        expect(bindings[*resolved].action_id == action);
        expect(!find_editor_shortcut_conflict(bindings, gesture, EditorShortcutScope::Global, *resolved));
    };
    check({EditorKey::N, true}, "file.new");
    check({EditorKey::O, true}, "file.open");
    check({EditorKey::O, true, true}, "file.open_folder");
    check({EditorKey::W, true}, "file.close");
    check({EditorKey::Tab, true}, "view.next_tab");
    check({EditorKey::Tab, true, true}, "view.previous_tab");
};

"text navigation shortcuts do not dispatch table mutations outside tables"_test = [] {
    auto bindings = default_editor_shortcuts();
    struct Case { EditorKey key; CommandKind text; CommandKind table; };
    const Case cases[] = {
        {EditorKey::Left, CommandKind::MoveWordLeft, CommandKind::InsertTableColumnLeft},
        {EditorKey::Right, CommandKind::MoveWordRight, CommandKind::InsertTableColumnRight},
        {EditorKey::Up, CommandKind::MoveParagraphStart, CommandKind::InsertTableRowAbove},
        {EditorKey::Down, CommandKind::MoveParagraphEnd, CommandKind::InsertTableRowBelow},
        {EditorKey::Back, CommandKind::DeleteWordBackward, CommandKind::DeleteTableRow},
        {EditorKey::DeleteKey, CommandKind::DeleteWordForward, CommandKind::DeleteTableColumn},
    };
    for (auto item : cases) {
        for (auto scope : {EditorShortcutScope::Global, EditorShortcutScope::Code,
                EditorShortcutScope::Math, EditorShortcutScope::Table}) {
            auto found = resolve_editor_shortcut(bindings, {item.key, true}, scope);
            expect(fatal(found.has_value()));
            expect(editor_shortcut_input_action(bindings[*found].action_id).command.kind
                == (scope == EditorShortcutScope::Table ? item.table : item.text));
        }
    }
    for (auto key : {EditorKey::Left, EditorKey::Right, EditorKey::Up, EditorKey::Down}) {
        auto found = resolve_editor_shortcut(bindings, {key, true, true}, EditorShortcutScope::Table);
        expect(fatal(found.has_value()));
        expect(editor_shortcut_input_action(bindings[*found].action_id).command.extend_selection);
    }
    for (std::size_t i = 0; i < bindings.size(); ++i) {
        if (bindings[i].gesture) expect(!find_editor_shortcut_conflict(bindings,
            *bindings[i].gesture, bindings[i].scope, i));
    }
};

"saved stock table shortcuts migrate without reviving disabled or customized bindings"_test = [] {
    auto previous = default_editor_shortcuts();
    std::erase_if(previous, [](auto const& binding) {
        return binding.id.starts_with("nav.word_") || binding.id.starts_with("nav.paragraph_")
            || binding.id.starts_with("edit.delete_word_");
    });
    for (auto& binding : previous) if (binding.scope == EditorShortcutScope::Table)
        binding.scope = EditorShortcutScope::Global;
    auto merged = merge_editor_shortcut_settings(previous);
    auto find = [&](auto const& values, std::string_view id) -> auto const& {
        return *std::ranges::find(values, id, &EditorShortcutBinding::id);
    };
    expect(find(merged, "table.column_left").scope == EditorShortcutScope::Table);
    auto text = resolve_editor_shortcut(merged, {EditorKey::Left, true}, EditorShortcutScope::Global);
    expect(fatal(text.has_value()));
    expect(merged[*text].action_id == "nav.word_left");
    auto customized = previous;
    auto left = std::ranges::find(customized, std::string_view{"table.column_left"}, &EditorShortcutBinding::id);
    left->custom_name = "My table command";
    auto right = std::ranges::find(customized, std::string_view{"table.column_right"}, &EditorShortcutBinding::id);
    right->gesture.reset();
    auto retained = merge_editor_shortcut_settings(customized);
    expect(find(retained, "table.column_left") == *left);
    expect(!find(retained, "nav.word_left").gesture);
    expect(!find(retained, "table.column_right").gesture);
    expect(find(retained, "table.column_right").scope == EditorShortcutScope::Global);
    expect(find(retained, "nav.word_right").gesture.has_value());
};

"saved custom shortcuts take precedence over newly introduced defaults"_test = [] {
    std::vector<EditorShortcutBinding> saved{{.id = "user.left", .action_id = "custom",
        .custom_name = "Wrap", .action_kind = EditorShortcutActionKind::InsertSnippet,
        .gesture = EditorKeyGesture{EditorKey::Left, true}, .snippet = U"[$1]"}};
    auto merged = merge_editor_shortcut_settings(saved);
    auto found = resolve_editor_shortcut(merged, {EditorKey::Left, true}, EditorShortcutScope::Global);
    expect(fatal(found.has_value()));
    expect(merged[*found] == saved.front());
    saved.push_back(saved.front());
    expect(throws([&] { merge_editor_shortcut_settings(saved); }));
};

};
