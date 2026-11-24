#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.core.input;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.ids;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;
#include "support/editor_test_support.hpp"

namespace {
void complete_invariants(const Editor& editor) {
    expect(validate_document(editor.document()).empty());
    expect(document_indexes_are_exact(editor.document()));
    walk_blocks(editor.document().root, [&](const BlockNode& block) {
        if (const auto* source = editable_inline_document(block))
            expect(bool(flatten_tokens(source->tree, source->source) == source->source));
    });
}
void complete_local_work() {
    const auto counts = read_core_operation_counters();
    expect(counts.full_document_parses == 0_u);
    expect(counts.full_document_serializations == 0_u);
    expect(counts.full_tree_transaction_diffs == 0_u);
}
void complete_history(Editor& editor, const std::string& before, const TextSelection& selected) {
    const auto after = editor.markdown_utf8();
    const auto after_selection = editor.selection();
    complete_invariants(editor);
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == before);
    expect(editor.selection() == selected);
    complete_invariants(editor);
    expect(fatal(editor.redo()));
    expect(editor.markdown_utf8() == after);
    expect(editor.selection() == after_selection);
    complete_invariants(editor);
}
}

suite editor_editing_completeness = [] {

"editing_gap_typing_at_atomic_boundaries_creates_one_reversible_paragraph"_test = [] {
    for (const auto initial : {"![alt](image.png)", "***", "[TOC]", "---\nx: y\n---\n"})
    for (const auto offset : {0u, 1u}) {
        Editor editor(initial);
        const auto id = editor.document().root.children.front().id;
        const auto selected = TextSelection::caret({id, offset});
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command(Command::InsertText(U"继续"))));
        complete_local_work();
        const auto counters = read_core_operation_counters();
        expect(counters.full_document_block_index_scans == 0_u);
        expect(fatal(editor.document().root.children.size() == 2_u));
        const auto& paragraph = editor.document().root.children[offset == 0 ? 0 : 1];
        expect(paragraph.kind == BlockKind::Paragraph);
        expect(bool(paragraph.inline_content().source == U"继续"));
        expect(editor.selection().active.container_id == paragraph.id);
        complete_history(editor, before, selected);
    }
};

"editing_gap_atomic_deletion_respects_direction_and_selected_objects"_test = [] {
    for (const auto object : {"![alt](image.png)", "***", "[TOC]"}) {
        for (const auto backward : {false, true}) {
            Editor editor(std::string("left\n\n") + object + "\n\nright");
            const auto id = editor.document().root.children[1].id;
            const auto selected = TextSelection::caret({id, backward ? 0u : 1u});
            editor.set_selection(selected);
            const auto before = editor.markdown_utf8();
            const auto revision = editor.revision();
            expect(fatal(editor.execute_command({.kind = backward ? CommandKind::DeleteBackward : CommandKind::DeleteForward})));
            expect(editor.markdown_utf8() == before);
            expect(editor.revision() == revision);
            expect(!editor.has_undo());
            expect(editor.selection().active.container_id == editor.document().root.children[backward ? 0 : 2].id);
            complete_invariants(editor);
        }
        for (const auto replacement : {false, true}) {
            Editor editor(object);
            const auto id = editor.document().root.children.front().id;
            const TextSelection selected{{id, 1}, {id, 0}};
            editor.set_selection(selected);
            const auto before = editor.markdown_utf8();
            const auto command = replacement ? Command::InsertText(U"replacement") : Command{.kind = CommandKind::DeleteSelection};
            expect(fatal(editor.execute_command(command)));
            expect(editor.markdown_utf8() == (replacement ? "replacement" : ""));
            complete_history(editor, before, selected);
        }
    }
};

"editing_gap_select_all_with_atomic_document_edges_can_delete_and_replace"_test = [] {
    for (const auto initial : {"![alt](image.png)\n\ntext", "text\n\n***", "![alt](image.png)\n\ntext\n\n[TOC]"})
    for (const auto paste : {false, true}) {
        Editor editor(initial);
        expect(fatal(editor.execute_command({.kind = CommandKind::SelectAll})));
        const auto selected = editor.selection();
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = paste ? CommandKind::Paste : CommandKind::DeleteSelection, .text = paste ? U"new" : U""})));
        complete_local_work();
        expect(editor.markdown_utf8() == (paste ? "new" : ""));
        complete_history(editor, before, selected);
    }
};

"editing_gap_shift_enter_in_table_cell_preserves_table_and_break_deletion"_test = [] {
    Editor editor("| ab | next |\n| --- | --- |\n| value | end |");
    const auto id = first_text(editor).id;
    const auto selected = TextSelection::caret({id, 1});
    editor.set_selection(selected);
    const auto before = editor.markdown_utf8();
    expect(fatal(editor.execute_command({.kind = CommandKind::InsertSoftBreak})));
    expect(bool(find_document_block(editor.document(), id)->inline_content().source == U"a<br>b"));
    complete_history(editor, before, selected);
    expect(fatal(editor.execute_command({.kind = CommandKind::DeleteBackward})));
    expect(bool(find_document_block(editor.document(), id)->inline_content().source == U"ab"));
    complete_invariants(editor);
};

"editing_gap_typed_table_pipes_and_newlines_cannot_create_extra_columns"_test = [] {
    for (const auto text : {U"x|y", U"x\\|y", U"`x|y`", U" a\r\nb "}) {
        Editor editor("| a | b |\n| --- | --- |\n| c | d |");
        const auto id = first_text(editor).id;
        const TextSelection selected{{id, 0}, {id, 1}};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_command(Command::InsertText(text))));
        const auto saved = editor.markdown_utf8();
        Editor reloaded(saved);
        const auto& table = reloaded.document().root.children.front();
        expect(fatal(table.kind == BlockKind::Table));
        expect(table.children.size() == 2_u);
        for (const auto& row : table.children) expect(row.children.size() == 2_u);
        complete_history(editor, before, selected);
    }
};

"editing_gap_table_insertions_preserve_existing_pipe_escape_parity"_test = [] {
    for (const auto paste : {false, true}) {
        Editor editor("| h |\n| --- |\n| \\| |");
        const auto id = editor.document().root.children[0].children[1].children[0].id;
        const auto selected = TextSelection::caret({id, 0});
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_command({.kind = paste ? CommandKind::Paste : CommandKind::InsertText, .text = U"\\"})));
        const auto* cell = find_document_block(editor.document(), id);
        expect(bool(inline_visible_text(cell->inline_content()) == U"\\|"));
        const auto saved = editor.markdown_utf8();
        Editor reloaded(saved);
        const auto& row = reloaded.document().root.children[0].children[1];
        expect(fatal(row.children.size() == 1_u));
        expect(bool(inline_visible_text(row.children[0].inline_content()) == U"\\|"));
        complete_history(editor, before, selected);
    }
};

"editing_gap_image_and_link_commands_escape_generated_syntax_and_keep_caret_reachable"_test = [] {
    for (const auto image : {false, true}) {
        Editor editor("label] text");
        const auto id = first_text(editor).id;
        const TextSelection selected{{id, 0}, {id, 11}};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        Command command;
        command.kind = image ? CommandKind::InsertImage : CommandKind::InsertLink;
        command.href = command.path = U"folder/a b(c).png";
        command.alt = U"label] text";
        command.title = U"a \"quoted\" title";
        expect(fatal(editor.execute_command(command)));
        const auto& source = find_document_block(editor.document(), id)->inline_content();
        const auto wanted = image ? InlineCstKind::Image : InlineCstKind::Link;
        const auto node = std::ranges::find(source.tree.nodes, wanted, &InlineCstNode::kind);
        expect(fatal(node != source.tree.nodes.end()));
        expect(node->semantics().href == "folder/a%20b(c).png");
        if (!image) expect(node->semantics().title == std::optional<std::string>{"a \"quoted\" title"});
        if (image) expect(editor.selection().active.source_offset == source.source.size());
        complete_history(editor, before, selected);
        if (image) {
            expect(fatal(editor.execute_command({.kind = CommandKind::InsertNewline})));
            expect(fatal(editor.execute_command(Command::InsertText(U"after"))));
            expect(editor.markdown_utf8().find("label\\] text") != std::string::npos);
            complete_invariants(editor);
        }
    }
};

"editing_gap_inline_code_chooses_delimiters_around_existing_backticks"_test = [] {
    for (const auto content : {U"a`b", U"`a", U"a`", U"a``b", U" padded "}) {
        Editor editor("");
        expect(fatal(editor.execute_command(Command::InsertText(content))));
        const auto id = first_text(editor).id;
        const auto length = std::u32string_view(content).size();
        const TextSelection selected{{id, 0}, {id, length}};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_command({.kind = CommandKind::ToggleInlineCode})));
        const auto& source = find_document_block(editor.document(), id)->inline_content();
        expect(fatal(source.tree.nodes.size() == 1_u));
        expect(source.tree.nodes.front().kind == InlineCstKind::CodeSpan);
        expect(bool(source.source.substr(editor.selection().anchor.source_offset,
            editor.selection().active.source_offset - editor.selection().anchor.source_offset) == content));
        complete_history(editor, before, selected);
    }
};

"editing_gap_link_and_image_commands_keep_html_text_semantics"_test = [] {
    for (const auto image : {false, true}) {
        Editor editor("<p>label</p>");
        const auto id = first_text(editor).id;
        const TextSelection selected{{id, 0}, {id, 5}};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        Command command;
        command.kind = image ? CommandKind::InsertImage : CommandKind::InsertLink;
        command.href = command.path = U"folder/a&b.png";
        command.alt = U"a\"b";
        command.title = U"title\"quoted";
        expect(fatal(editor.execute_command(command)));
        const auto& source = find_document_block(editor.document(), id)->inline_content();
        const auto node = std::ranges::find_if(source.tree.nodes, [image](const InlineCstNode& value) {
            return value.kind == InlineCstKind::HtmlElement
                && value.semantics().html_tag == (image ? "img" : "a");
        });
        expect(fatal(node != source.tree.nodes.end()));
        expect(node->semantics().href == "folder/a&b.png");
        expect(source.syntax_mode == InlineSyntaxMode::HtmlText);
        complete_history(editor, before, selected);
    }
};

"editing_gap_generated_inline_commands_work_at_atomic_boundaries_and_selections"_test = [] {
    for (const auto initial : {"![old](old.png)", "***", "[TOC]"})
    for (const auto image : {false, true})
    for (const auto position : {0u, 1u, 2u}) {
        Editor editor(initial);
        const auto id = editor.document().root.children.front().id;
        const auto selected = position == 2 ? TextSelection{{id, 1}, {id, 0}}
            : TextSelection::caret({id, position});
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        Command command;
        command.kind = image ? CommandKind::InsertImage : CommandKind::InsertLink;
        command.href = command.path = U"new.png";
        reset_core_operation_counters();
        expect(fatal(editor.execute_command(command)));
        complete_local_work();
        expect(editor.document().root.children.size() == (position == 2 ? 1u : 2u));
        expect(editor.markdown_utf8().find("new.png") != std::string::npos);
        complete_history(editor, before, selected);
        expect(fatal(editor.execute_command(Command::InsertText(U"next"))));
        complete_invariants(editor);
    }
};

"editing_gap_core_keyboard_modifiers_and_clipboard_payload_are_preserved"_test = [] {
    Editor editor("text");
    EditorInputEvent event;
    event.kind = EditorInputEvent::Kind::KeyDown;
    event.key.key_code = KeyCode::Enter;
    event.key.modifiers.shift = true;
    expect(editor.handle_input(event)->kind == CommandKind::InsertSoftBreak);
    event.key.key_code = KeyCode::Char; event.key.ch = U'z'; event.key.modifiers.ctrl = true;
    expect(editor.handle_input(event)->kind == CommandKind::Redo);
    event.key.key_code = KeyCode::Home;
    expect(editor.handle_input(event)->kind == CommandKind::MoveDocumentStart);
    expect(editor.handle_input(event)->extend_selection);
    event.key.key_code = KeyCode::End;
    expect(editor.handle_input(event)->kind == CommandKind::MoveDocumentEnd);
    event.kind = EditorInputEvent::Kind::ClipboardPaste;
    event.paste_payload = U"![alt](asset.png)";
    auto paste = editor.handle_input(event);
    expect(fatal(paste.has_value()));
    expect(paste->kind == CommandKind::Paste);
    expect(bool(paste->text == event.paste_payload));
};
};
