#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.core.document_edit;
import folia.core.command;
import folia.core.editor;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

#include "editing/support/container_editing.hpp"

suite editing_containers_block_insertion_tests = [] {

"container_gap_inserting_first_table_row_transfers_header_role"_test = [] {
    for (const auto edit : {DocumentTableEdit::InsertRowAbove, DocumentTableEdit::InsertRowAt}) {
        Editor editor("| H1 | H2 |\n| --- | --- |\n| A | B |");
        const auto& table = editor.document().root.children.front();
        const auto header_id = table.children.front().id;
        const auto selection = caret(table.children.front().children.front(), 1);
        const auto before = editor.markdown_utf8();
        editor.set_selection(selection);
        expect(fatal(editor.execute_document_table_edit(selection, edit).has_value()));
        const auto& changed = editor.document().root.children.front();
        expect(fatal(changed.children.size() == 3u));
        expect(changed.children.front().table_special().table_header_row);
        expect(!changed.children[1].table_special().table_header_row);
        expect(changed.children[1].id == header_id);
        expect(bool(changed.children[1].children.front().inline_content().source == U"H1"));
        Editor reloaded(editor.markdown_utf8());
        const auto& loaded = reloaded.document().root.children.front();
        expect(loaded.children.size() == changed.children.size());
        for (std::size_t index = 0; index < changed.children.size(); ++index) {
            expect(loaded.children[index].table_special().table_header_row
                == changed.children[index].table_special().table_header_row);
        }
        gap_history(editor, before, selection);
    }
};

"container_gap_block_insertion_replaces_selected_text_at_its_boundary"_test = [] {
    for (const auto kind : {CommandKind::InsertCodeBlock, CommandKind::InsertMathBlock,
             CommandKind::InsertTable, CommandKind::InsertToc}) {
        for (const auto reverse : {false, true}) {
            Editor editor("prefix abc suffix");
            const auto& owner = first_text(editor);
            auto selection = range(owner, 7, 10);
            if (reverse) std::swap(selection.anchor, selection.active);
            editor.set_selection(selection);
            const auto before = editor.markdown_utf8();
            Command command;
            command.kind = kind;
            command.rows = 1;
            command.cols = 2;
            reset_core_operation_counters();
            expect(fatal(editor.execute_document_insert_atomic_block(selection, command).has_value()));
            expect(read_core_operation_counters().full_document_parses == 0u);
            expect(read_core_operation_counters().full_document_serializations == 0u);
            expect(fatal(editor.document().root.children.size() == 3u));
            const auto& blocks = editor.document().root.children;
            expect(bool(blocks.front().inline_content().source == U"prefix "));
            expect(bool(blocks.back().inline_content().source == U" suffix"));
            if (kind == CommandKind::InsertCodeBlock || kind == CommandKind::InsertMathBlock) {
                expect(blocks[1].kind == (kind == CommandKind::InsertCodeBlock
                    ? BlockKind::CodeBlock : BlockKind::MathBlock));
                expect(bool(block_source_content(blocks[1].block_source) == U"abc"));
            } else {
                expect(blocks[1].kind == (kind == CommandKind::InsertTable ? BlockKind::Table : BlockKind::Toc));
            }
            gap_history(editor, before, selection);
        }
    }
};

"container_gap_code_conversion_preserves_selected_markdown_and_safe_fences"_test = [] {
    for (const auto source : {"# **alpha**", "prefix ``` suffix", "alpha\n\nbeta"}) {
        Editor editor(source);
        const auto& first = first_text(editor);
        auto selection = range(first, 0, first.inline_content().source.size());
        if (std::string_view(source).starts_with("prefix")) selection = range(first, 7, 10);
        else if (editor.document().root.children.size() > 1) {
            selection = {{first.id, 2, TextAffinity::Downstream},
                {editor.document().root.children.back().id, 3, TextAffinity::Upstream}};
        }
        editor.set_selection(selection);
        const auto selected = editor.selected_markdown_cps();
        const auto before = editor.markdown_utf8();
        Command command;
        command.kind = CommandKind::InsertCodeBlock;
        command.lang = U"cpp";
        expect(fatal(editor.execute_document_insert_atomic_block(selection, command).has_value()));
        const BlockNode* code = nullptr;
        walk_blocks(editor.document().root, [&](const BlockNode& block) {
            if (block.kind == BlockKind::CodeBlock) code = &block;
        });
        expect(fatal(code != nullptr));
        expect(bool(block_source_content(code->block_source) == selected));
        expect(code->block_source.tree().language == std::optional<std::string>{"cpp"});
        const auto after = editor.markdown_utf8();
        Editor reloaded(after);
        const BlockNode* reloaded_code = nullptr;
        walk_blocks(reloaded.document().root, [&](const BlockNode& block) {
            if (block.kind == BlockKind::CodeBlock) reloaded_code = &block;
        });
        expect(fatal(reloaded_code != nullptr));
        expect(bool(block_source_content(reloaded_code->block_source) == selected));
        expect(reloaded.markdown_utf8() == after);
        gap_history(editor, before, selection);
    }
};

"container_gap_block_insertion_from_table_cell_uses_flow_parent"_test = [] {
    for (const auto kind : {CommandKind::InsertCodeBlock, CommandKind::InsertMathBlock,
             CommandKind::InsertTable, CommandKind::InsertToc}) {
        Editor editor("| A | B |\n| --- | --- |\n| C | D |");
        const auto& table = editor.document().root.children.front();
        const auto table_id = table.id;
        const auto selection = caret(table.children.front().children.front(), 1);
        editor.set_selection(selection);
        const auto before = editor.markdown_utf8();
        Command command;
        command.kind = kind;
        command.rows = 1;
        command.cols = 2;
        expect(fatal(editor.execute_document_insert_atomic_block(selection, command).has_value()));
        expect(fatal(editor.document().root.children.size() == 2u));
        const auto* retained = find_block(editor.document().root, table_id);
        expect(fatal(retained != nullptr));
        expect(retained->children.size() == 2u);
        for (const auto& row : retained->children) {
            expect(row.kind == BlockKind::TableRow);
            expect(row.children.size() == 2u);
            for (const auto& cell : row.children) expect(cell.kind == BlockKind::TableCell);
        }
        expect(bool(retained->children.front().children.front().inline_content().source == U"A"));
        gap_history(editor, before, selection);
    }
};

"container_gap_code_insertion_from_cross_cell_selection_keeps_rectangular_table"_test = [] {
    Editor editor("| A | B |\n| --- | --- |\n| C | D |");
    const auto& table = editor.document().root.children.front();
    const auto table_id = table.id;
    const auto selection = TextSelection{
        {table.children.front().children.front().id, 0, TextAffinity::Downstream},
        {table.children.front().children.back().id, 1, TextAffinity::Upstream}};
    editor.set_selection(selection);
    const auto before = editor.markdown_utf8();
    Command command;
    command.kind = CommandKind::InsertCodeBlock;
    expect(fatal(editor.execute_document_insert_atomic_block(selection, command).has_value()));
    expect(fatal(editor.document().root.children.size() == 2u));
    const auto& code = editor.document().root.children.back();
    expect(code.kind == BlockKind::CodeBlock);
    expect(bool(block_source_content(code.block_source) == U"A\tB"));
    const auto* retained = find_block(editor.document().root, table_id);
    expect(fatal(retained != nullptr));
    expect(retained->children.size() == 2u);
    for (const auto& row : retained->children) expect(row.children.size() == 2u);
    expect(retained->children.front().children.front().inline_content().source.empty());
    expect(retained->children.front().children.back().inline_content().source.empty());
    gap_history(editor, before, selection);
};

};
