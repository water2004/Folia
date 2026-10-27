#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.document;
import folia.markdown.ids;
import folia.markdown.serializer;

using namespace folia;
using namespace boost::ut;

namespace {
BlockNode paragraph(std::uint64_t id, std::u32string source) {
    BlockNode block;
    block.id = NodeId{id};
    block.kind = BlockKind::Paragraph;
    block.inline_content.source = std::move(source);
    return block;
}

SerializedMarkdownProjection project(BlockVec blocks) {
    EditorDocument document;
    document.root.children = std::move(blocks);
    return serialize_markdown_projection(document);
}

std::u32string serialize(BlockVec blocks) {
    return project(std::move(blocks)).text;
}

std::u32string serialize_pair(std::u32string first, std::u32string second) {
    BlockVec blocks;
    blocks.push_back(paragraph(1, std::move(first)));
    blocks.push_back(paragraph(2, std::move(second)));
    return serialize(std::move(blocks));
}

BlockNode quoted_pair(std::u32string first, std::u32string second) {
    BlockNode quote;
    quote.id = NodeId{10};
    quote.kind = BlockKind::BlockQuote;
    quote.children.push_back(paragraph(11, std::move(first)));
    quote.children.push_back(paragraph(12, std::move(second)));
    return quote;
}
}

suite serializer_block_separator_regression_tests = [] {
    "dynamic_sibling_separator_compresses_only_lf_terminal_source"_test = [] {
        expect(fatal(serialize_pair(U"a\n", U"b") == U"a\n\nb"));
        // The historical serializer checks only text.back() == LF. A lone CR
        // therefore retains the ordinary blank-line separator.
        expect(fatal(serialize_pair(U"a\r", U"b") == U"a\r\n\nb"));
        expect(fatal(serialize_pair(U"a\r\n", U"b") == U"a\r\n\nb"));
    };

    "empty_previous_paragraph_keeps_single_separator"_test = [] {
        expect(fatal(serialize_pair(U"", U"b") == U"\nb"));
    };

    "nested_quote_preserves_dynamic_lf_versus_cr_separator"_test = [] {
        BlockVec lf;
        lf.push_back(quoted_pair(U"a\n", U"b"));
        expect(fatal(serialize(std::move(lf)) == U"> a\n>\n> b"));

        BlockVec cr;
        cr.push_back(quoted_pair(U"a\r", U"b"));
        auto output = project(std::move(cr));
        expect(fatal(output.text == U"> a\r\n>\n> b"));
        expect(fatal(output.source_maps.size() == 2u));
        bool first_map = false;
        bool second_map = false;
        for (const auto& map : output.source_maps) {
            if (map.container_id == NodeId{11}) {
                first_map = true;
                expect(fatal(map.source_to_serialized == std::vector<std::size_t>{2, 3, 4}));
            } else if (map.container_id == NodeId{12}) {
                second_map = true;
                expect(fatal(map.source_to_serialized == std::vector<std::size_t>{9, 10}));
            }
        }
        expect(fatal(first_map && second_map));
    };
};

