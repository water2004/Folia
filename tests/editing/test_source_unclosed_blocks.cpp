#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.core.source_editor;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

suite source_unclosed_block_tests = [] {

"source newline inside fenced list prefixes retains the accepted blank line"_test = [] {
    const std::u32string before = U"- ```\n  code\n  ```";
    for (const auto offset : {7u, 8u, 14u, 15u}) {
        for (const auto ending : {U"\n", U"\r\n", U"\r"}) {
            auto expected = before;
            expected.insert(offset, ending);
            Editor reloaded(cps_to_utf8(expected));
            expect(fatal(reloaded.markdown_utf8() == cps_to_utf8(expected)));
            Editor editor(cps_to_utf8(before));
            expect(fatal(editor.enter_source_mode()));
            const auto selected = SourceSelection::caret(offset);
            editor.set_source_selection(selected);
            reset_core_operation_counters();
            expect(fatal(editor.execute_command(Command::InsertText(ending))));
            const auto counters = read_core_operation_counters();
            expect(counters.full_document_parses == 0u && counters.full_document_serializations == 0u);
            expect(editor.source_editor()->source() == expected);
            expect(validate_document(editor.document()).empty());
            expect(editor.undo());
            expect(editor.source_editor()->source() == before);
            expect(editor.source_editor()->selection() == selected);
            expect(editor.redo());
            expect(editor.exit_source_mode());
            expect(editor.markdown_utf8() == cps_to_utf8(expected));
        }
    }
};

"unclosed raw list blocks retain body newlines and prefix provenance"_test = [] {
    for (const auto marker : {U"```", U"$$", U"\\["}) {
        for (const auto ending : {std::u32string{U"\n"}, std::u32string{U"\r\n"}, std::u32string{U"\r"}}) {
            const auto source = U"- " + std::u32string(marker) + ending + U"  body" + ending + U" " + ending + U"outside";
            Editor editor(cps_to_utf8(source));
            expect(fatal(editor.markdown_utf8() == cps_to_utf8(source)));
            const BlockNode* raw = nullptr;
            walk_blocks(editor.document().root, [&](const BlockNode& block) {
                if (!raw && (block.kind == BlockKind::CodeBlock || block.kind == BlockKind::MathBlock)) raw = &block;
            });
            expect(fatal(raw != nullptr));
            expect(bool(raw->block_source.source() == std::u32string(marker) + ending + U"body" + ending + ending));
            expect(block_source_tokens_partition(raw->block_source));
            expect(bool(block_source_content(raw->block_source) == U"body" + ending + ending));
            expect(fatal(editor.enter_source_mode()));
            editor.set_source_selection(SourceSelection::caret(source.find(U"body") + 2));
            expect(editor.execute_command(Command::InsertText(U"X")));
            expect(editor.undo());
            expect(editor.source_editor()->source() == source);
        }
    }
};

};
