#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.document_text_navigation;
import folia.core.editor;
import folia.core.source_editor;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.serializer;
import folia.markdown.text_edit;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/editor_test_support.hpp"

#include "editing/support/text_commands.hpp"

suite editing_commands_word_navigation_tests = [] {

"rendered_word_commands_delete_body_words_with_exact_local_history"_test = [] {
    const std::vector<std::string> fixtures{
        "alpha beta gamma", "**alpha** beta gamma", "# alpha beta gamma",
        "```cpp\r\nalpha beta gamma\r\n```", "    alpha beta gamma",
        "> ```cpp\r\n> alpha beta gamma\r\n> ```", "- alpha beta gamma",
        "<p>alpha beta gamma</p>", "<pre>alpha beta gamma</pre>",
    };
    for (const auto& source : fixtures) for (const auto backward : {false, true}) {
        Editor editor(source);
        const auto id = editor.document().cached_editable_order.front();
        const auto& owner = *find_document_block(editor.document(), id);
        const auto text = editable_raw_block_source(owner) ? owner.block_source.tree().content : owner.inline_content().source;
        const auto word = text.find(U"beta");
        expect(fatal(word != std::u32string::npos)) << source;
        const auto selection = TextSelection::caret(text_command_content_position(owner, word + (backward ? 4u : 0u)));
        editor.set_selection(selection);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = backward ? CommandKind::DeleteWordBackward
            : CommandKind::DeleteWordForward}))) << source;
        text_command_local_work();
        const auto after = editor.markdown_utf8();
        expect(after.find("beta") == std::string::npos) << source;
        expect(after.find("alpha") != std::string::npos && after.find("gamma") != std::string::npos) << source;
        text_command_history(editor, before, selection);
        text_command_reload_consistency(editor);
    }
};

"rendered_word_commands_replace_forward_and_reversed_selections"_test = [] {
    for (const auto reversed : {false, true}) for (const auto kind : {
            CommandKind::DeleteWordBackward, CommandKind::DeleteWordForward}) {
        Editor editor("alpha\r\n\r\nbeta");
        const auto order = editor.document().cached_editable_order;
        const TextPosition begin{order.front(), 2};
        const TextPosition end{order.back(), 2};
        const auto selected = reversed ? TextSelection{end, begin} : TextSelection{begin, end};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = kind})));
        text_command_local_work();
        expect(editor.markdown_utf8() == "alta");
        text_command_history(editor, before, selected);
    }
};

"rendered_word_navigation_preserves_unicode_clusters_and_raw_boundaries"_test = [] {
    const std::u32string body = U"e\u0301lan \U0001f469\u200d\U0001f4bb \U0001f1e8\U0001f1f3\r\nnext";
    const std::vector<std::u32string> fixtures{body, U"```\r\n" + body + U"\r\n```", U"    e\u0301lan \U0001f469\u200d\U0001f4bb \U0001f1e8\U0001f1f3\r\n    next"};
    for (const auto& fixture : fixtures) {
        Editor editor(cps_to_utf8(fixture));
        const auto& owner = *find_document_block(editor.document(), editor.document().cached_editable_order.front());
        const auto text = editable_raw_block_source(owner) ? owner.block_source.tree().content : owner.inline_content().source;
        const auto range = *editable_block_source_range(owner);
        for (const auto backward : {false, true}) {
            for (auto cursor = std::size_t{0}; cursor <= text.size();) {
                const auto initial = text_command_content_position(owner, cursor);
                editor.set_selection(TextSelection::caret(initial));
                reset_core_operation_counters();
                const auto moved = editor.execute_command({.kind = backward ? CommandKind::MoveWordLeft : CommandKind::MoveWordRight});
                expect(moved == (backward ? cursor != 0 : cursor != text.size()));
                text_command_local_work();
                const auto target = editor.selection().active;
                expect(target.container_id == owner.id);
                expect(target.source_offset >= range.start && target.source_offset <= range.end);
                const auto local = editable_raw_block_source(owner)
                    ? static_cast<std::size_t>(owner.block_source.tree().content_to_source.lower_bound(target.source_offset)
                        - owner.block_source.tree().content_to_source.begin()) : target.source_offset;
                expect(is_grapheme_boundary_char(text, local));
                if (backward && cursor != 0) expect(target.source_offset < initial.source_offset);
                if (!backward && cursor != text.size()) expect(target.source_offset > initial.source_offset);
                if (cursor == text.size()) break;
                cursor = next_grapheme_boundary_char(text, cursor);
            }
        }
        expect(!editor.has_undo());
    }
};

"rendered_paragraph_and_page_commands_make_progress_across_owners"_test = [] {
    for (const auto source : {"first\r\nwrap\r\n\r\nsecond\r\nmore\r\n\r\nlast",
            "first\n\n```cpp\nbody\nnext\n```\n\n***\n\nlast",
            "- first\n- second\n\n> third\n\nlast"}) {
        for (const auto pair : {std::pair{CommandKind::MoveParagraphEnd, CommandKind::MoveParagraphStart},
                std::pair{CommandKind::MovePageDown, CommandKind::MovePageUp}}) {
            Editor editor(source);
            const auto order = editor.document().cached_editable_order;
            const auto& first = *find_document_block(editor.document(), order.front());
            const auto& last = *find_document_block(editor.document(), order.back());
            const TextPosition start{first.id, editable_block_source_range(first)->start};
            const TextPosition finish{last.id, editable_block_source_range(last)->end};
            editor.set_selection(TextSelection::caret(start));
            for (std::size_t step = 0; step < 30 && editor.selection().active != finish; ++step) {
                const auto before = editor.selection();
                expect(editor.execute_command({.kind = pair.first, .extend_selection = true, .page_line_count = 2}));
                expect(editor.selection() != before);
                expect(editor.selection().anchor == start);
                if (editor.selection().active.container_id == finish.container_id
                    && editor.selection().active.source_offset == finish.source_offset) break;
            }
            expect(editor.selection().active.container_id == finish.container_id);
            expect(editor.selection().active.source_offset == finish.source_offset);
            editor.set_selection(TextSelection::caret(finish));
            for (std::size_t step = 0; step < 30; ++step) {
                const auto before = editor.selection();
                expect(editor.execute_command({.kind = pair.second, .extend_selection = true, .page_line_count = 2}));
                expect(editor.selection() != before);
                expect(editor.selection().anchor == finish);
                if (editor.selection().active.container_id == start.container_id
                    && editor.selection().active.source_offset == start.source_offset) break;
            }
            expect(editor.selection().active.container_id == start.container_id);
            expect(editor.selection().active.source_offset == start.source_offset);
            expect(!editor.has_undo());
        }
    }
};

"rendered_raw_word_selection_excludes_hidden_fence_and_indentation_source"_test = [] {
    for (const auto source : {"```\n  alpha  \n```", "    alpha\n        beta", "<pre>  alpha  </pre>"}) {
        Editor editor(source);
        const auto& owner = *find_document_block(editor.document(), editor.document().cached_editable_order.front());
        const auto body = owner.block_source.tree().content;
        const auto editable = *editable_block_source_range(owner);
        for (std::size_t index = 0; index < body.size(); ++index) {
            const auto selected = document_word_selection_at(editor.document(), text_command_content_position(owner, index));
            expect(fatal(selected.has_value())) << source;
            expect(selected->anchor.source_offset >= editable.start);
            expect(selected->active.source_offset <= editable.end);
            expect(selected->anchor.source_offset < selected->active.source_offset);
        }
    }
};

};
