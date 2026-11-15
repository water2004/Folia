#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.core.source_editor;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.ids;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

namespace {
void source_command_local() {
    const auto counters = read_core_operation_counters();
    expect(counters.full_document_parses == 0_u);
    expect(counters.full_document_serializations == 0_u);
    expect(counters.full_tree_transaction_diffs == 0_u);
}
std::u32string source_command_roundtrip(Editor& editor, Command command) {
    const auto before = editor.source_editor()->source();
    const auto selected = editor.source_editor()->selection();
    reset_core_operation_counters();
    const auto applied = editor.execute_command(command);
    if (!applied) std::cerr << "source command rejected kind " << static_cast<int>(command.kind)
        << " before " << std::quoted(cps_to_utf8(before)) << '\n';
    expect(fatal(applied)) << static_cast<int>(command.kind);
    source_command_local();
    expect(validate_document(editor.document()).empty());
    const auto after = editor.source_editor()->source();
    const auto after_selection = editor.source_editor()->selection();
    expect(fatal(editor.undo()));
    expect(bool(editor.source_editor()->source() == before));
    expect(editor.source_editor()->selection() == selected);
    expect(fatal(editor.redo()));
    expect(bool(editor.source_editor()->source() == after));
    expect(editor.source_editor()->selection() == after_selection);
    expect(fatal(editor.exit_source_mode()));
    expect(editor.markdown_utf8() == cps_to_utf8(after));
    expect(validate_document(editor.document()).empty());
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == cps_to_utf8(before));
    expect(fatal(editor.redo()));
    expect(editor.markdown_utf8() == cps_to_utf8(after));
    expect(fatal(editor.enter_source_mode()));
    return after;
}
}

suite editor_source_commands = [] {

"source_command_inline_formatting_supports_selection_direction_and_toggle"_test = [] {
    const std::vector<std::pair<CommandKind, std::u32string>> cases{
        {CommandKind::ToggleStrong, U"**"}, {CommandKind::ToggleEmphasis, U"*"},
        {CommandKind::ToggleStrikethrough, U"~~"}, {CommandKind::ToggleInlineCode, U"`"},
        {CommandKind::InsertMathInline, U"$"}};
    for (const auto& [kind, marker] : cases)
    for (const auto reversed : {false, true}) {
        Editor editor("before\r\nword\r\nafter");
        expect(fatal(editor.enter_source_mode()));
        editor.set_source_selection(reversed ? SourceSelection{12, 8} : SourceSelection{8, 12});
        const auto after = source_command_roundtrip(editor, {.kind = kind});
        expect(bool(after == U"before\r\n" + marker + U"word" + marker + U"\r\nafter"));
        editor.set_source_selection(reversed ? SourceSelection{12 + marker.size(), 8 + marker.size()}
            : SourceSelection{8 + marker.size(), 12 + marker.size()});
        expect(fatal(editor.execute_command({.kind = kind})));
        expect(bool(editor.source_editor()->source() == U"before\r\nword\r\nafter"));
    }
};

"source_command_line_styles_preserve_CRLF_and_convert_existing_list_markers"_test = [] {
    struct Case { std::string source; Command command; std::u32string expected; };
    for (const auto& test : std::vector<Case>{
        {"alpha\r\nbeta", {.kind = CommandKind::SetHeading, .level = 2}, U"## alpha\r\n## beta"},
        {"## alpha\r\n# beta", {.kind = CommandKind::ClearHeading}, U"alpha\r\nbeta"},
        {"alpha\r\nbeta", {.kind = CommandKind::ToggleBlockQuote}, U"> alpha\r\n> beta"},
        {"> alpha\r\n> beta", {.kind = CommandKind::ToggleBlockQuote}, U"alpha\r\nbeta"},
        {"alpha\r\nbeta", {.kind = CommandKind::ToggleUnorderedList}, U"- alpha\r\n- beta"},
        {"- alpha\r\n- beta", {.kind = CommandKind::ToggleOrderedList}, U"1. alpha\r\n2. beta"},
        {"1. alpha\r\n2. beta", {.kind = CommandKind::ToggleTaskList}, U"- [ ] alpha\r\n- [ ] beta"},
        {"- [ ] alpha\r\n- [x] beta", {.kind = CommandKind::ToggleTaskCheckbox}, U"- [x] alpha\r\n- [x] beta"},
        {"- [ ] alpha\r\n- [x] beta", {.kind = CommandKind::ToggleTaskList}, U"alpha\r\nbeta"},
    }) {
        Editor editor(test.source);
        expect(fatal(editor.enter_source_mode()));
        editor.set_source_selection({editor.source_editor()->source().size(), 0});
        expect(bool(source_command_roundtrip(editor, test.command) == test.expected)) << test.source;
    }
};

"source_command_insertions_remain_editable_with_shared_history_in_both_views"_test = [] {
    for (const auto kind : {CommandKind::InsertImage, CommandKind::InsertLink, CommandKind::InsertCodeBlock,
        CommandKind::InsertMathBlock, CommandKind::InsertTable, CommandKind::InsertToc,
        CommandKind::InsertFootnote, CommandKind::ToggleCallout}) {
        Editor editor("");
        expect(fatal(editor.enter_source_mode()));
        Command command;
        command.kind = kind;
        command.path = U"folder/a b(c).png";
        command.alt = U"a]b";
        command.href = U"https://example.test/a b";
        command.rows = 1; command.cols = 2;
        command.callout_kind = U"NOTE";
        const auto source = source_command_roundtrip(editor, command);
        expect(!source.empty());
        expect(fatal(editor.execute_command(Command::InsertText(U"continued"))));
        expect(validate_document(editor.document()).empty());
    }
};

"source_command_code_and_math_wrap_selection_and_code_fence_is_safe"_test = [] {
    for (const auto kind : {CommandKind::InsertCodeBlock, CommandKind::InsertMathBlock}) {
        Editor editor("before\r\n\r\na```b\r\n\r\nafter");
        expect(fatal(editor.enter_source_mode()));
        editor.set_source_selection({10, 15});
        Command command; command.kind = kind;
        command.lang = U"cpp";
        const auto source = source_command_roundtrip(editor, command);
        expect(source.find(U"a```b") != std::u32string::npos);
        if (kind == CommandKind::InsertCodeBlock) {
            expect(source.find(U"````cpp\r\na```b\r\n````") != std::u32string::npos);
        }
    }
};

"source_command_callout_body_switches_kind_and_unwraps_without_nesting"_test = [] {
    Editor editor("> [!NOTE]\r\n> alpha\r\n> beta");
    expect(fatal(editor.enter_source_mode()));
    editor.set_source_selection(SourceSelection::caret(18));
    auto source = source_command_roundtrip(editor, {.kind = CommandKind::ToggleCallout, .callout_kind = U"TIP"});
    expect(bool(source == U"> [!TIP]\r\n> alpha\r\n> beta"));
    editor.set_source_selection(SourceSelection::caret(17));
    source = source_command_roundtrip(editor, {.kind = CommandKind::ToggleCallout, .callout_kind = U"TIP"});
    expect(bool(source == U"alpha\r\nbeta"));
};

"source_command_footnote_insertion_preserves_selected_text_and_uses_fresh_label"_test = [] {
    Editor editor("word\r\n\r\n[^1]: existing");
    expect(fatal(editor.enter_source_mode()));
    editor.set_source_selection({0, 4});
    auto source = source_command_roundtrip(editor, {.kind = CommandKind::InsertFootnote});
    expect(bool(source == U"word[^2]\r\n\r\n[^1]: existing\r\n\r\n[^2]: "));
};

"source_command_explicit_replacement_maps_new_selection_and_rejects_invalid_target"_test = [] {
    Editor editor("ab\r\ntail");
    expect(fatal(editor.enter_source_mode()));
    const auto line = editor.source_editor()->lines().front().id;
    editor.set_source_selection({0, 2});
    Command command;
    command.kind = CommandKind::ReplaceSourceText;
    command.text = U"longer";
    command.replacement = ReplaceTextTarget{{line, {0, 2}}, {{line, 1}, {line, 5}}};
    expect(fatal(editor.execute_command(command)));
    expect(bool(editor.source_editor()->source() == U"longer\r\ntail"));
    expect(editor.source_editor()->selection().anchor == 1_u);
    expect(editor.source_editor()->selection().active == 5_u);
    const auto after = editor.source_editor()->source();
    const auto selected = editor.source_editor()->selection();
    command.replacement->range.container_id = NodeId{999999};
    expect(!editor.execute_command(command));
    expect(bool(editor.source_editor()->source() == after));
    expect(editor.source_editor()->selection() == selected);
    expect(fatal(editor.undo()));
    expect(bool(editor.source_editor()->source() == U"ab\r\ntail"));
    expect(editor.source_editor()->selection() == SourceSelection{0, 2});
    expect(fatal(editor.redo()));
    expect(editor.source_editor()->selection() == selected);
};
};
