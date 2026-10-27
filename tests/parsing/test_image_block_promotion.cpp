#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include "support/folia_test.hpp"

import folia.markdown.ast;
import folia.markdown.document;
import folia.markdown.image_dimension;
import folia.markdown.inline_cst;
import folia.markdown.inline_parser;
import folia.markdown.outline;
import folia.markdown.parser;
import folia.core.render_builder;
import folia.core.render_model;
import folia.markdown.serializer;
import folia.markdown.symbols;
import folia.core.theme;
import folia.markdown.utf;
import folia.markdown.html_cst;
import folia.markdown.image_block_promotion;

using namespace folia;
using namespace boost::ut;

namespace {

BlockNode parsed_paragraph(std::string source) {
    auto parsed = parse_text(1, source);
    if (parsed.document.root.children.empty()) return {};
    auto block = std::move(parsed.document.root.children.front());
    // The canonical parser currently performs this presentation decision
    // during dispatch. Candidate tests isolate the new final-CST helper by
    // handing it the same already-parsed owner as a Paragraph.
    block.kind = BlockKind::Paragraph;
    return block;
}

EditorDocument document_with(BlockNode block, std::u32string trailing_line_ending = {}) {
    EditorDocument document;
    document.root.children.push_back(std::move(block));
    document.trailing_line_ending = std::move(trailing_line_ending);
    return document;
}

RenderModel render(BlockNode block) {
    auto document = document_with(std::move(block));
    return build_render_model(
        document,
        Outline{},
        DocumentSymbolIndex{},
        default_theme_profile());
}

}

suite image_block_promotion_candidate_tests = [] {
    "a_sole_markdown_image_promotes_without_changing_source_or_render"_test = [] {
        constexpr std::string_view source = "![alt](image.png \"Title\")\n";
        auto block = parsed_paragraph(std::string(source));
        const auto before_id = block.id;
        const auto before_source = block.inline_content.source;
        const auto before_tree_nodes = block.inline_content.tree.nodes.size();

        expect(promote_image_paragraph(block));
        expect(block.kind == BlockKind::ImageBlock);
        expect(block.id == before_id);
        expect(block.inline_content.source == before_source);
        expect(block.inline_content.tree.nodes.size() == before_tree_nodes);
        expect(block.image_special().src == "image.png");
        expect(block.image_special().image_alt == "alt");
        expect(block.image_special().image_title == std::optional<std::string>{"Title"});
        expect(serialize_markdown(document_with(BlockNode(block), U"\n")) == source);

        const auto model = render(std::move(block));
        expect(model.blocks.size() == 1u);
        if (model.blocks.size() == 1) {
            expect(model.blocks.front().kind == RenderBlockKind::Image);
            expect(model.blocks.front().special().src == "image.png");
            expect(model.blocks.front().special().alt == "alt");
            expect(model.blocks.front().special().title == std::optional<std::string>{"Title"});
        }
    };

    "a_link_wrapped_image_keeps_both_image_and_link_metadata"_test = [] {
        auto block = parsed_paragraph(
            "[![alt](image.png \"image title\")]"
            "(https://example.test/target)");
        const auto source = block.inline_content.source;
        expect(promote_image_paragraph(block));
        expect(block.kind == BlockKind::ImageBlock);
        expect(block.image_special().src == "image.png");
        expect(block.image_special().image_alt == "alt");
        expect(block.image_special().image_title
            == std::optional<std::string>{"image title"});
        expect(block.image_special().image_link
            == std::optional<std::string>{"https://example.test/target"});
        expect(serialize_markdown(document_with(BlockNode(block))) == cps_to_utf8(source));

        const auto model = render(std::move(block));
        expect(model.blocks.size() == 1u);
        if (model.blocks.size() == 1) {
            expect(model.blocks.front().kind == RenderBlockKind::Image);
            expect(model.blocks.front().special().link
                == std::optional<std::string>{"https://example.test/target"});
        }
    };

    "whitespace_text_and_incomplete_images_stay_paragraphs"_test = [] {
        for (const auto source : {
                std::string("  ![alt](image.png)"),
                std::string("![alt](image.png)  "),
                std::string("![alt](image.png) trailing"),
                std::string("![alt]("),
            }) {
            auto block = parsed_paragraph(source);
            const auto before = block.inline_content.source;
            expect(!promote_image_paragraph(block));
            expect(block.kind == BlockKind::Paragraph);
            expect(block.inline_content.source == before);
            expect(serialize_markdown(document_with(std::move(block))) == source);
        }
    };

    "a_multiline_sole_image_stays_an_editable_paragraph"_test = [] {
        auto block = parsed_paragraph("![alt](image.png)");
        block.inline_content.source = U"![alt\ntext](image.png)";
        block.inline_content.tree = parse_inline(
            block.inline_content.source, InlineParseContext{});
        expect(block.inline_content.tree.nodes.size() == 1u);
        if (block.inline_content.tree.nodes.size() != 1) return;
        expect(block.inline_content.tree.nodes.front().kind == InlineCstKind::Image);
        const auto before_source = block.inline_content.source;
        const auto before_flattened = flatten_tokens(
            block.inline_content.tree, block.inline_content.source);
        expect(!promote_image_paragraph(block));
        expect(block.kind == BlockKind::Paragraph);
        expect(block.inline_content.source == before_source);
        expect(flatten_tokens(block.inline_content.tree, block.inline_content.source)
            == before_flattened);
        expect(serialize_markdown(document_with(BlockNode(block)))
            == cps_to_utf8(before_source));
        const auto model = render(std::move(block));
        expect(model.blocks.size() == 1u);
        if (model.blocks.size() == 1)
            expect(model.blocks.front().kind == RenderBlockKind::Text);
    };

    "the_image_promotion_helper_does_not_reclassify_headings"_test = [] {
        auto paragraph = parsed_paragraph("![x](u)");
        BlockNode heading = std::move(paragraph);
        heading.kind = BlockKind::Heading;
        const auto before_source = heading.inline_content.source;
        expect(!promote_image_paragraph(heading));
        expect(heading.kind == BlockKind::Heading);
        expect(heading.inline_content.source == before_source);
    };

    "raw_html_image_maker_builds_semantics_from_the_existing_html_cst"_test = [] {
        const std::u32string source =
            U"<img src=\"image.png\" alt=\"alt\" title=\"caption\" width=\"320\" height=\"50%\">";
        const auto tree = parse_html_cst(source);
        expect(tree.nodes.size() == 1u);
        if (tree.nodes.size() != 1) return;

        const auto read_attribute = [&](const HtmlCstNode& node, std::string_view name)
            -> std::optional<std::u32string> {
            const auto found = std::ranges::find(node.attributes, name, &HtmlAttribute::name);
            if (found == node.attributes.end()) return std::nullopt;
            if (!found->value_range) return std::u32string{};
            const auto range = *found->value_range;
            if (!range.valid_for(source.size())) return std::nullopt;
            return source.substr(range.start, range.length());
        };
        const auto block = make_html_image_block_from_element(
            tree.nodes.front(),
            NodeId(7),
            true,
            read_attribute,
            [](std::string_view target, bool image) {
                return image && !target.starts_with("javascript:");
            });
        expect(block.has_value());
        if (!block) return;
        expect(block->kind == BlockKind::ImageBlock);
        expect(block->image_special().src == "image.png");
        expect(block->image_special().image_alt == "alt");
        expect(block->image_special().image_title == std::optional<std::string>{"caption"});
        expect(block->image_special().image_width
            == std::optional<ImageDimension>{ImageDimension::pixels(320.0f)});
        expect(block->image_special().image_height
            == std::optional<ImageDimension>{ImageDimension::percent(50.0f)});

        // The existing raw-HTML dispatcher attaches its already-parsed
        // envelope after make_html_semantic returns.
        auto html_image = *block;
        auto& html = html_image.ensure_html_special();
        html.source = source;
        html.tree = tree;
        html.structure_shape = html_block_structure_shape(html_image);
        html.root_tag = "img";
        expect(serialize_markdown(document_with(BlockNode(html_image))) == cps_to_utf8(source));

        const auto model = render(BlockNode(html_image));
        expect(model.blocks.size() == 1u);
        if (model.blocks.size() == 1) {
            expect(model.blocks.front().kind == RenderBlockKind::Image);
            expect(model.blocks.front().special().src == "image.png");
            expect(model.blocks.front().special().image_width
                == std::optional<ImageDimension>{ImageDimension::pixels(320.0f)});
            expect(model.blocks.front().special().image_height
                == std::optional<ImageDimension>{ImageDimension::percent(50.0f)});
        }
    };

    "raw_html_image_maker_uses_parser_safe_target_policy"_test = [] {
        const std::u32string source = U"<img src=\"javascript:bad\">";
        const auto tree = parse_html_cst(source);
        expect(tree.nodes.size() == 1u);
        if (tree.nodes.size() != 1) return;
        const auto read_attribute = [&](const HtmlCstNode& node, std::string_view name)
            -> std::optional<std::u32string> {
            const auto found = std::ranges::find(node.attributes, name, &HtmlAttribute::name);
            if (found == node.attributes.end()) return std::nullopt;
            if (!found->value_range) return std::u32string{};
            const auto range = *found->value_range;
            if (!range.valid_for(source.size())) return std::nullopt;
            return source.substr(range.start, range.length());
        };
        expect(!make_html_image_block_from_element(
            tree.nodes.front(), NodeId(9), false, read_attribute,
            [](std::string_view, bool) { return true; }));
        expect(!make_html_image_block_from_element(
            tree.nodes.front(), NodeId(8), true, read_attribute,
            [](std::string_view target, bool image) {
                return image && !target.starts_with("javascript:");
            }));
    };
};
