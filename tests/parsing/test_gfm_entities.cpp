#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "support/folia_test.hpp"
#include "support/fixture.hpp"

import folia.markdown.parser;
import folia.markdown.ast;
import folia.markdown.inline_cst;
import folia.markdown.inline_document;
import folia.markdown.serializer;
import folia.markdown.utf;

using namespace folia;
using namespace boost::ut;

struct GfmEntityCase {
    std::u32string source;
    std::u32string decoded;
};

const auto gfm_entity_cases = [] {
    const auto rows = folia::test::data::read_tsv("parsing/fixtures/conformance/entities.tsv", 2);
    if (rows.size() != 2125) throw std::runtime_error("Incomplete cmark-gfm entity fixture");
    std::vector<GfmEntityCase> cases;
    for (const auto& row : rows)
        cases.push_back({folia::test::data::decode_utf32(row[0]), folia::test::data::decode_utf32(row[1])});
    return cases;
}();

boost::ut::suite gfm_pinned_entity_tests = [] {
    "pinned_cmark_gfm_2125_named_entities_are_semantic_and_lossless"_test = [] {
        std::uint64_t document_id = 1;
        for (const auto& test : gfm_entity_cases) {
            const auto label = cps_to_utf8(test.source);
            const auto parsed = parse_text(document_id++, cps_to_utf8(test.source));
            expect(fatal(!parsed.document.root.children.empty())) << label;
            if (parsed.document.root.children.empty()) continue;

            const auto& inline_document = parsed.document.root.children.front().inline_content;
            expect(fatal(inline_contains_kind(inline_document, InlineCstKind::Entity))) << label;
            expect(fatal(inline_document.source == test.source)) << label;
            expect(fatal(flatten_tokens(inline_document.tree, inline_document.source)
                == inline_document.source)) << label << " token flatten";
            expect(fatal(serialize_lossless(inline_document.tree, inline_document.source)
                == inline_document.source)) << label << " CST serialization";
            expect(fatal(inline_visible_text(inline_document) == test.decoded))
                << label << " decoded value";
            expect(fatal(serialize_markdown_cps(parsed.document) == test.source))
                << label << " saved Markdown";
        }
    };
};
