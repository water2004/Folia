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

suite editing_raw_blocks_render_history_tests = [] {

"command_pipeline_exposes_typed_and_toolbar_raw_blocks_to_rendering_immediately"_test = [] {
    struct TypedCase {
        std::u32string opening;
        BlockKind block_kind;
        RenderBlockKind render_kind;
    };
    for (const auto& test_case : std::vector<TypedCase>{
            {U"```cpp", BlockKind::CodeBlock, RenderBlockKind::Code},
            {U"$$", BlockKind::MathBlock, RenderBlockKind::Math},
        }) {
        Editor editor;
        expect(fatal(bool(editor.execute_command(Command::InsertText(test_case.opening)))));
        Command enter;
        enter.kind = CommandKind::InsertNewline;
        expect(fatal(bool(editor.execute_command(enter))));
        expect(fatal(bool(editor.document().root.children.front().kind == test_case.block_kind)));
        auto model = build_render_model(
            editor.document(), editor.outline(), editor.symbols(), default_theme_profile());
        expect(fatal(bool(model.revision == editor.revision())));
        expect(fatal(bool(model.blocks.size() == 1u)));
        expect(fatal(bool(model.blocks.front().kind == test_case.render_kind)));
    }

    for (const auto command_kind : {
            CommandKind::InsertCodeBlock,
            CommandKind::InsertMathBlock,
        }) {
        Editor editor;
        Command command;
        command.kind = command_kind;
        expect(fatal(bool(editor.execute_command(command))));
        auto model = build_render_model(
            editor.document(), editor.outline(), editor.symbols(), default_theme_profile());
        expect(fatal(bool(model.blocks.size() == 1u)));
        expect(fatal(bool(model.blocks.front().kind
            == (command_kind == CommandKind::InsertCodeBlock
                ? RenderBlockKind::Code
                : RenderBlockKind::Math))));
        expect(fatal(bool(model.blocks.front().id == editor.selection().active.container_id)));
    }
};

"code_and_math_blocks_use_local_source_edit_history"_test = [] {
    const std::vector<std::string> cases{
        "```cpp\nab\n```",
        "$$\nab\n$$",
    };
    for (const auto& markdown : cases) {
        Editor editor(markdown);
        const auto owner_id = editor.document().root.children.front().id;
        const auto content_offset = editor.document().root.children.front()
            .block_source.tree().content_to_source.front();
        const auto original_source = *document_editable_text(editor.document(), owner_id);
        const auto before_markdown = editor.markdown_utf8();
        const auto before_selection = TextSelection::caret(
            {owner_id, content_offset, TextAffinity::Downstream});
        editor.set_selection(before_selection);

        reset_core_operation_counters();
        auto inserted = editor.execute_document_insert_text(editor.selection(), U"X");
        expect(fatal(bool(inserted.has_value()))) << markdown;
        if (!inserted) continue;
        auto counters = read_core_operation_counters();
        expect(fatal(bool(counters.full_document_parses == 0u))) << markdown;
        expect(fatal(bool(counters.full_document_serializations == 0u))) << markdown;
        expect(fatal(bool(counters.full_tree_transaction_diffs == 0u))) << markdown;
        expect(fatal(bool(counters.inline_reparses == 0u))) << markdown;
        auto expected = original_source;
        expected.insert(content_offset, U"X");
        const auto inserted_expected = expected;
        expect(fatal(bool(document_editable_text(editor.document(), owner_id) == expected))) << markdown;
        const auto inserted_selection = editor.selection();

        expect(fatal(bool(editor.undo()))) << markdown;
        expect(fatal(bool(editor.markdown_utf8() == before_markdown))) << markdown;
        expect(fatal(bool(editor.selection() == before_selection))) << markdown;
        expect(fatal(bool(editor.redo()))) << markdown;
        expect(fatal(bool(document_editable_text(editor.document(), owner_id) == expected))) << markdown;
        expect(fatal(bool(editor.selection() == inserted_selection))) << markdown;

        reset_core_operation_counters();
        auto entered = editor.execute_document_enter(editor.selection());
        expect(fatal(bool(entered.has_value()))) << markdown;
        if (!entered) continue;
        counters = read_core_operation_counters();
        expect(fatal(bool(counters.full_document_parses == 0u))) << markdown;
        expect(fatal(bool(counters.full_document_serializations == 0u))) << markdown;
        expect(fatal(bool(counters.full_tree_transaction_diffs == 0u))) << markdown;
        expect(fatal(bool(counters.inline_reparses == 0u))) << markdown;
        expected.insert(content_offset + 1, U"\n");
        expect(fatal(bool(document_editable_text(editor.document(), owner_id) == expected))) << markdown;
        expect(fatal(bool(editor.undo()))) << markdown;
        expect(fatal(bool(document_editable_text(editor.document(), owner_id)
            == inserted_expected))) << markdown;

        reset_core_operation_counters();
        auto deleted = editor.execute_document_delete_backward(editor.selection());
        expect(fatal(bool(deleted.has_value()))) << markdown;
        if (!deleted) continue;
        counters = read_core_operation_counters();
        expect(fatal(bool(counters.full_document_parses == 0u))) << markdown;
        expect(fatal(bool(counters.full_document_serializations == 0u))) << markdown;
        expect(fatal(bool(counters.full_tree_transaction_diffs == 0u))) << markdown;
        expect(fatal(bool(counters.inline_reparses == 0u))) << markdown;
        expect(fatal(bool(document_editable_text(editor.document(), owner_id)
            == original_source))) << markdown;
        expect(fatal(bool(editor.undo()))) << markdown;
        expect(fatal(bool(document_editable_text(editor.document(), owner_id)
            == inserted_expected))) << markdown;
    }
};

};
