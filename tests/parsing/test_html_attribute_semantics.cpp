#include "support/folia_test.hpp"
import folia.markdown.ids;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.inline_parser;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

suite html_attribute_semantics = [] {
"html attributes decode references once while preserving source and literal backslashes"_test = [] {
    const std::u32string source = U"<a href=\"folder/a&amp;b.png\" title=\"a&quot;b\">label</a>"
        U"<img src='a\\&amp;b.png' alt='a&quot;b &amp;lt; &unknown;' width='&#51;20'>";
    InlineParseContext context;
    context.next_id = NodeId{1};
    const auto tree = parse_inline(source, context);
    expect(fatal(tree.nodes.size() == 2_u));
    expect(flatten_tokens(tree, source) == source);
    const auto& link = tree.nodes[0].semantics();
    const auto& image = tree.nodes[1].semantics();
    expect(link.href == "folder/a&b.png");
    expect(link.title == std::optional<std::string>{"a\"b"});
    expect(image.href == "a\\&b.png");
    expect(image.alt == "a\"b &lt; &unknown;");
    expect(image.html_attributes.at("width") == "320");
};
};
