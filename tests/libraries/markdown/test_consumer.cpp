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
};
