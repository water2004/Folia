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

suite editing_raw_blocks_code_language_tests = [] {

"code_language_command_edits_only_the_first_info_token"_test = [] {
    Editor editor("```cpp linenums\nvalue\n```");
    const auto code_id = editor.document().root.children.front().id;
    const auto content_start = block_source_editable_range(
        editor.document().root.children.front().block_source).start;
    editor.set_selection(TextSelection::caret({
        code_id, content_start, TextAffinity::Downstream}));
    const auto before_selection = editor.selection();

    Command command;
    command.kind = CommandKind::SetCodeBlockLanguage;
    command.lang = U"javascript";
    expect(fatal(editor.execute_command(command)));
    auto const* code = find_block(editor.document().root, code_id);
    expect(fatal(bool(code)));
    if (!code) return;
    expect(fatal(code->block_source.source()
        == U"```javascript linenums\nvalue\n```"));
    expect(fatal(code->block_source.tree().language
        == std::optional<std::string>{"javascript"}));
    expect(fatal(code->block_source.source().substr(
        code->block_source.tree().info_range->start,
        code->block_source.tree().info_range->length())
        == U"javascript linenums"));
    expect(fatal(editor.selection().active.source_offset
        == before_selection.active.source_offset + 7u));

    expect(fatal(editor.undo()));
    code = find_block(editor.document().root, code_id);
    expect(fatal(bool(code && code->block_source.source()
        == U"```cpp linenums\nvalue\n```")));
    expect(fatal(editor.selection() == before_selection));
    expect(fatal(editor.redo()));
    code = find_block(editor.document().root, code_id);
    expect(fatal(bool(code && code->block_source.tree().language
        == std::optional<std::string>{"javascript"})));
};

"code_language_command_preserves_fence_spelling_and_mermaid_semantics"_test = [] {
    Editor editor("~~~cpp title=diagram\ngraph TD; A-->B\n~~~");
    const auto code_id = editor.document().root.children.front().id;
    const auto visible = block_source_editable_range(
        editor.document().root.children.front().block_source);
    editor.set_selection(TextSelection::caret({
        code_id, visible.start, TextAffinity::Downstream}));
    const auto before = editor.selection();

    Command set_mermaid;
    set_mermaid.kind = CommandKind::SetCodeBlockLanguage;
    set_mermaid.lang = U"mermaid";
    expect(fatal(editor.execute_command(set_mermaid)));
    auto const* code = find_block(editor.document().root, code_id);
    expect(fatal(bool(code)));
    if (!code) return;
    expect(fatal(code->block_source.source()
        == U"~~~mermaid title=diagram\ngraph TD; A-->B\n~~~"));
    expect(fatal(code->block_source.tree().language
        == std::optional<std::string>{"mermaid"}));
    expect(fatal(code->block_source.source().starts_with(U"~~~")));
    expect(fatal(code->block_source.source().ends_with(U"~~~")));

    auto rendered = build_render_model(
        editor.document(), editor.outline(), editor.symbols(), default_theme_profile());
    expect(fatal(rendered.blocks.size() == 1u));
    expect(fatal(rendered.blocks.front().kind == RenderBlockKind::Code));
    expect(fatal(rendered.blocks.front().special().language
        == std::optional<std::string>{"mermaid"}));

    expect(fatal(editor.undo()));
    expect(fatal(editor.selection() == before));
    code = find_block(editor.document().root, code_id);
    expect(fatal(bool(code && code->block_source.source()
        == U"~~~cpp title=diagram\ngraph TD; A-->B\n~~~")));
    expect(fatal(editor.redo()));
    code = find_block(editor.document().root, code_id);
    expect(fatal(bool(code && code->block_source.tree().language
        == std::optional<std::string>{"mermaid"})));
};

};
