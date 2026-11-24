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

suite editing_history_inline_transactions_tests = [] {

"insert_undo_redo_restore_source_and_selection_exactly"_test = [] {
    Editor editor("abc");
    editor.set_selection(caret(first_text(editor), 1));
    const auto before_selection = editor.selection();
    expect(fatal(bool(editor.execute_document_insert_text(editor.selection(), U"X").has_value())));
    expect(fatal(bool(editor.markdown_utf8() == "aXbc")));
    const auto after_selection = editor.selection();
    expect(fatal(bool(after_selection.active.source_offset == 2u)));
    expect(fatal(bool(editor.has_undo())));
    expect(fatal(bool(editor.undo())));
    expect(fatal(bool(editor.markdown_utf8() == "abc")));
    expect(fatal(bool(editor.selection() == before_selection)));
    expect(fatal(bool(editor.has_redo())));
    expect(fatal(bool(editor.redo())));
    expect(fatal(bool(editor.markdown_utf8() == "aXbc")));
    expect(fatal(bool(editor.selection() == after_selection)));
};

"inline_html_edit_undo_redo_preserves_source_and_nested_markdown_exactly"_test = [] {
    constexpr std::string_view source =
        "before <span style=\"color:#123456\"><strong>*literal*</strong></span> after";
    Editor editor{std::string(source)};
    const auto literal = source.find("literal");
    expect(fatal(bool(literal != std::string_view::npos)));
    if (literal == std::string_view::npos) return;

    const auto before = caret(first_text(editor), literal + 3u);
    editor.set_selection(before);
    reset_core_operation_counters();
    expect(bool(editor.execute_document_insert_text(
        editor.selection(), U"X").has_value())) << "insert";
    const auto counters = read_core_operation_counters();
    expect(bool(counters.full_document_parses == 0u)) << "no full parse";
    expect(bool(counters.full_document_serializations == 0u)) << "no full serialize";
    const auto edited = std::string(source.substr(0, literal + 3u))
        + "X"
        + std::string(source.substr(literal + 3u));
    expect(bool(editor.markdown_utf8() == edited)) << "edited source";
    expect(bool(inline_contains_kind(
        first_text(editor).inline_content(),
        InlineCstKind::HtmlElement))) << "html element";
    expect(bool(inline_contains_kind(
        first_text(editor).inline_content(),
        InlineCstKind::Emphasis))) << "markdown retained inside inline html";
    expect(bool(flatten_tokens(
        first_text(editor).inline_content().tree,
        first_text(editor).inline_content().source)
        == first_text(editor).inline_content().source)) << "lossless flatten";
    const auto after = editor.selection();

    expect(bool(editor.undo())) << "undo";
    expect(bool(editor.markdown_utf8() == source)) << "undo source";
    expect(bool(editor.selection() == before)) << "undo selection";
    expect(bool(editor.redo())) << "redo";
    expect(bool(editor.markdown_utf8() == edited)) << "redo source";
    expect(bool(editor.selection() == after)) << "redo selection";
};

"ordinary_block_separators_preserve_selection_source_and_history_exactly"_test = [] {
    struct Case {
        std::string source;
        std::string edited;
        BlockKind first_kind;
    };
    const std::vector<Case> cases{
        {"first\n\nsecond", "first\n\nsXecond", BlockKind::Paragraph},
        {"# first\n\nsecond", "# first\n\nsXecond", BlockKind::Heading},
        {"first\r\n\r\nsecond", "first\r\n\r\nsXecond", BlockKind::Paragraph},
        {"# first\r\n\r\nsecond", "# first\r\n\r\nsXecond", BlockKind::Heading},
    };

    for (const auto& entry : cases) {
        Editor editor(entry.source);
        expect(fatal(bool(editor.document().root.children.size() == 2u))) << entry.source;
        if (editor.document().root.children.size() != 2u) continue;
        const auto& first = editor.document().root.children[0];
        const auto& second = editor.document().root.children[1];
        expect(fatal(bool(first.kind == entry.first_kind))) << entry.source;
        expect(fatal(bool(second.kind == BlockKind::Paragraph))) << entry.source;
        expect(fatal(bool(first.inline_content().source == U"first"))) << entry.source;
        expect(fatal(bool(second.inline_content().source == U"second"))) << entry.source;

        const auto before = TextSelection::caret(
            {second.id, 1u, TextAffinity::Downstream});
        editor.set_selection(before);
        expect(fatal(bool(editor.execute_document_insert_text(
            editor.selection(), U"X").has_value()))) << entry.source;
        expect(fatal(bool(editor.markdown_utf8() == entry.edited))) << entry.source;
        expect(fatal(bool(editor.document().root.children.size() == 2u))) << entry.source;
        expect(fatal(bool(*editor.editable_source(second.id) == U"sXecond"))) << entry.source;
        const auto after = editor.selection();
        expect(fatal(bool(after.active.container_id == second.id))) << entry.source;
        expect(fatal(bool(after.active.source_offset == 2u))) << entry.source;

        Editor reloaded(editor.markdown_utf8());
        expect(fatal(bool(reloaded.document().root.children.size() == 2u))) << entry.source;
        if (reloaded.document().root.children.size() == 2u) {
            expect(fatal(bool(reloaded.document().root.children[1].inline_content().source
                == U"sXecond"))) << entry.source;
        }

        expect(fatal(bool(editor.undo()))) << entry.source;
        expect(fatal(bool(editor.markdown_utf8() == entry.source))) << entry.source;
        expect(fatal(bool(editor.selection() == before))) << entry.source;
        expect(fatal(bool(editor.redo()))) << entry.source;
        expect(fatal(bool(editor.markdown_utf8() == entry.edited))) << entry.source;
        expect(fatal(bool(editor.selection() == after))) << entry.source;
    }
};

"editor_reports_local_text_changes_and_structural_fallbacks"_test = [] {
    Editor editor("abc");
    editor.set_selection(caret(first_text(editor), 1));
    expect(fatal(editor.execute_command(Command::InsertText(U"X"))));
    auto inserted = editor.take_last_document_change();
    expect(fatal(bool(inserted.has_value())));
    if (!inserted) return;
    expect(fatal(!inserted->structural));
    expect(fatal(inserted->forward));
    expect(fatal(bool(inserted->text_operations.size() == 1u)));
    expect(fatal(bool(inserted->text_operations.front().forward.replacement == U"X")));

    expect(fatal(editor.undo()));
    auto undone = editor.take_last_document_change();
    expect(fatal(bool(undone.has_value())));
    if (!undone) return;
    expect(fatal(!undone->structural));
    expect(fatal(!undone->forward));
    expect(fatal(bool(undone->revision_before > undone->revision_after)));

    expect(fatal(editor.redo()));
    auto redone = editor.take_last_document_change();
    expect(fatal(bool(redone.has_value())));
    if (!redone) return;
    expect(fatal(redone->forward));
    editor.set_selection(caret(first_text(editor), 2));
    expect(fatal(editor.execute_command(Command{.kind = CommandKind::InsertNewline})));
    auto split = editor.take_last_document_change();
    expect(fatal(bool(split.has_value())));
    expect(fatal(bool(split && split->structural)));
};

"footnote_insert_is_one_exact_source_and_tree_history_transaction"_test = [] {
    Editor editor("body");
    const auto before = range(first_text(editor), 0, 4);
    editor.set_selection(before);
    Command command;
    command.kind = CommandKind::InsertFootnote;
    reset_core_operation_counters();
    const auto transaction = editor.execute_document_insert_footnote(editor.selection(), command);
    expect(fatal(bool(transaction.has_value()))) << "transaction";
    if (!transaction) return;
    const auto counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_document_parses == 0u))) << "full parse";
    expect(fatal(bool(counters.full_document_serializations == 0u))) << "full serialize";
    expect(fatal(bool(counters.full_tree_transaction_diffs == 0u))) << "tree diff";
    expect(fatal(bool(counters.full_document_block_index_scans == 0u)))
        << "footnote insert full block index";
    expect(fatal(bool(counters.incremental_document_block_index_repairs == 1u)))
        << "footnote insert incremental block index";
    expect(fatal(bool(counters.full_document_symbol_derivations == 0u)))
        << "inserted definition and edited reference stay contribution-local";
    expect(fatal(bool(counters.inline_reparses == 2u))) << "edited owner plus new definition body";
    expect(fatal(bool(transaction->operations.size() == 2u))) << "operations";
    expect(fatal(document_indexes_are_exact(editor.document()))) << "insert indexes";
    expect(fatal(bool(editor.markdown_utf8() == "body[^1]\n\n[^1]: "))) << "insert markdown";
    expect(fatal(bool(editor.symbols().footnotes.size() == 1u))) << "definition symbols";
    expect(fatal(bool(editor.symbols().footnote_references.size() == 1u))) << "reference symbols";
    const auto after = editor.selection();
    expect(fatal(bool(after.active.container_id == editor.document().root.children.back().children.front().id))) << "definition selection";
    expect(fatal(bool(editor.undo()))) << "undo";
    expect(fatal(bool(editor.markdown_utf8() == "body"))) << "undo markdown";
    expect(fatal(bool(editor.selection() == before))) << "undo selection";
    expect(fatal(bool(editor.symbols().footnotes.empty()))) << "undo definition symbols";
    expect(fatal(bool(editor.symbols().footnote_references.empty()))) << "undo reference symbols";
    expect(fatal(document_indexes_are_exact(editor.document()))) << "undo indexes";
    expect(fatal(bool(editor.redo()))) << "redo";
    expect(fatal(bool(editor.markdown_utf8() == "body[^1]\n\n[^1]: "))) << "redo markdown";
    expect(fatal(bool(editor.selection() == after))) << "redo selection";
    expect(fatal(bool(editor.symbols().footnotes.size() == 1u))) << "redo definition symbols";
    expect(fatal(bool(editor.symbols().footnote_references.size() == 1u))) << "redo reference symbols";
    expect(fatal(document_indexes_are_exact(editor.document()))) << "redo indexes";
    expect(fatal(bool(validate_document(editor.document()).empty()))) << "redo document invariants";
};

"missing_footnote_definition_command_is_one_semantic_undoable_edit"_test = [] {
    Editor editor("body[^missing]");
    auto before = editor.selection();
    Command command;
    command.kind = CommandKind::CreateFootnoteDefinition;
    command.footnote_label = "missing";
    expect(fatal(editor.execute_command(command)));
    expect(editor.markdown_utf8() == "body[^missing]\n\n[^missing]: ");
    expect(fatal(editor.undo()));
    expect(editor.markdown_utf8() == "body[^missing]");
    expect(editor.selection() == before);
};

};
