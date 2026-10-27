#include <unordered_map>
#include <unordered_set>
#include <string>
#include <string_view>
#include <iterator>
#include <vector>

#include "support/folia_test.hpp"

import folia.markdown.ast;
import folia.markdown.document;
import folia.core.document_edit;
import folia.core.document_format;
import folia.core.document_history;
import folia.markdown.dialect;
import folia.core.editor;
import folia.markdown.instrumentation;
import folia.markdown.parser;
import folia.markdown.selection;
import folia.markdown.serializer;
import folia.markdown.text_edit;

import folia.markdown.block_source;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.utf;
#include "support/commonmark_projection.hpp"

using namespace folia;
using namespace boost::ut;

namespace {

BlockNode const* find_kind(BlockNode const& block, BlockKind kind) {
    if (block.kind == kind) return &block;
    for (auto const& child : block.children) {
        if (auto const* found = find_kind(child, kind)) return found;
    }
    return nullptr;
}

} // namespace

suite gfm_table_source_tests = [] {
    auto dialect = folia::test::commonmark::gfm_dialect();

    "gfm_table_rows_continue_without_pipes_and_stop_at_blocks"_test = [dialect] {
        for (const auto& suffix : {std::string("\ntext\n"), std::string("> quote\n"),
                std::string("2. item\n"), std::string("-\n"), std::string("# heading\n")}) {
            const auto source = std::string("| a | b |\n| - | - |\nplain\n") + suffix;
            const auto parsed = parse_text(130, source, dialect);
            const auto* table = find_kind(parsed.document.root, BlockKind::Table);
            expect(fatal(table != nullptr));
            expect(table->children.size() == 2_u);
            expect(serialize_markdown(parsed.document) == source);
        }
        const std::string source = "first\nsecond\n| a | b |\n| - | - |\nrow\n";
        const auto parsed = parse_text(131, source, dialect);
        expect(parsed.document.root.children.size() == 2_u);
        expect(find_kind(parsed.document.root, BlockKind::Table) != nullptr);
        expect(serialize_markdown(parsed.document) == source);
    };

    "gfm_table_source_roundtrips_official_save_failures"_test = [dialect] {
        constexpr std::string_view cases[] = {
            "| abc | defghi |\n:-: | -----------:\nbar | baz\n",
            "| f\\|oo  |\n| ------ |\n| b `\\|` az |\n| b **\\|** im |\n",
            "| abc | def |\n| --- | --- |\n| bar |\n| bar | baz | boo |\n",
        };
        for (std::size_t index = 0; index < std::size(cases); ++index) {
            auto parsed = parse_text(100 + index, std::string(cases[index]), dialect);
            expect(serialize_markdown(parsed.document) == cases[index]);
        }
    };

    "gfm_table_source_roundtrips_nested_prefixes_tabs_and_crlf"_test = [dialect] {
        constexpr std::string_view cases[] = {
            "> | a\t | b |\r\n> | --- | --- |\r\n> x  | y\t\r\n",
            "- | a\t | b |\n  | --- | --- |\n  x  | y\t\n",
        };
        for (std::size_t index = 0; index < std::size(cases); ++index) {
            auto parsed = parse_text(110 + index, std::string(cases[index]), dialect);
            expect(serialize_markdown(parsed.document) == cases[index]);
        }
    };

    "gfm_table_cell_edit_history_keeps_padding_without_full_document_work"_test = [dialect] {
        constexpr std::string_view source = "| head  |\r\n| ----- |\r\n| b `\\|` az  |\r\n";
        auto parsed = parse_text(120, std::string(source), dialect);
        auto const* table = find_kind(parsed.document.root, BlockKind::Table);
        expect(table != nullptr);
        if (!table || table->children.size() < 2 || table->children[1].children.empty()) return;
        auto const* cell = &table->children[1].children[0];
        const auto old = serialize_markdown(parsed.document);
        reset_core_operation_counters();
        auto transaction = document_insert_text(parsed.document,
            TextSelection::caret({cell->id, cell->inline_content.source.size(), TextAffinity::Downstream}), U"!");
        expect(transaction.has_value());
        if (!transaction) return;
        auto counters = read_core_operation_counters();
        expect(counters.full_document_parses == 0u);
        expect(counters.full_document_serializations == 0u);
        expect(serialize_markdown(parsed.document).find("| b `\\|` az!  |") != std::string::npos);

        DocumentHistory history;
        auto selection = transaction->selection_after;
        history.push(*transaction);
        reset_core_operation_counters();
        expect(history.undo(parsed.document, selection));
        counters = read_core_operation_counters();
        expect(counters.full_document_parses == 0u);
        expect(counters.full_document_serializations == 0u);
        expect(serialize_markdown(parsed.document) == old);
        reset_core_operation_counters();
        expect(history.redo(parsed.document, selection));
        counters = read_core_operation_counters();
        expect(counters.full_document_parses == 0u);
        expect(counters.full_document_serializations == 0u);
        expect(serialize_markdown(parsed.document).find("| b `\\|` az!  |") != std::string::npos);
    };

    "gfm_table_empty_column_insert_undo_regenerates_changed_rows"_test = [dialect] {
        constexpr std::string_view source = "| head | second |\n| --- | --- |\n| one |\n";
        auto parsed = parse_text(121, std::string(source), dialect);
        auto const* table = find_kind(parsed.document.root, BlockKind::Table);
        expect(table != nullptr);
        if (!table || table->children.size() < 2) return;
        const auto before = serialize_markdown(parsed.document);
        const auto cell_id = table->children[1].children.back().id;
        auto transaction = document_edit_table(parsed.document,
            TextSelection::caret({cell_id, 0, TextAffinity::Downstream}),
            DocumentTableEdit::InsertColumnRight);
        expect(transaction.has_value());
        if (!transaction) return;
        expect(serialize_markdown(parsed.document) != before);
        DocumentHistory history;
        auto selection = transaction->selection_after;
        history.push(*transaction);
        expect(history.undo(parsed.document, selection));
        expect(serialize_markdown(parsed.document) == before);
        expect(history.redo(parsed.document, selection));
        expect(serialize_markdown(parsed.document) != before);
    };

    "gfm_table_row_and_column_undo_restore_exact_source"_test = [dialect] {
        constexpr std::string_view source = "| a  | b |\n| --- | --- |\n| c  | d\t|\n| e | f |\n";
        auto parsed = parse_text(122, std::string(source), dialect);
        auto const* table = find_kind(parsed.document.root, BlockKind::Table);
        expect(table != nullptr);
        if (!table || table->children.size() < 3) return;
        const auto before = serialize_markdown(parsed.document);
        const auto first_data_cell = table->children[1].children[0].id;
        const auto second_data_cell = table->children[1].children[1].id;
        auto run_and_undo = [&](NodeId cell_id, DocumentTableEdit edit) {
            auto transaction = document_edit_table(parsed.document,
                TextSelection::caret({cell_id, 0, TextAffinity::Downstream}), edit);
            expect(transaction.has_value());
            if (!transaction) return;
            DocumentHistory history;
            auto selection = transaction->selection_after;
            history.push(*transaction);
            expect(history.undo(parsed.document, selection));
            expect(serialize_markdown(parsed.document) == before);
        };
        run_and_undo(first_data_cell, DocumentTableEdit::InsertRowBelow);
        run_and_undo(first_data_cell, DocumentTableEdit::DeleteRow);
        run_and_undo(second_data_cell, DocumentTableEdit::DeleteColumn);
    };

    "gfm_table_canonical_format_clears_row_source_and_undo_restores_it"_test = [dialect] {
        constexpr std::string_view source = "|a  | b|\n|---|---|\n| c  |d |";
        auto parsed = parse_text(123, std::string(source), dialect);
        auto const* table = find_kind(parsed.document.root, BlockKind::Table);
        expect(table != nullptr);
        if (!table || table->children.empty() || table->children.front().children.empty()) return;
        const auto cell_id = table->children.front().children.front().id;
        const auto before = serialize_markdown(parsed.document);
        auto transaction = document_format_markdown(parsed.document,
            TextSelection::caret({cell_id, 0, TextAffinity::Downstream}));
        expect(transaction.has_value());
        if (!transaction) return;
        expect(serialize_markdown(parsed.document) != before);
        auto const* formatted_table = find_kind(parsed.document.root, BlockKind::Table);
        expect(formatted_table != nullptr);
        if (!formatted_table) return;
        for (const auto& row : formatted_table->children)
            expect(!row.table_special().row_syntax.has_value());
        DocumentHistory history;
        auto selection = transaction->selection_after;
        history.push(*transaction);
        expect(history.undo(parsed.document, selection));
        expect(serialize_markdown(parsed.document) == before);
        auto const* restored_table = find_kind(parsed.document.root, BlockKind::Table);
        expect(restored_table != nullptr);
        if (restored_table)
            for (const auto& row : restored_table->children)
                expect(row.table_special().row_syntax.has_value());
    };

    "gfm_table_source_owner_ids_remap_during_fresh_id_paste"_test = [dialect] {
        constexpr std::string_view source = "| a  | b |\n| --- | --- |\n| c | d |";
        Editor editor("", dialect);
        const auto transaction = editor.execute_document_paste_text(
            editor.selection(), std::u32string(source.begin(), source.end()));
        expect(transaction.has_value());
        if (!transaction) return;
        expect(editor.markdown_utf8() == source);
        auto const* table = find_kind(editor.document().root, BlockKind::Table);
        expect(table != nullptr);
        if (!table) return;
        for (const auto& row : table->children) {
            const auto& syntax = row.table_special().row_syntax;
            expect(syntax.has_value());
            if (!syntax) continue;
            expect(syntax->cell_owners.size() == row.children.size());
            for (std::size_t index = 0; index < syntax->cell_owners.size(); ++index)
                expect(syntax->cell_owners[index] == row.children[index].id);
        }
    };
};
