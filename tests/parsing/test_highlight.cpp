#include <string>
#include <string_view>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document_text;
import folia.markdown.dialect;
import folia.markdown.ids;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.markdown.instrumentation;
import folia.markdown.parser;
import folia.markdown.serializer;
import folia.markdown.text_edit;
import folia.markdown.utf;
import folia.core.inline_source_edit;

using namespace folia;
using namespace boost::ut;

#include "support/parser_test_support.hpp"

namespace {
InlineDocument highlight_document(std::u32string_view source, InlineParseContext context = {}) {
    return {std::u32string(source), parse_inline(source, context), context.syntax_mode};
}
}

suite highlight_tests = [] {
    "highlight markers preserve scalar ranges and inline source"_test = [] {
        const auto document = highlight_document(U"前 ==重点🙂== 后");
        expect_lossless(document);
        const auto* highlight = first_inline(document, InlineCstKind::Highlight);
        expect(fatal(highlight != nullptr));
        expect(highlight->status == ParseStatus::Complete);
        expect(highlight->range == SourceRange{2, 9});
        expect(highlight->delimiter_ranges().opening == SourceRange{2, 4});
        expect(highlight->delimiter_ranges().content == SourceRange{4, 7});
        expect(highlight->delimiter_ranges().closing == std::optional<SourceRange>{{7, 9}});
        expect(bool(inline_visible_text(document) == U"前 重点🙂 后"));
    };

    "highlight supports nested Markdown and independent link labels"_test = [] {
        for (const auto& source : {
            U"==**bold** and *italic* and ~~removed~~==",
            U"**==highlight==** then *after*",
            U"[a ==b== [c](d)](e)", U"*==a*b==c==",
            U"==outer ==inner== outer==",
            U"==[==link==](url) and ![==alt==](image.png)==",
            U"==line one\nline two==",
            U"before==inside==after",
        }) {
            const auto document = highlight_document(source);
            expect_lossless(document);
            expect(first_inline(document, InlineCstKind::Highlight) != nullptr) << cps_to_utf8(source);
        }
        const auto nested = highlight_document(U"==**bold** and *italic* and ~~removed~~==");
        expect(first_inline(nested, InlineCstKind::Strong) != nullptr);
        expect(first_inline(nested, InlineCstKind::Emphasis) != nullptr);
        expect(first_inline(nested, InlineCstKind::Strikethrough) != nullptr);
        expect(bool(inline_visible_text(nested) == U"bold and italic and removed"));
        const auto after = highlight_document(U"==mark== **after**");
        expect(first_inline(after, InlineCstKind::Strong) != nullptr);
        const auto crossing = highlight_document(U"*==a*b==c==");
        const auto* final_highlight = first_inline(crossing, InlineCstKind::Highlight);
        expect(fatal(final_highlight != nullptr));
        expect(bool(inline_source_slice(crossing, final_highlight->delimiter_ranges().content) == U"c"));
        const auto nested_links = highlight_document(U"[a ==b== [c](d)](e)");
        expect(first_inline(nested_links, InlineCstKind::Link) != nullptr);
        const auto image = highlight_document(U"![==**alt**==](image.png)");
        const auto* image_node = first_inline(image, InlineCstKind::Image);
        expect(fatal(image_node != nullptr));
        expect(image_node->semantics().alt == "alt");
    };

    "highlight does not consume delimiters from code math attributes or link targets"_test = [] {
        for (const auto& source : {
            U"`==literal==`", U"$==literal==$", U"\\(==literal==\\)",
            U"<span title='==literal=='>plain</span>", U"<!-- ==literal== -->",
            U"[label](https://example.com/==literal==)", U"![alt](==image==.png)",
            U"<https://example.com/==literal==>", U"[[==target==|alias]]",
            U"==![a==](img)",
        }) {
            const auto document = highlight_document(source);
            expect_lossless(document);
            expect(first_inline(document, InlineCstKind::Highlight) == nullptr) << cps_to_utf8(source);
        }
        for (const auto& source : {
            U"==a `==code==` b==", U"==a $x==y$ b==", U"==a \\(x==y\\) b==",
            U"==a <b title='=='>b</b> c==", U"==a [label](path==) b==",
            U"==a [[target==|alias]] b==", U"==a [^note==] b==",
        }) {
            const auto document = highlight_document(source);
            expect_lossless(document);
            expect(fatal(document.tree.nodes.size() == 1u)) << cps_to_utf8(source);
            expect(document.tree.nodes.front().kind == InlineCstKind::Highlight) << cps_to_utf8(source);
            expect(document.tree.nodes.front().range == SourceRange{0, document.source.size()});
        }
        const auto crossing = highlight_document(U"==outside [inside==](url)");
        expect(first_inline(crossing, InlineCstKind::Highlight) == nullptr);
        expect(first_inline(crossing, InlineCstKind::Link) != nullptr);
        const auto bare = highlight_document(U"https://example.com/==marked==");
        expect(first_inline(bare, InlineCstKind::Autolink) != nullptr);
        expect(first_inline(bare, InlineCstKind::Highlight) != nullptr);
    };

    "highlight exact runs escaping whitespace and incomplete states remain lossless"_test = [] {
        for (const auto& source : {
            U"=single=", U"===three===", U"====four====", U"====", U"== spaced ==",
            U"==word ==", U"== word==", U"\\==escaped==", U"==escaped\\==",
            U"==line one\n\nline two==",
        }) {
            const auto document = highlight_document(source);
            expect_lossless(document);
            expect(first_inline(document, InlineCstKind::Highlight) == nullptr) << cps_to_utf8(source);
        }
        for (const auto& source : {U"==", U"==unfinished", U"==**bold**", U"==one ==two"}) {
            const auto document = highlight_document(source);
            expect_lossless(document);
            const auto* incomplete = first_inline(document, InlineCstKind::Incomplete);
            expect(fatal(incomplete != nullptr)) << cps_to_utf8(source);
            expect(incomplete->status == ParseStatus::MissingCloser);
        }
        const auto escaped = highlight_document(U"==a\\==b==");
        expect(fatal(escaped.tree.nodes.size() == 1u));
        expect(escaped.tree.nodes.front().kind == InlineCstKind::Highlight);
        expect(bool(inline_visible_text(escaped) == U"a==b"));
        const auto adjacent_escape = highlight_document(U"\\===text==");
        expect(first_inline(adjacent_escape, InlineCstKind::Highlight) != nullptr);
        expect(bool(inline_visible_text(adjacent_escape) == U"=text"));
        const auto escaped_close = highlight_document(U"==text\\===");
        expect(first_inline(escaped_close, InlineCstKind::Highlight) != nullptr);
        expect(bool(inline_visible_text(escaped_close) == U"text="));
    };

    "highlight opt-out and HTML text scopes retain equals source"_test = [] {
        InlineParseContext disabled;
        disabled.dialect.highlights = false;
        InlineParseContext html;
        html.syntax_mode = InlineSyntaxMode::HtmlText;
        for (const auto& context : {disabled, html}) {
            const auto document = highlight_document(U"==literal==", context);
            expect_lossless(document);
            expect(first_inline(document, InlineCstKind::Highlight) == nullptr);
            expect(bool(inline_visible_text(document) == U"==literal=="));
        }
        const auto parsed = parse_text(1, "==literal==", disabled.dialect);
        expect(serialize_markdown(parsed.document) == "==literal==");
    };

    "highlight survives all editable block containers and save reload"_test = [] {
        const std::string source =
            "# ==heading==\n\n==paragraph==\n\n- ==list==\n\n- [x] ==task==\n\n"
            "> ==quote==\n\n> [!NOTE] ==title==\n> ==body==\n\n"
            "| ==cell== |\n| --- |\n| ==body cell== |\n\n[^note]: ==footnote==\n";
        const auto parsed = parse_text(1, source);
        expect(serialize_markdown(parsed.document) == source);
        auto count = 0u;
        walk_blocks(parsed.document.root, [&](const BlockNode& block) {
            if (const auto* document = editable_inline_document(block)) {
                expect_lossless(*document);
                if (first_inline(*document, InlineCstKind::Highlight)) ++count;
            }
        });
        expect(count == 10u);
        const auto reloaded = parse_text(2, serialize_markdown(parsed.document));
        expect(serialize_markdown(reloaded.document) == source);
    };

    "highlight character editing and inverse edits preserve source and markers"_test = [] {
        InlineParseContext context;
        for (const auto& source : {U"==高亮🙂==", U"==**nested**==", U"==a\\==b=="}) {
            InlineDocument document;
            std::vector<TextEdit> inverse;
            reset_core_operation_counters();
            for (const auto character : std::u32string_view(source)) {
                const auto applied = apply_inline_source_edit(NodeId{7}, document,
                    {NodeId{7}, {document.source.size(), document.source.size()}, std::u32string(1, character)}, context);
                inverse.push_back(applied.inverse);
                expect_lossless(document);
            }
            expect(first_inline(document, InlineCstKind::Highlight) != nullptr);
            expect(inline_backward_delete_range(document, 2) == std::optional<SourceRange>{{0, 2}});
            expect(inline_forward_delete_range(document, 0) == std::optional<SourceRange>{{0, 2}});
            for (auto edit = inverse.rbegin(); edit != inverse.rend(); ++edit) {
                apply_inline_source_edit(NodeId{7}, document, *edit, context);
                expect_lossless(document);
            }
            expect(document.source.empty());
            const auto counters = read_core_operation_counters();
            expect(counters.full_document_parses == 0u);
            expect(counters.full_document_serializations == 0u);
        }
    };

    "highlight parser handles long unmatched and deeply nested delimiter input"_test = [] {
        std::u32string unmatched;
        for (auto index = 0; index < 4000; ++index) unmatched += U"==word ";
        const auto document = highlight_document(unmatched);
        expect_lossless(document);
        expect(first_inline(document, InlineCstKind::Highlight) == nullptr);

        std::u32string nested;
        for (auto index = 0; index < 2000; ++index) nested += U"==word ";
        nested += U"center";
        for (auto index = 0; index < 2000; ++index) nested += U" word==";
        const auto tree = parse_inline(nested, {});
        expect(bool(flatten_tokens(tree, nested) == nested));
        const auto* nodes = &tree.nodes;
        auto depth = 0u;
        while (!nodes->empty()) {
            const InlineCstNode* wrapper = nullptr;
            for (const auto& node : *nodes) if (node.kind == InlineCstKind::Highlight) wrapper = &node;
            if (!wrapper) break;
            ++depth;
            nodes = &wrapper->children;
        }
        expect(depth == 2000u);
    };
};
