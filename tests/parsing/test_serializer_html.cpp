#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.ids;
import folia.markdown.parser;
import folia.markdown.serializer;
import folia.markdown.text_edit;

using namespace folia;
using namespace boost::ut;

namespace {

BlockNode paragraph(NodeId id, std::u32string source) {
    BlockNode block;
    block.id = id;
    block.kind = BlockKind::Paragraph;
    block.inline_content.source = std::move(source);
    return block;
}

BlockNode html_container(NodeId id, std::u32string source) {
    BlockNode block;
    block.id = id;
    block.kind = BlockKind::HtmlContainer;
    block.ensure_html_special().source = std::move(source);
    return block;
}

} // namespace

suite serializer_html_projection_tests = [] {
    "html_envelope_keeps_two_owner_slots_and_maps_edited_second_source"_test = [] {
        auto parsed = parse_text(1, "<section><p>A</p><p>b</p></section>");
        expect(fatal(parsed.document.root.children.size() == 1u));
        if (parsed.document.root.children.empty()) return;
        auto& root = parsed.document.root.children.front();
        expect(fatal(root.kind == BlockKind::HtmlContainer));
        expect(fatal(root.children.size() == 2u));
        if (root.children.size() != 2u) return;
        const auto first_owner = root.children[0].id;
        const auto second_owner = root.children[1].id;
        expect(fatal(first_owner != second_owner));
        root.children[1].inline_content.source = U"B";

        const auto projection = serialize_markdown_projection(parsed.document);
        expect(fatal(projection.text == U"<section><p>A</p><p>B</p></section>"));
        expect(fatal(projection.source_maps.size() == 2u));
        if (projection.source_maps.size() == 2u) {
            expect(fatal(projection.source_maps[0].container_id == first_owner));
            expect(fatal(projection.source_maps[0].source_to_serialized
                == std::vector<std::size_t>{12, 13}));
            expect(fatal(projection.source_maps[1].container_id == second_owner));
            expect(fatal(projection.source_maps[1].source_to_serialized
                == std::vector<std::size_t>{20, 21}));
        }
    };

    "html_structural_edit_rebuilds_with_preserved_element_provenance"_test = [] {
        auto parsed = parse_text(1, "<section><p class='x'>A</p></section>");
        expect(fatal(parsed.document.root.children.size() == 1u));
        if (parsed.document.root.children.empty()) return;
        auto& root = parsed.document.root.children.front();
        expect(fatal(root.kind == BlockKind::HtmlContainer));
        expect(fatal(root.children.size() == 1u));
        if (root.children.empty()) return;
        const auto original_owner = root.children.front().id;
        expect(fatal(root.children.front().has_html_element_provenance()));

        // Adding a child invalidates the original envelope shape. The semantic
        // serializer keeps the surviving paragraph's original opening marker.
        root.children.push_back(paragraph(NodeId{999}, U"B"));
        const auto projection = serialize_markdown_projection(parsed.document);
        expect(fatal(projection.text == U"<section><p class='x'>A</p>B</section>"));
        expect(fatal(projection.source_maps.size() == 2u));
        if (projection.source_maps.size() == 2u) {
            expect(fatal(projection.source_maps[0].container_id == original_owner));
            expect(fatal(projection.source_maps[0].source_to_serialized
                == std::vector<std::size_t>{22, 23}));
            expect(fatal(projection.source_maps[1].container_id == NodeId{999}));
            expect(fatal(projection.source_maps[1].source_to_serialized
                == std::vector<std::size_t>{27, 28}));
        }
    };

    "overlapping_html_slots_keep_raw_envelope_without_source_maps"_test = [] {
        auto root = html_container(NodeId{1}, U"<div>AB</div>");
        root.children.push_back(paragraph(NodeId{2}, U"x"));
        root.children.push_back(paragraph(NodeId{3}, U"y"));
        auto& html = root.ensure_html_special();
        html.content_slots = {
            {NodeId{2}, SourceRange{5, 7}},
            {NodeId{3}, SourceRange{6, 7}},
        };
        html.structure_shape = html_block_structure_shape(root);
        EditorDocument document;
        document.root.children.push_back(std::move(root));

        const auto projection = serialize_markdown_projection(document);
        expect(fatal(projection.text == U"<div>AB</div>"));
        expect(fatal(projection.source_maps.empty()));
    };

    "out_of_range_html_slot_keeps_raw_envelope_without_source_maps"_test = [] {
        auto root = html_container(NodeId{1}, U"<div>A</div>");
        root.children.push_back(paragraph(NodeId{2}, U"x"));
        auto& html = root.ensure_html_special();
        html.content_slots = {{NodeId{2}, SourceRange{50, 51}}};
        html.structure_shape = html_block_structure_shape(root);
        EditorDocument document;
        document.root.children.push_back(std::move(root));

        const auto projection = serialize_markdown_projection(document);
        expect(fatal(projection.text == U"<div>A</div>"));
        expect(fatal(projection.source_maps.empty()));
    };
};
