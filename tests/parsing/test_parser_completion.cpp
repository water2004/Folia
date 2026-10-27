#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "support/folia_test.hpp"

import folia.markdown.parser;
import folia.markdown.ast;
import folia.markdown.serializer;
import folia.markdown.utf;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.document_text;

using namespace folia;
using namespace boost::ut;

namespace {

using Completion = folia::detail::Parser::Completion;

Completion expected_completion(std::u32string_view text) {
    if (text.empty()) return Completion::Empty;
    return text.back() == U'\r' || text.back() == U'\n'
        ? Completion::LineEnding : Completion::Other;
}

bool verify_completion_summary(std::string_view source) {
    ParseInput input{1, std::string(source)};
    folia::detail::Parser parser(&input);
    auto blocks = parser.parse_blocks();
    bool complete = true;
    const auto verify_node = [&](auto&& self, const BlockNode& block) -> void {
        const auto fact = parser.completions.find(block.id.v);
        if (fact == parser.completions.end()) {
            complete = false;
        } else {
            const auto serialized = serialize_markdown_fragment(BlockVec{block});
            const auto expected = expected_completion(serialized);
            expect(fact->second == expected) << "completion node=" << block.id.v
                << " kind=" << static_cast<int>(block.kind)
                << " actual=" << static_cast<int>(fact->second)
                << " expected=" << static_cast<int>(expected);
            if (fact->second != expected) complete = false;
        }
        if (const auto* owner = editable_inline_document(block)) {
            if (flatten_tokens(owner->tree, owner->source) != owner->source
                || serialize_lossless(owner->tree, owner->source) != owner->source) {
                complete = false;
            }
        }
        for (const auto& child : block.children) self(self, child);
    };
    for (const auto& block : blocks) verify_node(verify_node, block);

    // The completion optimization is parser bookkeeping only. It must leave
    // every parsed owner's source and the document's save bytes untouched.
    const auto parsed = parse_text(1, std::string(source));
    return complete && serialize_markdown(parsed.document) == std::string(source);
}

} // namespace

suite parser_completion = [] {
    "parser_completion_matches_serializer_and_exact_save"_test = [] {
        constexpr std::array<std::string_view, 24> cases{
            "",
            "\r\n\n",
            "alpha\r\nbeta\n\nlast\r",
            "hard break  \r\nnext\n",
            "#\r\n\n## body ###\n",
            "```text\r\nbody\r\n```\r\n",
            "> [!NOTE] title\r\n> body\n>\r\n",
            "[^note]: footnote body\r\n\r\nreference [^note]\n",
            "| a | b |\r\n| --- | --- |\r\n| x | y |\n",
            "<section>\r\n<p>one</p>\n<pre>two</pre>\r\n</section>\r\n> - item\r\n>   continuation\n",
            ">\n> next\n",
            ">\r\n> next\r\n",
            "> first\n>\n>\n> last\n",
            "> first\r\n>\r\n>\r\n> last\r\n",
            "- first\n\n\n- second\n",
            "- first\r\n\r\n\r\n- second\r\n",
            "- \n  text\n",
            "- \r\n  text\r\n",
            "[^ending]: body\r\n\r\nreference\n",
            "<section>\r\n<p>body</p>\r\n</section>\r\n",
            "- ",
            ">",
            "> first\n>",
            "> first\r\n>",
        };
        for (const auto source : cases) {
            expect(verify_completion_summary(source)) << "source=" << source;
        }
    };
};
