#include "support/folia_test.hpp"
import folia.core.command;
import folia.core.document_edit;
import folia.core.editor;
import folia.core.source_editor;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.instrumentation;
import folia.markdown.selection;
import folia.markdown.text_edit;

using namespace folia;
using namespace boost::ut;
#include "support/editor_test_support.hpp"

suite editor_interaction_sequences = [] {
"editing_sequence_modes_history_and_rejected_commands_preserve_model_invariants"_test = [] {
    const std::vector<std::string> seeds{
        "", "plain text", "![alt](image.png)", "***", "[TOC]",
        "---\ntitle: value\n---\n", "- one\n- two", "- [ ] one\n- [x] two",
        "> quote", "> [!NOTE]\n> body", "[^n]: note", "```cpp\nvalue\n```",
        "$$\nx+y\n$$", "| a | b |\n| --- | --- |\n| c | d |", "<p>html &amp; text</p>",
        "![alt](a.png)\n\ntext\n\n***"};
    const std::array<CommandKind, 20> kinds{
        CommandKind::InsertText, CommandKind::Paste, CommandKind::InsertNewline, CommandKind::InsertSoftBreak,
        CommandKind::DeleteBackward, CommandKind::DeleteForward, CommandKind::DeleteSelection,
        CommandKind::MoveLeft, CommandKind::MoveRight, CommandKind::MoveDocumentStart, CommandKind::MoveDocumentEnd,
        CommandKind::SelectAll, CommandKind::ToggleStrong, CommandKind::ToggleInlineCode,
        CommandKind::ToggleUnorderedList, CommandKind::ToggleOrderedList, CommandKind::ToggleBlockQuote,
        CommandKind::SetHeading, CommandKind::Undo, CommandKind::Redo};
    const std::array<std::u32string, 8> inserted{U"x", U"中", U" ", U"*", U"`", U"-", U"[", U"\n"};
    for (std::size_t seed = 0; seed < seeds.size(); ++seed)
    for (const auto start_in_source : {false, true}) {
        Editor editor(seeds[seed]);
        const auto initial = editor.markdown_utf8();
        if (start_in_source) expect(fatal(editor.enter_source_mode()));
        std::mt19937 random(static_cast<std::uint32_t>(0xF011A + 97 * seed + start_in_source));
        for (std::size_t step = 0; step < 48; ++step) {
            if (step % 11 == 10) {
                expect(fatal(editor.source_editor() ? editor.exit_source_mode() : editor.enter_source_mode()));
            }
            const auto before = editor.markdown_utf8();
            const auto revision = editor.revision();
            const auto source_mode = editor.source_editor() != nullptr;
            const auto selected_source = source_mode ? editor.source_editor()->selection() : SourceSelection{};
            const auto selected_rich = source_mode ? TextSelection{} : editor.selection();
            Command command;
            command.kind = kinds[random() % kinds.size()];
            command.text = inserted[random() % inserted.size()];
            command.level = 2;
            command.extend_selection = random() % 3 == 0;
            reset_core_operation_counters();
            const auto changed = editor.execute_command(command);
            const auto counts = read_core_operation_counters();
            expect(counts.full_document_parses == 0_u) << seed << step;
            expect(counts.full_document_serializations == 0_u) << seed << step;
            expect(counts.full_tree_transaction_diffs == 0_u) << seed << step;
            expect(fatal(validate_document(editor.document()).empty()))
                << seed << step << static_cast<int>(command.kind) << before;
            expect(fatal(document_indexes_are_exact(editor.document()))) << seed << step;
            walk_blocks(editor.document().root, [&](const BlockNode& block) {
                if (const auto* source = editable_inline_document(block))
                    expect(fatal(bool(flatten_tokens(source->tree, source->source) == source->source))) << seed << step;
            });
            const auto after = editor.markdown_utf8();
            if (!changed) {
                expect(fatal(after == before)) << seed << step << static_cast<int>(command.kind);
                expect(editor.revision() == revision);
                if (source_mode) expect(editor.source_editor()->selection() == selected_source);
                else expect(editor.selection() == selected_rich);
            }
            if (source_mode) {
                const auto selected = editor.source_editor()->selection();
                expect(selected.anchor <= editor.source_editor()->source().size());
                expect(selected.active <= editor.source_editor()->source().size());
            } else {
                for (auto position : {editor.selection().anchor, editor.selection().active}) {
                    auto length = document_edit_detail::editable_length(editor.document(), position.container_id);
                    expect(fatal(length.has_value())) << seed << step << static_cast<int>(command.kind);
                    expect(fatal(position.source_offset <= *length)) << seed << step << position.source_offset << *length;
                }
            }
            if (step % 8 == 0) {
                Editor reloaded(after);
                expect(fatal(reloaded.markdown_utf8() == after)) << seed << step << after;
            }
        }
        while (editor.has_undo()) expect(fatal(editor.undo())) << seed;
        expect(fatal(editor.markdown_utf8() == initial)) << seed << start_in_source;
        expect(validate_document(editor.document()).empty());
    }
};
};
