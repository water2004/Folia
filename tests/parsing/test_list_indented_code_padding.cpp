#include <cstddef>
#include <string>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.parser;
import folia.markdown.ast;
import folia.markdown.block_source;
import folia.markdown.serializer;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

#include "support/parser_test_support.hpp"

suite list_indented_code_padding_tests = [] {
    "retained_list_opening_padding_participates_in_code_indent"_test = [] {
        struct Case {
            std::string source;
            std::u32string block_source;
            std::u32string content;
            std::size_t expected_children;
        };
        for (const auto& test_case : std::vector<Case>{
                {"-\t\tfoo\n", U"\tfoo", U"  foo", 1u},
                {"1.     indented code\n\n   paragraph\n\n       more code\n",
                    U"    indented code\n", U"indented code", 3u},
                {"1.      indented code\n\n   paragraph\n\n       more code\n",
                    U"     indented code\n", U" indented code", 3u}}) {
            const auto parsed = parse_text(1, test_case.source);
            expect(fatal(parsed.document.root.children.size() == 1u));
            if (parsed.document.root.children.size() != 1u) continue;
            const auto& list = parsed.document.root.children.front();
            expect(fatal(list.kind == BlockKind::List));
            expect(fatal(list.children.size() == 1u));
            if (list.children.size() != 1u) continue;
            const auto& item = list.children.front();
            expect(fatal(item.children.size() == test_case.expected_children));
            if (item.children.size() != test_case.expected_children) continue;
            const auto& code = item.children.front();
            expect(fatal(code.kind == BlockKind::CodeBlock));
            expect(bool(code.block_source.source() == test_case.block_source)) << "raw=" << cps_to_utf8(code.block_source.source()) << "; expected=" << cps_to_utf8(test_case.block_source);
            expect(bool(block_source_code_projection(code.block_source) == test_case.content)) << "content=" << cps_to_utf8(block_source_code_projection(code.block_source)) << "; expected=" << cps_to_utf8(test_case.content);
            expect(serialize_markdown(parsed.document) == test_case.source) << "saved=" << serialize_markdown(parsed.document) << "; expected=" << test_case.source;
        }
    };
};
