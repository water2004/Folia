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

suite editing_raw_blocks_creation_tests = [] {

"raw_block_creation_owns_a_real_empty_body_before_first_input"_test = [] {
    struct Case { std::u32string opening; std::u32string closing; BlockKind kind; };
    for (const auto& t : std::vector<Case>{
            {U"$$", U"$$", BlockKind::MathBlock},
            {U"\\[", U"\\]", BlockKind::MathBlock},
            {U"```math", U"```", BlockKind::MathBlock},
            {U"```cpp", U"```", BlockKind::CodeBlock},
            {U"~~~cpp", U"~~~", BlockKind::CodeBlock},
        }) {
        Editor editor;
        expect(fatal(bool(editor.execute_command(Command::InsertText(t.opening)))));
        const auto before = editor.markdown_utf8();
        const auto before_selection = editor.selection();
        reset_core_operation_counters();
        expect(fatal(bool(editor.execute_command({.kind = CommandKind::InsertNewline}))));
        const auto counters = read_core_operation_counters();
        expect(counters.full_document_parses == 0_u && counters.full_document_serializations == 0_u);
        const auto id = editor.selection().active.container_id;
        const auto* block = find_block(editor.document().root, id);
        expect(fatal(block != nullptr));
        expect(block->kind == t.kind);
        const auto empty_source = t.opening + U"\n\n" + t.closing;
        expect(block->block_source.source() == empty_source) << "created " << cps_to_utf8(t.opening)
            << " actual=" << cps_to_utf8(block->block_source.source());
        const auto at = t.opening.size() + 1;
        expect(editor.selection().active.source_offset == at);
        expect(block->block_source.source()[at] == U'\n');
        expect(block->block_source.tree().content.empty());
        expect(std::ranges::equal(block->block_source.tree().content_to_source, std::vector<std::size_t>{at}));
        const auto source_kind = block->block_source.tree().kind;
        const auto opened_selection = editor.selection();
        const auto saved = editor.markdown_utf8();
        Editor reloaded(saved);
        expect(reloaded.document().root.children.front().block_source.source() == empty_source);
        expect(fatal(editor.undo()));
        expect(editor.markdown_utf8() == before && editor.selection() == before_selection);
        expect(fatal(editor.redo()));
        expect(editor.markdown_utf8() == saved && editor.selection() == opened_selection);
        expect(find_block(editor.document().root, id)->block_source.tree().kind == source_kind);
        for (auto text : {U"x", U"()", U"[{}]", U"α😀"}) {
            expect(fatal(bool(editor.execute_command(Command::InsertText(text)))));
            block = find_block(editor.document().root, id);
            expect(block->block_source.source() == t.opening + U"\n" + text + U"\n" + t.closing)
                << "insert " << cps_to_utf8(t.opening) << " actual=" << cps_to_utf8(block->block_source.source());
            expect(block->block_source.tree().content == text) << "content " << cps_to_utf8(t.opening);
            expect(editor.selection().active.source_offset == at + std::u32string_view{text}.size())
                << "caret " << cps_to_utf8(t.opening) << " at=" << editor.selection().active.source_offset;
            expect(fatal(editor.undo()));
            expect(find_block(editor.document().root, id)->block_source.source() == empty_source) << "undo " << cps_to_utf8(t.opening);
            expect(editor.selection() == opened_selection) << "undo selection " << cps_to_utf8(t.opening);
        }
    }
};

"task_raw_block_continuations_exclude_checkbox_width_and_reload_exactly"_test = [] {
    for (auto marker : {U"- [ ] ", U"+ [X] ", U"12. [x] "})
    for (auto opening : {U"$$", U"\\[", U"```math", U"```cpp"}) {
        Editor editor;
        for (auto c : std::u32string_view{marker})
            expect(fatal(editor.execute_command(Command::InsertText(std::u32string(1, c)))));
        expect(fatal(editor.execute_command(Command::InsertText(opening))));
        const auto before = editor.markdown_utf8();
        const auto before_selection = editor.selection();
        reset_core_operation_counters();
        expect(fatal(editor.execute_command({.kind = CommandKind::InsertNewline})));
        expect(fatal(editor.execute_command(Command::InsertText(U"x"))));
        auto counters = read_core_operation_counters();
        expect(counters.full_document_parses == 0_u && counters.full_document_serializations == 0_u);
        const auto closing = std::u32string_view{opening} == U"$$" ? U"$$"
            : std::u32string_view{opening} == U"\\[" ? U"\\]" : U"```";
        const auto body = std::u32string{opening} + U"\nx\n" + closing;
        auto prefix = std::u32string(std::u32string_view{marker}.starts_with(U"12.") ? 4 : 2, U' ');
        const auto saved = editor.markdown_utf8();
        expect(saved == cps_to_utf8(std::u32string{marker} + opening + U"\n" + prefix + U"x\n" + prefix + closing));
        Editor reloaded(saved);
        const BlockNode* raw = nullptr;
        walk_blocks(reloaded.document().root, [&](const BlockNode& block) {
            if (block.kind == BlockKind::MathBlock || block.kind == BlockKind::CodeBlock) raw = &block;
        });
        expect(fatal(raw != nullptr)) << saved;
        expect(raw->block_source.source() == body) << saved;
        expect(fatal(editor.undo())); expect(fatal(editor.undo()));
        expect(editor.markdown_utf8() == before && editor.selection() == before_selection);
        expect(fatal(editor.redo())); expect(fatal(editor.redo()));
        expect(editor.markdown_utf8() == saved);
    }
};

"typed_fences_create_auto_closed_block_source_transactions"_test = [] {
    Editor editor;
    expect(fatal(bool(editor.execute_document_insert_text(
        editor.selection(), U"```cpp").has_value())));
    const auto opening_before_markdown = editor.markdown_utf8();
    const auto opening_before = editor.selection();
    reset_core_operation_counters();
    auto opened = editor.execute_document_enter(editor.selection());
    expect(fatal(bool(opened.has_value())));
    if (!opened) return;
    auto counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_document_parses == 0u)));
    expect(fatal(bool(counters.full_document_serializations == 0u)));
    expect(fatal(bool(counters.full_tree_transaction_diffs == 0u)));
    expect(fatal(bool(editor.document().root.children.front().kind == BlockKind::CodeBlock)));
    expect(fatal(bool(editor.document().root.children.front().block_source.source() == U"```cpp\n\n```")));
    expect(fatal(bool(editor.document().root.children.front().block_source.tree().language
        == std::optional<std::string>{"cpp"})));
    expect(fatal(bool(editor.selection().active.source_offset == 7u)));
    const auto opened_markdown = editor.markdown_utf8();
    const auto opened_selection = editor.selection();

    expect(fatal(bool(editor.undo())));
    expect(fatal(bool(editor.markdown_utf8() == opening_before_markdown)));
    expect(fatal(bool(editor.selection() == opening_before)));
    expect(fatal(bool(editor.redo())));
    expect(fatal(bool(editor.markdown_utf8() == opened_markdown)));
    expect(fatal(bool(editor.selection() == opened_selection)));

    reset_core_operation_counters();
    auto exited = editor.execute_document_enter(editor.selection());
    expect(fatal(bool(exited.has_value())));
    if (!exited) return;
    counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_document_parses == 0u)));
    expect(fatal(bool(counters.full_document_serializations == 0u)));
    expect(fatal(bool(counters.full_tree_transaction_diffs == 0u)));
    expect(fatal(bool(editor.document().root.children.size() == 2u)));
    expect(fatal(bool(editor.document().root.children.front().block_source.source() == U"```cpp\n\n```")));
    expect(fatal(bool(editor.document().root.children.back().kind == BlockKind::Paragraph)));
    expect(fatal(bool(editor.selection().active.container_id
        == editor.document().root.children.back().id)));
    expect(fatal(bool(editor.selection().active.source_offset == 0u)));
    const auto exited_markdown = editor.markdown_utf8();
    const auto exited_selection = editor.selection();
    expect(fatal(bool(editor.undo())));
    expect(fatal(bool(editor.markdown_utf8() == opened_markdown)));
    expect(fatal(bool(editor.selection() == opened_selection)));
    expect(fatal(bool(editor.redo())));
    expect(fatal(bool(editor.markdown_utf8() == exited_markdown)));
    expect(fatal(bool(editor.selection() == exited_selection)));
};

"typed_tilde_fence_auto_closes_and_round_trips_history"_test = [] {
    Editor editor;
    expect(fatal(bool(editor.execute_document_insert_text(
        editor.selection(), U"~~~cpp").has_value())));
    const auto before_markdown = editor.markdown_utf8();
    const auto before_selection = editor.selection();

    reset_core_operation_counters();
    expect(fatal(bool(editor.execute_document_enter(editor.selection()).has_value())));
    auto counters = read_core_operation_counters();
    expect(fatal(bool(counters.full_document_parses == 0u)));
    expect(fatal(bool(counters.full_document_serializations == 0u)));
    expect(fatal(bool(counters.full_tree_transaction_diffs == 0u)));
    expect(fatal(bool(editor.document().root.children.size() == 1u)));
    if (editor.document().root.children.size() != 1u) return;
    const auto& code = editor.document().root.children.front();
    expect(fatal(bool(code.kind == BlockKind::CodeBlock)));
    expect(fatal(bool(code.block_source.source() == U"~~~cpp\n\n~~~")));
    expect(fatal(bool(code.block_source.tree().language == std::optional<std::string>{"cpp"})));
    expect(fatal(bool(editor.selection().active.source_offset == 7u)));

    const auto after_markdown = editor.markdown_utf8();
    const auto after_selection = editor.selection();
    expect(fatal(bool(editor.undo())));
    expect(fatal(bool(editor.markdown_utf8() == before_markdown)));
    expect(fatal(bool(editor.selection() == before_selection)));
    expect(fatal(bool(editor.redo())));
    expect(fatal(bool(editor.markdown_utf8() == after_markdown)));
    expect(fatal(bool(editor.selection() == after_selection)));
};

"toolbar_blocks_share_complete_source_construction_with_typed_rules"_test = [] {
    Editor code;
    Command insert_code;
    insert_code.kind = CommandKind::InsertCodeBlock;
    insert_code.lang = U"cpp";
    auto code_transaction = code.execute_document_insert_atomic_block(code.selection(), insert_code);
    expect(fatal(bool(code_transaction.has_value())));
    if (code_transaction) {
        expect(fatal(bool(code.document().root.children.size() == 1u)));
        auto const* block = find_block(code.document().root, code.selection().active.container_id);
        expect(fatal(bool(block && block->kind == BlockKind::CodeBlock)));
        expect(fatal(bool(block && block->block_source.source() == U"```cpp\n\n```")));
        expect(fatal(bool(code.selection().active.source_offset == 7u)));
        expect(fatal(bool(code.execute_document_enter(code.selection()).has_value())));
        expect(fatal(bool(code.document().root.children.size() == 2u)));
        expect(fatal(bool(code.document().root.children.front().block_source.source()
            == U"```cpp\n\n```")));
        expect(fatal(bool(code.document().root.children.back().kind == BlockKind::Paragraph)));
    }

    Editor math;
    Command insert_math;
    insert_math.kind = CommandKind::InsertMathBlock;
    auto math_transaction = math.execute_document_insert_atomic_block(math.selection(), insert_math);
    expect(fatal(bool(math_transaction.has_value())));
    if (math_transaction) {
        expect(fatal(bool(math.document().root.children.size() == 1u)));
        auto const* block = find_block(math.document().root, math.selection().active.container_id);
        expect(fatal(bool(block && block->kind == BlockKind::MathBlock)));
        expect(fatal(bool(block && block->block_source.source() == U"$$\n\n$$")));
        expect(fatal(bool(math.selection().active.source_offset == 3u)));
        expect(fatal(bool(math.execute_document_enter(math.selection()).has_value())));
        expect(fatal(bool(math.document().root.children.size() == 2u)));
        expect(fatal(bool(math.document().root.children.front().block_source.source()
            == U"$$\n\n$$")));
        expect(fatal(bool(math.document().root.children.back().kind == BlockKind::Paragraph)));
    }

    Editor heading("title");
    expect(fatal(bool(heading.execute_document_set_heading(heading.selection(), 1).has_value())));
    expect(fatal(bool(heading.document().root.children.front().text_special().opening_marker == U"# ")));
    expect(fatal(bool(heading.markdown_utf8() == "# title")));
};

"typed_math_delimiter_creates_editable_auto_closed_block"_test = [] {
    Editor editor;
    expect(fatal(bool(editor.execute_document_insert_text(editor.selection(), U"$$").has_value())));
    expect(fatal(bool(editor.execute_document_enter(editor.selection()).has_value())));
    auto const* block = find_block(editor.document().root, editor.selection().active.container_id);
    expect(fatal(bool(block && block->kind == BlockKind::MathBlock)));
    expect(fatal(bool(block && block->block_source.source() == U"$$\n\n$$")));
    expect(fatal(bool(editor.selection().active.source_offset == 3u)));
    expect(fatal(bool(editor.execute_document_enter(editor.selection()).has_value())));
    block = find_block(editor.document().root, editor.selection().active.container_id);
    expect(fatal(bool(editor.document().root.children.size() == 2u)));
    expect(fatal(bool(block && block->kind == BlockKind::Paragraph)));
    expect(fatal(bool(editor.document().root.children.front().block_source.source()
        == U"$$\n\n$$")));
    expect(fatal(bool(editor.selection().active.source_offset == 0u)));
};

};
