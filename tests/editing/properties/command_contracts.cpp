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

suite editing_properties_command_contracts_tests = [] {

"derived_symbols_and_outline_refresh_after_structure_edits"_test = [] {
    Editor editor("title");
    editor.set_selection(caret(first_text(editor), 0));
    reset_core_operation_counters();
    expect(fatal(bool(editor.execute_document_set_heading(editor.selection(), 2).has_value())));
    const auto counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_document_block_index_scans == 0u)));
    expect(fatal(bool(counters.incremental_document_block_index_repairs == 1u)));
    expect(fatal(bool(counters.full_document_symbol_derivations == 0u)));
    expect(fatal(bool(counters.full_document_outline_derivations == 1u)));
    expect(fatal(document_indexes_are_exact(editor.document())));
    expect(fatal(bool(editor.document().root.children.front().kind == BlockKind::Heading)));
    expect(fatal(bool(editor.symbols().headings.size() == 1u)));
    expect(fatal(bool(editor.outline().items.size() == 1u)));
    expect(fatal(bool(editor.outline().items.front().title_plain_text == "title")));
};

"set_dialect_reloads_explicitly_and_keeps_one_selection"_test = [] {
    MarkdownDialect dialect = default_dialect();
    dialect.math.inline_dollar = false;
    Editor editor("$x$", dialect);
    expect(fatal(bool(!inline_contains_kind(first_text(editor).inline_content(), InlineCstKind::InlineMath))));
    editor.set_selection(caret(first_text(editor), 1));
    expect(fatal(bool(editor.execute_document_insert_text(editor.selection(), U"y").has_value())));
    expect(fatal(bool(editor.has_undo())));
    const auto before_reload_revision = editor.revision();
    dialect.math.inline_dollar = true;
    editor.set_dialect(dialect);
    expect(fatal(bool(inline_contains_kind(first_text(editor).inline_content(), InlineCstKind::InlineMath))));
    expect(fatal(bool(editor.selection().active.container_id == first_text(editor).id)));
    expect(fatal(bool(editor.revision() == before_reload_revision + 1)));
    expect(fatal(bool(!editor.has_undo())));
    expect(fatal(bool(!editor.has_redo())));
};

"input_events_translate_to_commands_without_mutating_state"_test = [] {
    Editor editor("abc");
    EditorInputEvent text;
    text.kind = EditorInputEvent::Kind::TextInput;
    text.text_input.kind = TextInputEvent::Kind::InsertText;
    text.text_input.text = U"X";
    auto command = editor.handle_input(text);
    expect(fatal(bool(command.has_value())));
    expect(fatal(bool(command->kind == CommandKind::InsertText)));
    expect(fatal(bool(command->text == U"X")));
    expect(fatal(bool(editor.markdown_utf8() == "abc")));

    EditorInputEvent key;
    key.kind = EditorInputEvent::Kind::KeyDown;
    key.key.key_code = KeyCode::Backspace;
    command = editor.handle_input(key);
    expect(fatal(bool(command && command->kind == CommandKind::DeleteBackward)));
};

"selection_validation_rejects_foreign_or_out_of_range_positions"_test = [] {
    Editor editor("abc");
    bool foreign = false;
    try { editor.set_selection(TextSelection::caret({NodeId(999999), 0, TextAffinity::Downstream})); }
    catch (const std::out_of_range&) { foreign = true; }
    expect(fatal(bool(foreign)));
    bool outside = false;
    try { editor.set_selection(caret(first_text(editor), 4)); }
    catch (const std::out_of_range&) { outside = true; }
    expect(fatal(bool(outside)));
};

"enter_replaces_the_source_selection_before_applying_block_semantics"_test = [] {
    auto exercise = [](std::string markdown, std::u32string_view selected_source,
                       std::u32string_view expected_first, std::u32string_view expected_second) {
        Editor editor(std::move(markdown));
        const auto fragments = document_text_fragments(editor.document());
        auto selected = std::find_if(fragments.begin(), fragments.end(), [&](const auto& fragment) {
            return fragment.text == selected_source;
        });
        expect(fatal(bool(selected != fragments.end())));
        if (selected == fragments.end()) return;

        const auto before_markdown = editor.markdown_utf8();
        const TextSelection before{
            {selected->container_id, 1, TextAffinity::Downstream},
            {selected->container_id, 3, TextAffinity::Downstream},
        };
        editor.set_selection(before);
        reset_core_operation_counters();
        auto transaction = editor.execute_document_enter(editor.selection());
        expect(fatal(bool(transaction.has_value())));
        if (!transaction) return;
        const auto counters = read_core_operation_counters();
        expect(fatal(bool(counters.full_document_parses == 0u)));
        expect(fatal(bool(counters.full_document_serializations == 0u)));
        expect(fatal(bool(counters.full_tree_transaction_diffs == 0u)));

        const auto after_fragments = document_text_fragments(editor.document());
        auto first = std::find_if(after_fragments.begin(), after_fragments.end(), [&](const auto& fragment) {
            return fragment.text == expected_first;
        });
        auto second = std::find_if(after_fragments.begin(), after_fragments.end(), [&](const auto& fragment) {
            return fragment.text == expected_second;
        });
        expect(fatal(bool(first != after_fragments.end())));
        expect(fatal(bool(second != after_fragments.end())));
        const auto after_markdown = editor.markdown_utf8();
        const auto after_selection = editor.selection();

        expect(fatal(bool(editor.undo())));
        expect(fatal(bool(editor.markdown_utf8() == before_markdown)));
        expect(fatal(bool(editor.selection() == before)));
        expect(fatal(bool(editor.redo())));
        expect(fatal(bool(editor.markdown_utf8() == after_markdown)));
        expect(fatal(bool(editor.selection() == after_selection)));
    };

    exercise("abcd", U"abcd", U"a", U"d");
    exercise("> abcd", U"abcd", U"a", U"d");
    exercise("- abcd", U"abcd", U"a", U"d");
    exercise("> [!NOTE] abcd\n> body", U"abcd", U"a", U"d");
    exercise("| abcd |\n| --- |", U"abcd", U"a<br>d", U"a<br>d");
};

};
