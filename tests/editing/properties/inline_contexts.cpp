#include <cstddef>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.core.command;
import folia.markdown.dialect;
import folia.markdown.document;
import folia.core.document_edit;
import folia.core.document_ids;
import folia.markdown.document_text;
import folia.core.editor;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.core.input;
import folia.core.render_builder;
import folia.core.render_model;
import folia.markdown.serializer;
import folia.markdown.text_edit;
import folia.core.theme;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

suite editing_properties_inline_contexts_tests = [] {

"all_editable_block_contexts_share_the_source_edit_history_pipeline"_test = [] {
    const std::vector<std::string> cases{
        "abc",
        "# abc",
        "- abc",
        "- [ ] abc",
        "> abc",
        "| abc |\n| --- |",
        "> [!NOTE]\n> abc",
        "[^1]: abc",
        "- > abc",
        "> - abc",
        "- [ ] > abc",
    };
    for (auto const& markdown : cases) {
        Editor editor(markdown);
        auto const id = first_text(editor).id;
        editor.set_selection(TextSelection::caret({id, 1, TextAffinity::Downstream}));
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(bool(editor.execute_document_insert_text(editor.selection(), U"X").has_value()))) << markdown;
        const auto counters = read_core_operation_counters();
        expect(fatal(bool(counters.full_document_parses == 0u)));
        expect(fatal(bool(counters.full_document_serializations == 0u)));
        expect(fatal(bool(counters.full_tree_transaction_diffs == 0u)));
        expect(fatal(bool(counters.inline_reparses == 1u)));
        auto edited = *editor.editable_source(id);
        expect(fatal(bool(edited == U"aXbc"))) << markdown << " source=" << cps_to_utf8(edited);
        auto const* owner = find_block(editor.document().root, id);
        expect(fatal(bool(owner != nullptr)));
        if (owner) expect(fatal(bool(flatten_tokens(owner->inline_content.tree, owner->inline_content.source) == owner->inline_content.source)));
        expect(fatal(bool(editor.selection().active.container_id == id)));
        expect(fatal(bool(editor.selection().active.source_offset == 2u)));

        const auto saved = editor.markdown_utf8();
        Editor reloaded(saved);
        expect(fatal(bool(first_text(reloaded).inline_content.source == U"aXbc")));

        expect(fatal(bool(editor.undo())));
        expect(fatal(bool(editor.markdown_utf8() == before)));
        expect(fatal(bool(*editor.editable_source(id) == U"abc")));
        expect(fatal(bool(editor.redo())));
        expect(fatal(bool(*editor.editable_source(id) == U"aXbc")));
    }
};

"representative_inline_states_round_trip_through_every_editable_context"_test = [] {
    const std::vector<std::u32string> samples{
        U"abc", U"*abc*", U"_abc_", U"**abc**", U"__abc__", U"**", U"**abc",
        U"a***b***c", U"~~abc~~", U"~~abc", U"`abc`", U"`abc", U"[title](url)",
        U"[title](<url>)", U"[title](url \"name\")", U"[title](", U"![alt](url)",
        U"$abc$", U"$abc", U"\\*abc\\*", U"&amp;", U"a\\**b*",
    };

    for (const auto& sample : samples) {
        const auto original_source = U"p" + sample + U"q";
        const auto source_utf8 = cps_to_utf8(original_source);
        const std::vector<std::string> documents{
            source_utf8,
            "# " + source_utf8,
            "- " + source_utf8,
            "- [ ] " + source_utf8,
            "> " + source_utf8,
            "| " + source_utf8 + " |\n| --- |",
            "> [!NOTE]\n> " + source_utf8,
            "[^1]: " + source_utf8,
            "- > " + source_utf8,
        };

        for (const auto& markdown : documents) {
            Editor editor(markdown);
            const auto owner_id = first_text(editor).id;
            expect(fatal(bool(first_text(editor).inline_content.source == original_source)));
            const auto offset = 1 + sample.size() / 2;
            const auto before_source = editor.markdown_utf8();
            const auto before_selection = TextSelection::caret(
                {owner_id, offset, TextAffinity::Downstream});
            editor.set_selection(before_selection);

            reset_core_operation_counters();
            expect(fatal(bool(editor.execute_document_insert_text(editor.selection(), U"X").has_value())));
            const auto counters = read_core_operation_counters();
            expect(fatal(bool(counters.full_document_parses == 0u)));
            expect(fatal(bool(counters.full_document_serializations == 0u)));
            expect(fatal(bool(counters.full_tree_transaction_diffs == 0u)));
            expect(fatal(bool(counters.inline_reparses == 1u)));

            auto expected_source = original_source;
            expected_source.insert(offset, 1, U'X');
            const auto* owner = find_block(editor.document().root, owner_id);
            expect(fatal(bool(owner != nullptr)));
            if (!owner) continue;
            expect(fatal(bool(owner->inline_content.source == expected_source)));
            expect(fatal(bool(flatten_tokens(owner->inline_content.tree, owner->inline_content.source)
                == owner->inline_content.source)));
            expect(fatal(bool(editor.selection().active.container_id == owner_id)));
            expect(fatal(bool(editor.selection().active.source_offset == offset + 1)));

            const auto edited_source = editor.markdown_utf8();
            Editor reloaded(edited_source);
            expect(fatal(bool(first_text(reloaded).inline_content.source == expected_source)));

            expect(fatal(bool(editor.undo())));
            expect(fatal(bool(editor.markdown_utf8() == before_source)));
            expect(fatal(bool(editor.selection() == before_selection)));
            expect(fatal(bool(editor.redo())));
            expect(fatal(bool(editor.markdown_utf8() == edited_source)));
            expect(fatal(bool(*editor.editable_source(owner_id) == expected_source)));
        }
    }
};

"random_editor_source_edits_restore_exactly_across_editable_contexts"_test = [] {
    std::mt19937_64 random{0xED1705u};
    const std::u32string alphabet = U"abc *_~`[]()!$\\&;'\"😀";
    const auto make_document = [](std::size_t context, std::u32string_view source) {
        const auto text = cps_to_utf8(source);
        switch (context) {
            case 0: return text;
            case 1: return "# " + text;
            case 2: return "- " + text;
            case 3: return "> " + text;
            case 4: return "| " + text + " |\n| --- |";
            case 5: return "> [!NOTE] " + text + "\n> body";
            case 6: return "> [!NOTE] title\n> " + text;
            default: return "[^n]: " + text;
        }
    };

    for (std::size_t iteration = 0; iteration < 240; ++iteration) {
        std::u32string original = U"p";
        const auto random_length = static_cast<std::size_t>(random() % 32);
        for (std::size_t index = 0; index < random_length; ++index) {
            original.push_back(alphabet[random() % alphabet.size()]);
        }
        original.push_back(U'q');

        const auto context = iteration % 8;
        Editor editor(make_document(context, original));
        const auto fragments = document_text_fragments(editor.document());
        auto owner = std::find_if(fragments.begin(), fragments.end(), [&](const auto& fragment) {
            return fragment.text == original;
        });
        expect(fatal(bool(owner != fragments.end()))) << iteration;
        if (owner == fragments.end()) continue;

        auto expected = original;
        const auto insert = (random() & 1u) != 0;
        const auto offset = insert
            ? static_cast<std::size_t>(random() % (original.size() + 1))
            : 1 + static_cast<std::size_t>(random() % original.size());
        const auto before = TextSelection::caret({
            owner->container_id,
            offset,
            TextAffinity::Downstream,
        });
        editor.set_selection(before);
        const auto before_markdown = editor.markdown_utf8();

        if (insert) expected.insert(offset, 1, U'X');
        else {
            expected.erase(offset - 1, 1);
            // The command removes a complete formatting pair when its target
            // is a delimiter. Build the oracle from the original CST, before
            // mutation; ordinary/incomplete text still loses one character.
            const auto* original_block = find_block(editor.document().root, owner->container_id);
            std::vector<const InlineCstNode*> pending;
            for (const auto& node : original_block->inline_content.tree.nodes) pending.push_back(&node);
            while (!pending.empty()) {
                const auto* node = pending.back(); pending.pop_back();
                for (const auto& child : node->children) pending.push_back(&child);
                if (node->status != ParseStatus::Complete || !node->delimited || !node->delimited->closing) continue;
                const auto paired = node->kind == InlineCstKind::Emphasis || node->kind == InlineCstKind::Strong
                    || node->kind == InlineCstKind::Strikethrough || node->kind == InlineCstKind::CodeSpan
                    || node->kind == InlineCstKind::InlineMath;
                const auto& parts = *node->delimited;
                const auto character = offset - 1;
                const auto in_marker = (character >= parts.opening.start && character < parts.opening.end)
                    || (character >= parts.closing->start && character < parts.closing->end);
                if (!paired || !in_marker) continue;
                expected = original.substr(0, node->range.start)
                    + original.substr(parts.content.start, parts.content.length())
                    + original.substr(node->range.end);
                break;
            }
        }

        reset_core_operation_counters();
        auto transaction = insert
            ? editor.execute_document_insert_text(editor.selection(), U"X")
            : editor.execute_document_delete_backward(editor.selection());
        expect(fatal(bool(transaction.has_value()))) << iteration;
        if (!transaction) continue;
        const auto counters = read_core_operation_counters();
        expect(fatal(bool(counters.full_document_parses == 0u))) << iteration;
        expect(fatal(bool(counters.full_document_serializations == 0u))) << iteration;
        expect(fatal(bool(counters.full_tree_transaction_diffs == 0u))) << iteration;
        expect(fatal(bool(counters.inline_reparses == 1u))) << iteration;

        const auto* block = find_block(editor.document().root, owner->container_id);
        expect(fatal(bool(block != nullptr))) << iteration;
        if (!block) continue;
        const auto* inline_document = editable_inline_document(*block);
        expect(fatal(bool(inline_document != nullptr))) << iteration;
        if (!inline_document) continue;
        expect(fatal(bool(inline_document->source == expected))) << iteration;
        expect(fatal(bool(flatten_tokens(inline_document->tree, inline_document->source)
            == inline_document->source))) << iteration;
        expect(fatal(bool(editor.selection().active.container_id == owner->container_id))) << iteration;
        expect(fatal(bool(editor.selection().active.source_offset <= expected.size()))) << iteration;

        const auto after_markdown = editor.markdown_utf8();
        const auto after_selection = editor.selection();
        Editor reloaded(after_markdown);
        const auto reloaded_fragments = document_text_fragments(reloaded.document());
        expect(fatal(bool(std::ranges::any_of(reloaded_fragments, [&](const auto& fragment) {
            return fragment.text == expected;
        })))) << iteration;

        expect(fatal(bool(editor.undo()))) << iteration;
        expect(fatal(bool(editor.markdown_utf8() == before_markdown))) << iteration;
        expect(fatal(bool(editor.selection() == before))) << iteration;
        expect(fatal(bool(editor.redo()))) << iteration;
        expect(fatal(bool(editor.markdown_utf8() == after_markdown))) << iteration;
        expect(fatal(bool(editor.selection() == after_selection))) << iteration;
        expect(fatal(bool(*editor.editable_source(owner->container_id) == expected))) << iteration;
    }
};

};
