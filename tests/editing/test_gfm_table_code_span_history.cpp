#include <string>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.dialect;
import folia.markdown.document;
import folia.core.editor;
import folia.markdown.inline_document;
import folia.markdown.selection;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

namespace {

MarkdownDialect gfm_table_dialect() {
    auto dialect = default_dialect();
    dialect.gfm = {true, true, true, true};
    dialect.tables = {true, true};
    return dialect;
}

}

suite gfm_table_code_span_history_tests = [] {

"table_cell_code_pipe_semantics_survive_local_edit_undo_and_redo"_test = [] {
    const std::string markdown = "| h |\n| --- |\n| `\\|` |\n";
    Editor editor(markdown, gfm_table_dialect());
    const auto& table = editor.document().root.children.front();
    const auto& cell = table.children[1].children.front();
    const auto cell_id = cell.id;
    const auto before = TextSelection::caret({cell_id, 3, TextAffinity::Downstream});
    editor.set_selection(before);

    expect(fatal(editor.execute_document_insert_text(editor.selection(), U"|").has_value()));
    const auto after = editor.selection();
    const auto* edited = find_document_block(editor.document(), cell_id);
    expect(fatal(edited != nullptr));
    if (!edited) return;
    expect(fatal(edited->inline_content.syntax_mode == InlineSyntaxMode::TableCell));
    expect(fatal(edited->inline_content.source == U"`\\||`"));
    expect(fatal(inline_visible_text(edited->inline_content) == U"||"));
    expect(fatal(flatten_tokens(edited->inline_content.tree, edited->inline_content.source)
        == edited->inline_content.source));
    const std::string edited_markdown = "| h |\n| --- |\n| `\\||` |\n";
    expect(fatal(editor.markdown_utf8() == edited_markdown));

    expect(fatal(editor.undo()));
    expect(fatal(editor.selection() == before));
    expect(fatal(editor.markdown_utf8() == markdown));
    const auto* undone = find_document_block(editor.document(), cell_id);
    expect(fatal(undone && undone->inline_content.source == U"`\\|`"));
    expect(fatal(undone && undone->inline_content.syntax_mode == InlineSyntaxMode::TableCell));
    expect(fatal(undone && inline_visible_text(undone->inline_content) == U"|"));

    expect(fatal(editor.redo()));
    expect(fatal(editor.selection() == after));
    expect(fatal(editor.markdown_utf8() == edited_markdown));
    const auto* redone = find_document_block(editor.document(), cell_id);
    expect(fatal(redone && redone->inline_content.source == U"`\\||`"));
    expect(fatal(redone && redone->inline_content.syntax_mode == InlineSyntaxMode::TableCell));
    expect(fatal(redone && inline_visible_text(redone->inline_content) == U"||"));
};

};
