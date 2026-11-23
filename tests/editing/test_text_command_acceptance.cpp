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

namespace {

void text_command_local_work() {
    const auto counts = read_core_operation_counters();
    expect(counts.full_document_parses == 0_u);
    expect(counts.full_document_serializations == 0_u);
    expect(counts.full_tree_transaction_diffs == 0_u);
}

void text_command_history(Editor& editor, const std::string& before, TextSelection selection) {
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

std::vector<std::pair<BlockKind, std::u32string>> text_command_visible_owners(const Editor& editor) {
    std::vector<std::pair<BlockKind, std::u32string>> result;
    for (const auto id : editor.document().cached_editable_order) {
        const auto& block = *find_document_block(editor.document(), id);
        if (editable_raw_block_source(block)) result.emplace_back(block.kind, block.block_source.tree().content);
        else if (const auto* inlines = editable_inline_document(block)) result.emplace_back(block.kind, inlines->source);
        else result.emplace_back(block.kind, U"\ufffc");
    }
    return result;
}

void text_command_reload_consistency(const Editor& editor) {
    Editor reloaded(editor.markdown_utf8());
    expect(reloaded.markdown_utf8() == editor.markdown_utf8());
    expect(bool(text_command_visible_owners(reloaded) == text_command_visible_owners(editor)));
}

TextPosition text_command_content_position(const BlockNode& block, std::size_t offset) {
    const auto source_offset = editable_raw_block_source(block)
        ? block.block_source.tree().content_to_source[offset] : offset;
    return {block.id, source_offset, TextAffinity::Downstream};
}

suite text_command_acceptance_tests = [] {

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
        const auto text = editable_raw_block_source(owner) ? owner.block_source.tree().content : owner.inline_content.source;
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
        const auto text = editable_raw_block_source(owner) ? owner.block_source.tree().content : owner.inline_content.source;
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

"rendered_tab_reclassifies_indented_paragraphs_and_preserves_selection_direction"_test = [] {
    for (const auto source : {"alpha", "alpha\r\nbeta", "> alpha\r\n> beta", "alpha\n\nbeta"})
    for (const auto reversed : {false, true}) {
        Editor editor(source);
        expect(fatal(editor.execute_command({.kind = CommandKind::SelectAll})));
        auto selection = editor.selection();
        if (reversed) std::swap(selection.anchor, selection.active);
        editor.set_selection(selection);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::TabForward}))) << source;
        text_command_local_work();
        expect(editor.markdown_utf8() != before);
        expect(!editor.selection().is_caret());
        const auto start_index = *document_editable_order_position(editor.document(), editor.selection().anchor.container_id);
        const auto end_index = *document_editable_order_position(editor.document(), editor.selection().active.container_id);
        expect(reversed ? start_index >= end_index : start_index <= end_index);
        text_command_reload_consistency(editor);
        text_command_history(editor, before, selection);
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::TabBackward}))) << source;
        text_command_local_work();
        expect(editor.markdown_utf8() == before) << source;
        text_command_reload_consistency(editor);
    }
    Editor caret_editor("alpha");
    const auto before = caret_editor.markdown_utf8();
    const auto selected = caret(caret_editor.document().root.children.front(), 0);
    caret_editor.set_selection(selected);
    reset_core_operation_counters();
    expect(fatal(caret_editor.execute_command({.kind = CommandKind::TabForward})));
    text_command_local_work();
    expect(caret_editor.markdown_utf8() == "    alpha");
    expect(caret_editor.document().root.children.front().kind == BlockKind::CodeBlock);
    text_command_reload_consistency(caret_editor);
    text_command_history(caret_editor, before, selected);
    expect(fatal(caret_editor.execute_command({.kind = CommandKind::TabBackward})));
    expect(caret_editor.markdown_utf8() == before);
    expect(caret_editor.document().root.children.front().kind == BlockKind::Paragraph);
};

"rendered_indentation_excludes_an_endpoint_at_the_next_line_start"_test = [] {
    for (const auto reversed : {false, true}) {
        Editor editor("alpha\r\nbeta\r\ngamma");
        const auto id = editor.document().cached_editable_order.front();
        const TextPosition start{id, 0};
        const TextPosition end{id, 7};
        const auto selected = reversed ? TextSelection{end, start} : TextSelection{start, end};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::IndentText})));
        text_command_local_work();
        expect(editor.markdown_utf8() == "    alpha\r\nbeta\r\ngamma");
        text_command_reload_consistency(editor);
        text_command_history(editor, before, selected);
    }
};

"rendered_heading_tab_indents_exact_markdown_and_backtab_restores_heading"_test = [] {
    const std::vector<std::pair<std::string, std::string>> fixtures{
        {"# alpha", "    # alpha"},
        {"## alpha ##", "    ## alpha ##"},
        {"alpha\r\n=====", "    alpha\r\n    ====="},
        {"alpha\r\nbeta\r\n---  ", "    alpha\r\n    beta\r\n    ---  "},
        {"> # alpha", ">     # alpha"},
        {"> alpha\r\n> ===", ">     alpha\r\n>     ==="},
    };
    for (const auto& [source, expected] : fixtures) for (const auto reversed : {false, true}) {
        Editor editor(source);
        const auto& heading = *find_document_block(editor.document(), editor.document().cached_editable_order.front());
        expect(fatal(heading.kind == BlockKind::Heading)) << source;
        const TextPosition start{heading.id, 0};
        const TextPosition end{heading.id, heading.inline_content.source.size()};
        const auto selected = reversed ? TextSelection{end, start} : TextSelection{start, end};
        editor.set_selection(selected);
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::TabForward}))) << source;
        text_command_local_work();
        expect(editor.markdown_utf8() == expected) << source;
        text_command_reload_consistency(editor);
        text_command_history(editor, source, selected);
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::TabBackward}))) << source;
        text_command_local_work();
        expect(editor.markdown_utf8() == source);
        text_command_reload_consistency(editor);
    }
    for (const auto& [source, expected] : fixtures) {
        Editor editor(source);
        const auto id = editor.document().cached_editable_order.front();
        editor.set_selection(TextSelection::caret({id, 0}));
        expect(fatal(editor.execute_command({.kind = CommandKind::TabForward}))) << source;
        expect(editor.markdown_utf8() == expected);
        expect(fatal(editor.execute_command({.kind = CommandKind::TabBackward}))) << source;
        expect(editor.markdown_utf8() == source);
        text_command_reload_consistency(editor);
        editor.set_selection(TextSelection::caret({id, 1}));
        expect(fatal(editor.execute_command({.kind = CommandKind::TabForward})));
        expect(find_document_block(editor.document(), id)->kind == BlockKind::Heading);
        text_command_reload_consistency(editor);
    }
};

"rendered_code_outdent_reclassifies_literal_atomic_and_container_syntax"_test = [] {
    for (const auto source : {"    ***", "    [TOC]", "    - alpha", "    <p>alpha</p>"}) {
        Editor editor(source);
        const auto& code = *find_document_block(editor.document(), editor.document().cached_editable_order.front());
        const auto selected = TextSelection::caret(text_command_content_position(code, 0));
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::TabBackward}))) << source;
        text_command_local_work();
        expect(editor.markdown_utf8() == std::string(source).substr(4));
        text_command_reload_consistency(editor);
        text_command_history(editor, before, selected);
    }
};

"typing_after_empty_paragraph_indentation_activates_code_without_global_work"_test = [] {
    for (const auto prefix : {U"    ", U"\t", U" \t"}) for (const auto tab : {false, true}) {
        if (tab && std::u32string_view(prefix) != U"    ") continue;
        Editor editor("");
        const auto initial = editor.selection();
        expect(fatal(editor.execute_command(tab ? Command{.kind = CommandKind::TabForward}
            : Command::InsertText(prefix))));
        const auto blank_source = editor.markdown_utf8();
        const auto blank_selection = editor.selection();
        expect(blank_source == cps_to_utf8(prefix));
        reset_core_operation_counters();
        expect(fatal(editor.execute_command(Command::InsertText(U"alpha"))));
        text_command_local_work();
        expect(read_core_operation_counters().block_fragment_parses == 1_u);
        expect(editor.markdown_utf8() == cps_to_utf8(std::u32string(prefix) + U"alpha"));
        const auto* code = find_document_block(editor.document(), editor.selection().active.container_id);
        expect(fatal(code != nullptr && code->kind == BlockKind::CodeBlock));
        expect(bool(code->block_source.tree().content == U"alpha"));
        text_command_reload_consistency(editor);
        text_command_history(editor, blank_source, blank_selection);
        expect(fatal(editor.undo()));
        expect(fatal(editor.undo()));
        expect(editor.markdown_utf8().empty());
        expect(editor.selection() == initial);
    }
    Editor ordinary("alpha");
    ordinary.set_selection(caret(ordinary.document().root.children.front(), 2));
    reset_core_operation_counters();
    expect(fatal(ordinary.execute_command(Command::InsertText(U"X"))));
    expect(read_core_operation_counters().block_fragment_parses == 0_u);
    text_command_local_work();
    Editor html("<p>alpha</p>");
    html.set_selection(TextSelection::caret({html.document().cached_editable_order.front(), 0}));
    reset_core_operation_counters();
    expect(fatal(html.execute_command(Command::InsertText(U"    "))));
    expect(read_core_operation_counters().block_fragment_parses == 0_u);
    text_command_local_work();
    expect(html.markdown_utf8() == "<p>    alpha</p>");
    text_command_reload_consistency(html);
};

"rendered_tab_and_backtab_preserve_raw_fences_crlf_and_container_prefixes"_test = [] {
    const std::vector<std::string> fixtures{
        "```cpp\r\nalpha\r\nbeta\r\n```", "    alpha\r\n    beta",
        "> ```cpp\r\n> alpha\r\n> beta\r\n> ```",
        "- ```cpp\r\n  alpha\r\n  beta\r\n  ```",
        "<pre>alpha\r\nbeta</pre>",
    };
    for (const auto& source : fixtures) for (const auto reversed : {false, true}) {
        Editor editor(source);
        const auto& owner = *find_document_block(editor.document(), editor.document().cached_editable_order.front());
        const auto range = *editable_block_source_range(owner);
        const TextPosition start{owner.id, range.start};
        const TextPosition end{owner.id, range.end};
        const auto selected = reversed ? TextSelection{end, start} : TextSelection{start, end};
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        reset_core_operation_counters();
        // Use explicit text indentation within list-owned raw blocks: a Tab
        // on the list item itself has the structural list indentation policy.
        expect(fatal(editor.execute_command({.kind = CommandKind::IndentText}))) << source;
        text_command_local_work();
        expect(editor.markdown_utf8() != before);
        text_command_reload_consistency(editor);
        text_command_history(editor, before, selected);
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::OutdentText}))) << source;
        text_command_local_work();
        expect(editor.markdown_utf8() == before) << source;
        text_command_reload_consistency(editor);
    }
};

"rendered_tab_prioritizes_code_body_and_table_cells_inside_lists"_test = [] {
    for (const auto delimiter : {"```cpp", "$$"}) {
        const auto opening = std::string(delimiter);
        const auto closing = opening == "```cpp" ? "```" : "$$";
        Editor editor("- first\n- " + opening + "\n  alpha\n  " + closing);
        const auto& owner = *find_document_block(editor.document(), editor.document().cached_editable_order.back());
        expect(fatal(owner.kind == (opening == "```cpp" ? BlockKind::CodeBlock : BlockKind::MathBlock)));
        const auto selected = TextSelection::caret(text_command_content_position(owner, 0));
        editor.set_selection(selected);
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_command({.kind = CommandKind::TabForward})));
        expect(editor.markdown_utf8() == "- first\n- " + opening + "\n      alpha\n  " + closing);
        expect(editor.document().root.children.front().children.size() == 2_u);
        expect(fatal(editor.execute_command({.kind = CommandKind::TabBackward})));
        expect(editor.markdown_utf8() == before);
        expect(editor.document().root.children.front().children.size() == 2_u);
    }
    {
        Editor editor("- first\n- | aa | bb |\n  | --- | --- |\n  | cc | dd |");
        const auto order = editor.document().cached_editable_order;
        expect(fatal(order.size() == 5_u));
        editor.set_selection(TextSelection::caret({order[1], 0}));
        const auto before = editor.markdown_utf8();
        expect(fatal(editor.execute_command({.kind = CommandKind::TabForward})));
        expect(editor.selection().active.container_id == order[2]);
        expect(editor.markdown_utf8() == before);
        expect(!editor.has_undo());
        expect(fatal(editor.execute_command({.kind = CommandKind::TabBackward})));
        expect(editor.selection().active.container_id == order[1]);
        expect(editor.markdown_utf8() == before);
    }
};

"source_tab_routing_stays_literal_inside_nested_code_math_and_tables"_test = [] {
    const std::vector<std::string> fixtures{
        "- first\r\n- ```cpp\r\n  alpha\r\n  ```",
        "- first\r\n- $$\r\n  alpha\r\n  $$",
        "- first\r\n- | aa | bb |\r\n  | --- | --- |\r\n  | cc | dd |",
    };
    for (const auto& source : fixtures) for (const auto reversed : {false, true}) {
        Editor editor(source);
        expect(fatal(editor.enter_source_mode()));
        const auto end = source.find("\r\n", 9);
        const auto selected = reversed ? SourceSelection{end, 9} : SourceSelection{9, end};
        editor.set_source_selection(selected);
        const auto before = editor.source_editor()->source();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::TabForward})));
        text_command_local_work();
        auto expected = before;
        expected.insert(9, U"    ");
        expect(editor.source_editor()->source() == expected);
        expect(!editor.source_editor()->selection().is_caret());
        expect(fatal(editor.execute_command({.kind = CommandKind::TabBackward})));
        expect(editor.source_editor()->source() == before);
        expect(editor.source_editor()->selection() == selected);
        expect(fatal(editor.undo()));
        expect(editor.source_editor()->source() == expected);
        expect(fatal(editor.undo()));
        expect(editor.source_editor()->source() == before);
        expect(editor.source_editor()->selection() == selected);
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

} // namespace
