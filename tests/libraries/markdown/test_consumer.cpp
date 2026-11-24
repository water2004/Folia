#include "support/folia_test.hpp"
import folia.markdown;

using namespace boost::ut;
using namespace folia;

suite markdown_consumer = [] {
    "standalone Markdown library preserves source spellings and line endings"_test = [] {
        for (std::string source : {
            "# title\r\n\r\n_a_ **b** [t](<url> \"name\") &amp; \\*x\\*\r\n",
            "- [x] first\n- second\n\n> quote `$x$`\n",
            "| a | b |\n| --- | --- |\n| `x` | $y$ |\n",
            "```cpp\nint x;\n```\n\n$$\nx^2\n$$\n"}) {
            auto parsed = parse_text(1, source);
            expect(serialize_markdown(parsed.document) == source);
            expect(serialize_markdown(parse_text(2, serialize_markdown(parsed.document)).document) == source);
        }
    };
    "standalone inline CST retains each incomplete source prefix"_test = [] {
        for (std::u32string source : {U"**abc**", U"[t](<url>)", U"$x^2$", U"a\\**b*", U"&amp;"}) {
            for (std::size_t n = 0; n <= source.size(); ++n) {
                auto prefix = source.substr(0, n);
                auto tree = parse_inline(prefix, InlineParseContext{});
                expect(flatten_tokens(tree, prefix) == prefix);
            }
        }
    };
    "standalone parser exposes the same document tree and derived outline"_test = [] {
        auto parsed = parse_text(1, "# One\n\nbody\n\n## Two\n");
        expect(parsed.document.root.kind == BlockKind::Document);
        expect(parsed.document.root.children.size() >= 3_u);
        expect(parsed.outline.flat_items().size() == 2_u);
    };
    "standalone Markdown library exposes highlights inside custom callouts"_test = [] {
        const std::string source = "> [!Research-idea_2] ==Title==\r\n"
            "> **==Body==** and ==[link](https://example.test)==\r\n";
        const auto parsed = parse_text(1, source);
        expect(fatal(parsed.document.root.children.size() == 1_u));
        const auto& callout = parsed.document.root.children.front();
        expect(callout.kind == BlockKind::Callout);
        expect(callout.container_special().callout_kind == "RESEARCH-IDEA_2");
        expect(fatal(callout.children.size() == 2_u));
        const auto& title = callout.children.front();
        const auto& body = callout.children.back();
        expect(title.kind == BlockKind::CalloutTitle);
        expect(bool(inline_visible_text(title.inline_content()) == U"Title"));
        expect(bool(inline_visible_text(body.inline_content()) == U"Body and link"));
        expect(fatal(!title.inline_content().tree.nodes.empty()));
        expect(title.inline_content().tree.nodes.front().kind == InlineCstKind::Highlight);
        expect(bool(flatten_tokens(title.inline_content().tree, title.inline_content().source)
            == title.inline_content().source));
        expect(serialize_markdown(parsed.document) == source);
        expect(serialize_markdown(parse_text(2, serialize_markdown(parsed.document)).document) == source);
    };
};
