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

suite editing_containers_atomic_boundaries_tests = [] {

"container_gap_delete_atomic_neighbor_from_nonempty_text"_test = [] {
    for (const auto atomic : {"![alt](image.png)", "***", "[TOC]"}) {
        for (const auto backward : {false, true}) {
            for (const auto quoted : {false, true}) {
                auto markdown = backward ? std::string(atomic) + "\n\ntext"
                    : std::string("text\n\n") + atomic;
                if (quoted) markdown = gap_quote(markdown);
                Editor editor(markdown);
                const auto* text = gap_text(editor, U"text");
                expect(fatal(text != nullptr)) << markdown;
                const auto id = text->id;
                const auto selection = caret(*text, backward ? 0 : 4);
                editor.set_selection(selection);
                const auto before = editor.markdown_utf8();
                reset_core_operation_counters();
                auto transaction = backward
                    ? editor.execute_document_delete_backward(selection)
                    : editor.execute_document_delete_forward(selection);
                expect(fatal(transaction.has_value())) << markdown;
                expect(read_core_operation_counters().full_document_parses == 0u);
                expect(read_core_operation_counters().full_document_serializations == 0u);
                const auto* remaining = find_block(editor.document().root, id);
                expect(fatal(remaining != nullptr));
                expect(bool(remaining->inline_content().source == U"text"));
                expect(editor.selection().active.container_id == id);
                expect(editor.selection().active.source_offset == (backward ? 0u : 4u));
                std::size_t atomics = 0;
                walk_blocks(editor.document().root, [&](const BlockNode& block) {
                    if (block.kind == BlockKind::ImageBlock || block.kind == BlockKind::ThematicBreak
                        || block.kind == BlockKind::Toc) ++atomics;
                });
                expect(atomics == 0u);
                gap_history(editor, before, selection);
            }
        }
    }
};

"container_gap_empty_terminal_owner_respects_selection_affinity"_test = [] {
    for (const auto upstream : {false, true}) {
        Editor editor("one");
        expect(fatal(editor.execute_document_enter(caret(first_text(editor), 3)).has_value()));
        const auto& blocks = editor.document().root.children;
        expect(fatal(blocks.size() == 2u));
        const auto selection = TextSelection{{blocks.front().id, 0, TextAffinity::Downstream},
            {blocks.back().id, 0, upstream ? TextAffinity::Upstream : TextAffinity::Downstream}};
        editor.set_selection(selection);
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_document_set_heading(selection, 1).has_value()));
        expect(editor.document().root.children.front().kind == BlockKind::Heading);
        expect(editor.document().root.children.back().kind == (upstream ? BlockKind::Heading : BlockKind::Paragraph));
        gap_history(editor, before, selection);
    }
};

"container_gap_callout_unwrap_preserves_incoming_and_title_body_boundaries"_test = [] {
    for (const auto ending : {std::string{"\n"}, std::string{"\r\n"}, std::string{"\r"}})
    for (const auto titled : {false, true})
    for (const auto preceding : {false, true})
    for (const auto following : {false, true})
    for (const auto interrupted : {false, true}) {
        if (interrupted && !preceding) continue;
        auto original = preceding ? std::string{"before"} + ending + (interrupted ? "" : ending) : "";
        original += std::string{"> [!NOTE]"} + (titled ? " title" : "") + ending + "> body";
        if (following) original += ending + ending + "after";
        auto expected = preceding ? std::string{"before"} + ending + ending : "";
        if (titled) expected += "title" + ending + ending;
        expected += "body";
        if (following) expected += ending + ending + "after";

        Editor editor(original);
        expect(fatal(editor.markdown_utf8() == original));
        const auto* body = gap_text(editor, U"body");
        expect(fatal(body != nullptr));
        const auto selected = caret(*body, 2);
        editor.set_selection(selected);
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::ToggleCallout, .callout_kind = U"NOTE"})));
        const auto counters = read_core_operation_counters();
        expect(counters.full_document_parses == 0u);
        expect(counters.full_document_serializations == 0u);
        expect(counters.full_tree_transaction_diffs == 0u);
        expect(editor.markdown_utf8() == expected) << std::quoted(original)
            << " => " << std::quoted(editor.markdown_utf8());
        expect(editor.selection() == selected);
        expect(editor.document().root.children.size() == static_cast<std::size_t>(1 + titled + preceding + following));
        std::vector<std::u32string> expected_order;
        if (preceding) expected_order.push_back(U"before");
        if (titled) expected_order.push_back(U"title");
        expected_order.push_back(U"body");
        if (following) expected_order.push_back(U"after");
        expect(bool(gap_text_order(editor.document()) == expected_order));
        Editor reloaded(editor.markdown_utf8());
        expect(bool(gap_text_order(reloaded.document()) == expected_order));
        expect(reloaded.document().root.children.size() == editor.document().root.children.size());
        gap_history(editor, original, selected);
    }
};

};
