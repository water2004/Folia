#include <unordered_map>
#include <unordered_set>
#include <string>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.dialect;
import folia.markdown.inline_document;
import folia.markdown.parser;
import folia.core.render_builder;
import folia.core.render_model;
import folia.markdown.serializer;
import folia.core.theme;
import folia.markdown.utf;

import folia.markdown.block_source;
import folia.markdown.inline_cst;
#include "support/commonmark_projection.hpp"

using namespace folia;
using namespace boost::ut;

suite gfm_table_code_span_tests = [] {

"table_code_span_pipe_is_visible_but_source_mapped_and_saved_losslessly"_test = [] {
    auto dialect = folia::test::commonmark::gfm_dialect();
    const std::string markdown = "| h |\n| --- |\n| `\\|` |\n";
    const auto parsed = parse_text(1, markdown, dialect);
    const auto& table = parsed.document.root.children.front();
    const auto& cell = table.children[1].children.front();

    expect(fatal(cell.kind == BlockKind::TableCell));
    expect(fatal(cell.inline_content.syntax_mode == InlineSyntaxMode::TableCell));
    expect(fatal(cell.inline_content.source == U"`\\|`"));
    expect(fatal(flatten_tokens(cell.inline_content.tree, cell.inline_content.source)
        == cell.inline_content.source));
    expect(fatal(serialize_markdown(parsed.document) == markdown));
    expect(fatal(inline_visible_text(cell.inline_content) == U"|"));

    const auto projection = folia::test::commonmark::project(parsed.document.root);
    expect(fatal(projection.projected_html.find("<code>|</code>") != std::string::npos));

    const auto model = build_render_model(
        parsed.document, parsed.outline, parsed.symbols, default_theme_profile());
    const auto& items = model.blocks.front().special().table_cells[1];
    bool has_visible_pipe = false;
    bool has_hidden_escape = false;
    for (const auto& item : items) {
        if (item.kind == InlineRenderItem::Kind::Text && item.text == U"|") {
            has_visible_pipe = true;
            expect(fatal(item.source_span.source_range == SourceRange{2, 3}))
                << "pipe mapped to [" << item.source_span.source_range.start
                << ", " << item.source_span.source_range.end << ")"
                << " owner=" << item.source_span.container_id.v;
        }
        if (item.kind == InlineRenderItem::Kind::Marker
            && item.source_span.source_range == SourceRange{1, 2}) {
            has_hidden_escape = true;
            expect(fatal(item.special().visibility == MarkerVisibility::HiddenButEditable))
                << "escape visibility=" << static_cast<int>(item.special().visibility)
                << " range=[" << item.source_span.source_range.start
                << ", " << item.source_span.source_range.end << ")";
        }
    }
    expect(fatal(has_visible_pipe));
    expect(fatal(has_hidden_escape));

    const auto paragraph = parse_text(2, "`\\|`");
    expect(fatal(inline_visible_text(paragraph.document.root.children.front().inline_content)
        == U"\\|"));
};

};
