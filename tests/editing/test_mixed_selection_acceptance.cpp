#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

namespace {

void mixed_selection_history(Editor& editor, const std::string& before, TextSelection selection) {
    const auto after = editor.markdown_utf8();
    const auto after_selection = editor.selection();
    expect(validate_document(editor.document()).empty());
    expect(document_indexes_are_exact(editor.document()));
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == before);
    expect(editor.selection() == selection);
    expect(document_indexes_are_exact(editor.document()));
    expect(fatal(editor.redo()));
    expect(editor.markdown_utf8() == after);
    expect(editor.selection() == after_selection);
    expect(document_indexes_are_exact(editor.document()));
}

suite mixed_selection_acceptance_tests = [] {

"mixed_selection_empty_cell_boundaries_collapse_and_accept_replacement"_test = [] {
    for (const auto reversed : {false, true})
    for (const auto kind : {CommandKind::DeleteSelection, CommandKind::InsertText,
            CommandKind::Paste, CommandKind::InsertNewline, CommandKind::InsertSoftBreak}) {
        Editor editor("| aa | bb |\n| --- | --- |\n| cc | dd |");
        const auto first = editor.document().cached_editable_order[0];
        const auto second = editor.document().cached_editable_order[1];
        const TextPosition from{first, 2, TextAffinity::Downstream};
        const TextPosition to{second, 0, TextAffinity::Upstream};
        const auto selection = reversed ? TextSelection{to, from} : TextSelection{from, to};
        editor.set_selection(selection);
        const auto before = editor.markdown_utf8();
        const auto revision = editor.revision();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = kind, .text = U"X"}))) << static_cast<int>(kind);
        const auto counts = read_core_operation_counters();
        expect(counts.full_document_parses == 0_u);
        expect(counts.full_document_serializations == 0_u);
        expect(counts.full_tree_transaction_diffs == 0_u);
        expect(editor.selection().is_caret());
        expect(editor.selection().active.container_id == first);
        const auto* cell = find_document_block(editor.document(), first);
        const auto expected = kind == CommandKind::DeleteSelection ? U"aa"
            : kind == CommandKind::InsertText || kind == CommandKind::Paste ? U"aaX" : U"aa<br>";
        expect(bool(cell->inline_content().source == expected));
        expect(bool(find_document_block(editor.document(), second)->inline_content().source == U"bb"));
        if (kind == CommandKind::DeleteSelection) {
            expect(editor.revision() == revision);
            expect(editor.markdown_utf8() == before);
            expect(!editor.has_undo());
        } else {
            mixed_selection_history(editor, before, selection);
        }
    }
};

"mixed_selection_tables_do_not_block_atomic_and_raw_owner_deletion"_test = [] {
    const std::string table = "| aa | bb |\n| --- | --- |\n| cc | dd |";
    const std::vector<std::string> sources{
        "before\n\n" + table + "\n\n```cpp\nvalue\n```\n\n![alt](asset.png)\n\nafter",
        "![alt](asset.png)\n\n```cpp\nvalue\n```\n\n" + table + "\n\n***",
        "```cpp\nvalue\n```\n\n" + table + "\n\n$$\nx + y\n$$",
        "[TOC]\n\n" + table + "\n\n> ![alt](asset.png)",
        table + "\n\n```cpp\n```\n\n![alt](asset.png)",
    };
    for (const auto& source : sources)
    for (const auto reversed : {false, true})
    for (const auto kind : {CommandKind::DeleteSelection, CommandKind::InsertText,
            CommandKind::Paste, CommandKind::InsertNewline}) {
        Editor editor(source);
        expect(fatal(editor.execute_command({.kind = CommandKind::SelectAll})));
        auto selection = editor.selection();
        if (reversed) std::swap(selection.anchor, selection.active);
        editor.set_selection(selection);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = kind, .text = U"X"}))) << source << static_cast<int>(kind);
        const auto counts = read_core_operation_counters();
        expect(counts.full_document_parses == 0_u);
        expect(counts.full_document_serializations == 0_u);
        expect(counts.full_tree_transaction_diffs == 0_u);
        std::size_t cells = 0;
        walk_blocks(editor.document().root, [&](const BlockNode& block) {
            if (block.kind == BlockKind::TableCell) ++cells;
            expect(block.kind != BlockKind::CodeBlock && block.kind != BlockKind::MathBlock
                && block.kind != BlockKind::ImageBlock && block.kind != BlockKind::ThematicBreak
                && block.kind != BlockKind::Toc);
        });
        expect(cells == 4_u);
        mixed_selection_history(editor, before, selection);
    }
};

"mixed_selection_partial_raw_boundaries_preserve_fences_across_tables"_test = [] {
    for (const auto reversed : {false, true}) {
        Editor editor("```cpp\nprefix\n```\n\n| aa | bb |\n| --- | --- |\n| cc | dd |\n\n```cpp\nsuffix\n```");
        const auto order = editor.document().cached_editable_order;
        const auto* first = find_document_block(editor.document(), order.front());
        const auto* last = find_document_block(editor.document(), order.back());
        const auto first_id = first->id;
        const auto last_id = last->id;
        const TextPosition from{first_id, block_source_editable_range(first->block_source).start + 3};
        const TextPosition to{last_id, block_source_editable_range(last->block_source).start + 3};
        const auto selection = reversed ? TextSelection{to, from} : TextSelection{from, to};
        editor.set_selection(selection);
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_command({.kind = CommandKind::DeleteSelection})));
        first = find_document_block(editor.document(), first_id);
        last = find_document_block(editor.document(), last_id);
        expect(fatal(first != nullptr && last != nullptr));
        expect(bool(first->block_source.source() == U"```cpp\npre\n```"));
        expect(bool(last->block_source.source() == U"```cpp\nfix\n```"));
        expect(editor.selection().active.container_id == first_id);
        expect(editor.selection().active.source_offset == from.source_offset);
        mixed_selection_history(editor, before, selection);
    }
};

"mixed_selection_protected_empty_boundaries_still_allow_typing"_test = [] {
    for (const auto source : {"```cpp\nvalue\n```\n\nafter", "***\n\nafter"}) {
        Editor editor(source);
        const auto order = editor.document().cached_editable_order;
        const auto* first = find_document_block(editor.document(), order.front());
        const auto first_range = *editable_block_source_range(*first);
        const TextSelection selection{{first->id, first_range.end}, {order.back(), 0}};
        editor.set_selection(selection);
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_command(Command::InsertText(U"X")))) << source;
        expect(editor.selection().is_caret());
        expect(editor.markdown_utf8().find('X') != std::string::npos);
        mixed_selection_history(editor, before, selection);
    }
};

};

} // namespace
