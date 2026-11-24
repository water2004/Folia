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

suite editing_containers_heading_conversion_tests = [] {

"container_gap_heading_applies_to_selected_paragraphs_in_both_directions"_test = [] {
    for (const auto reverse : {false, true}) {
        Editor editor("one\n\ntwo\n\ntail");
        const auto* one = gap_text(editor, U"one");
        const auto* tail = gap_text(editor, U"tail");
        expect(fatal(one != nullptr && tail != nullptr));
        auto selection = TextSelection{{one->id, 1, TextAffinity::Downstream},
            {tail->id, 0, TextAffinity::Downstream}};
        if (reverse) std::swap(selection.anchor, selection.active);
        editor.set_selection(selection);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_document_set_heading(selection, 2).has_value()));
        const auto counts = read_core_operation_counters();
        expect(counts.full_document_parses == 0u);
        expect(counts.full_document_serializations == 0u);
        expect(counts.full_document_block_index_scans == 0u);
        expect(editor.selection() == selection);
        const auto& blocks = editor.document().root.children;
        expect(fatal(blocks.size() == 3u));
        expect(blocks[0].kind == BlockKind::Heading);
        expect(blocks[1].kind == BlockKind::Heading);
        expect(blocks[2].kind == BlockKind::Paragraph);
        expect(blocks[0].text_special().level == 2u);
        expect(blocks[1].text_special().level == 2u);
        expect(editor.markdown_utf8() == "## one\n\n## two\n\ntail");
        gap_history(editor, before, selection);
    }
};

"container_gap_multiline_heading_preserves_container_lines_and_selection"_test = [] {
    for (const auto ending : {std::string("\n"), std::string("\r\n"), std::string("\r")}) {
        for (const auto pattern : {
                 std::string("one\ntwo\nthree"),
                 std::string("> one\n> two\n> three"),
                 std::string("> one\ntwo\nthree"),
                 std::string("- one\n  two\n  three"),
                 std::string("- one\ntwo\nthree"),
                 std::string("- [x] one\n  two\n  three"),
                 std::string("[^note]: one\n    two\n    three"),
                 std::string("> [!NOTE]\n> one\n> two\n> three")}) {
            std::string markdown;
            for (const auto value : pattern) markdown += value == '\n' ? ending : std::string(1, value);
            Editor editor(markdown);
            const BlockNode* original = nullptr;
            walk_blocks(editor.document().root, [&](const BlockNode& block) {
                if (block.kind == BlockKind::Paragraph
                    && block.inline_content.source.starts_with(U"one")) original = &block;
            });
            expect(fatal(original != nullptr)) << markdown;
            const auto original_id = original->id;
            const auto last_start = original->inline_content.source.rfind(U"three");
            expect(fatal(last_start != std::u32string::npos));
            const auto old_path = document_block_path(editor.document(), original_id);
            expect(fatal(old_path.has_value()));
            const auto selection = TextSelection{{original_id, 1, TextAffinity::Downstream},
                {original_id, last_start + 2, TextAffinity::Upstream}};
            editor.set_selection(selection);
            const auto before = editor.markdown_utf8();
            reset_core_operation_counters();
            expect(fatal(editor.execute_document_set_heading(selection, 3).has_value())) << markdown;
            const auto counts = read_core_operation_counters();
            expect(counts.full_document_parses == 0u);
            expect(counts.full_document_serializations == 0u);
            expect(counts.full_document_block_index_scans == 0u);
            std::vector<const BlockNode*> headings;
            walk_blocks(editor.document().root, [&](const BlockNode& block) {
                if (block.kind == BlockKind::Heading) headings.push_back(&block);
            });
            expect(fatal(headings.size() == 3u)) << editor.markdown_utf8();
            expect(headings[0]->id == original_id);
            expect(bool(headings[0]->inline_content.source == U"one"));
            expect(bool(headings[1]->inline_content.source == U"two"));
            expect(bool(headings[2]->inline_content.source == U"three"));
            expect(editor.selection().anchor == selection.anchor);
            expect(editor.selection().active.container_id == headings[2]->id);
            expect(editor.selection().active.source_offset == 2u);
            expect(editor.selection().active.affinity == TextAffinity::Upstream);
            Editor reloaded(editor.markdown_utf8());
            std::size_t reloaded_count = 0;
            walk_blocks(reloaded.document().root, [&](const BlockNode& block) {
                if (block.kind != BlockKind::Heading) return;
                ++reloaded_count;
                const auto path = document_block_path(reloaded.document(), block.id);
                expect(path.has_value() && path->size() == old_path->size()) << editor.markdown_utf8();
            });
            expect(reloaded_count == 3u) << editor.markdown_utf8();
            expect(reloaded.markdown_utf8() == editor.markdown_utf8());
            gap_history(editor, before, selection);
        }
    }
};

"container_gap_heading_handles_hardbreaks_and_empty_physical_lines"_test = [] {
    {
        Editor editor("one  \r\ntwo\\\nthree");
        const auto& block = first_text(editor);
        const auto selection = caret(block, block.inline_content.source.size());
        editor.set_selection(selection);
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_document_set_heading(selection, 2).has_value()));
        expect(editor.markdown_utf8() == "## one  \r\n## two\\\n## three") << editor.markdown_utf8();
        expect(editor.selection().active.source_offset == 5u);
        Editor reloaded(editor.markdown_utf8());
        expect(reloaded.document().root.children.size() == 3u);
        expect(reloaded.markdown_utf8() == editor.markdown_utf8());
        gap_history(editor, before, selection);
    }
    {
        Editor editor("one");
        expect(fatal(editor.execute_document_insert_soft_break(caret(first_text(editor), 3)).has_value()));
        const auto selection = editor.selection();
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_document_set_heading(selection, 1).has_value()));
        expect(fatal(editor.document().root.children.size() == 2u));
        const auto& empty = editor.document().root.children.back();
        expect(empty.kind == BlockKind::Heading);
        expect(empty.inline_content.source.empty());
        expect(editor.selection().active.container_id == empty.id);
        expect(editor.selection().active.source_offset == 0u);
        gap_history(editor, before, selection);
    }
    {
        Editor editor("");
        const auto selection = editor.selection();
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_document_set_heading(selection, 6).has_value()));
        expect(editor.document().root.children.front().kind == BlockKind::Heading);
        expect(editor.document().root.children.front().inline_content.source.empty());
        gap_history(editor, before, selection);
    }
};

"container_gap_heading_to_paragraph_preserves_sibling_boundaries"_test = [] {
    for (const auto ending : {std::string("\n"), std::string("\r\n"), std::string("\r")}) {
        for (const auto pattern : {"# one\n## two", "> # one\n> ## two", "- # one\n  ## two"}) {
            std::string markdown;
            for (const auto value : std::string_view(pattern)) markdown += value == '\n' ? ending : std::string(1, value);
            Editor editor(markdown);
            std::vector<const BlockNode*> headings;
            walk_blocks(editor.document().root, [&](const BlockNode& block) {
                if (block.kind == BlockKind::Heading) headings.push_back(&block);
            });
            expect(fatal(headings.size() == 2u));
            const auto selection = TextSelection{{headings.front()->id, 0, TextAffinity::Downstream},
                {headings.back()->id, 3, TextAffinity::Upstream}};
            editor.set_selection(selection);
            const auto before = editor.markdown_utf8();
            expect(fatal(editor.execute_document_set_heading(selection, 0).has_value()));
            expect(bool(gap_text_order(editor.document()) == std::vector<std::u32string>{U"one", U"two"}));
            Editor reloaded(editor.markdown_utf8());
            expect(bool(gap_text_order(reloaded.document()) == std::vector<std::u32string>{U"one", U"two"}))
                << editor.markdown_utf8();
            expect(reloaded.markdown_utf8() == editor.markdown_utf8());
            if (std::string_view(pattern).front() == '#') {
                expect(editor.markdown_utf8() == "one" + ending + ending + "two") << editor.markdown_utf8();
            }
            gap_history(editor, before, selection);
        }
    }
};

"container_gap_heading_preserves_callout_title_and_html_multiline_roles"_test = [] {
    {
        Editor editor("> [!NOTE] Title\n> body");
        const auto& callout = editor.document().root.children.front();
        expect(fatal(callout.kind == BlockKind::Callout));
        expect(fatal(callout.children.front().kind == BlockKind::CalloutTitle));
        const auto title_id = callout.children.front().id;
        const auto title_selection = caret(callout.children.front(), 2);
        editor.set_selection(title_selection);
        const auto before = editor.markdown_utf8();
        expect(!editor.execute_document_set_heading(title_selection, 2).has_value());
        expect(editor.markdown_utf8() == before);
        expect(!editor.has_undo());
        const auto* body = gap_text(editor, U"body");
        expect(fatal(body != nullptr));
        const auto selection = TextSelection{{title_id, 1, TextAffinity::Downstream},
            {body->id, 4, TextAffinity::Upstream}};
        editor.set_selection(selection);
        expect(fatal(editor.execute_document_set_heading(selection, 2).has_value()));
        const auto& after = editor.document().root.children.front();
        expect(after.children.front().kind == BlockKind::CalloutTitle);
        expect(bool(after.children.front().inline_content.source == U"Title"));
        expect(after.children.back().kind == BlockKind::Heading);
        gap_history(editor, before, selection);
    }
    {
        Editor editor("<div><p>one\ntwo</p><p>tail</p></div>");
        const auto* text = gap_text(editor, U"one\ntwo");
        expect(fatal(text != nullptr));
        const auto selection = caret(*text, 5);
        editor.set_selection(selection);
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_document_set_heading(selection, 2).has_value()));
        const auto* heading = find_document_block(editor.document(), selection.active.container_id);
        expect(fatal(heading != nullptr));
        expect(heading->kind == BlockKind::Heading);
        expect(bool(heading->inline_content.source == U"one\ntwo"));
        Editor reloaded(editor.markdown_utf8());
        std::size_t headings = 0;
        walk_blocks(reloaded.document().root, [&](const BlockNode& block) {
            if (block.kind == BlockKind::Heading) {
                ++headings;
                expect(bool(block.inline_content.source == U"one\ntwo"));
            }
        });
        expect(headings == 1u) << editor.markdown_utf8();
        gap_history(editor, before, selection);
    }
};

"container_gap_heading_rejects_invalid_offsets_and_table_cell_targets"_test = [] {
    Editor editor("one\ntwo");
    auto document = editor.document();
    const auto id = document.root.children.front().id;
    for (const auto offset : {std::size_t{8}, (std::numeric_limits<std::size_t>::max)()}) {
        const auto invalid = TextSelection::caret({id, offset, TextAffinity::Downstream});
        expect(!document_set_heading(document, invalid, 2).has_value());
        expect(document.root.children.size() == 1u);
        expect(document.root.children.front().kind == BlockKind::Paragraph);
        expect(bool(document.root.children.front().inline_content.source == U"one\ntwo"));
    }
    Editor table("| A | B |\n|---|---|\n| one | two |");
    const auto& cell = table.document().root.children.front().children.front().children.front();
    const auto selection = caret(cell, 0);
    const auto before = table.markdown_utf8();
    expect(!table.execute_document_set_heading(selection, 2).has_value());
    expect(table.markdown_utf8() == before);
    expect(!table.has_undo());
};

"container_gap_heading_level_change_preserves_existing_export_anchor"_test = [] {
    Editor editor("## existing title");
    const auto& heading = first_text(editor);
    const auto slug = heading.text_special().slug;
    expect(fatal(!slug.empty()));
    const auto selection = caret(heading, 3);
    editor.set_selection(selection);
    const auto before = editor.markdown_utf8();
    expect(fatal(editor.execute_document_set_heading(selection, 4).has_value()));
    const auto& after = editor.document().root.children.front();
    expect(after.text_special().level == 4u);
    expect(after.text_special().slug == slug);
    expect(bool(after.inline_content.source == U"existing title"));
    gap_history(editor, before, selection);
};

};
