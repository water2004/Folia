#include <cstdint>
#include <utility>
#include <vector>
#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.document;
import folia.markdown.ids;
import folia.markdown.serializer;

using namespace folia;
using namespace boost::ut;

namespace {

BlockNode make_deep_quote_tree(std::size_t depth, NodeId& paragraph_id) {
    BlockNode node;
    node.id = NodeId{2};
    node.kind = BlockKind::Paragraph;
    node.inline_content.source = U"a";
    paragraph_id = node.id;
    std::uint64_t next_id = 3;
    for (std::size_t level = 0; level < depth; ++level) {
        BlockNode quote;
        quote.id = NodeId{next_id++};
        quote.kind = BlockKind::BlockQuote;
        quote.children.push_back(std::move(node));
        node = std::move(quote);
    }
    return node;
}

BlockNode make_deep_list_tree(std::size_t depth, std::vector<NodeId>& paragraph_ids) {
    BlockNode subtree;
    paragraph_ids.resize(depth);
    std::uint64_t next_id = 2;
    for (std::size_t reverse = depth; reverse > 0; --reverse) {
        const auto level = reverse - 1;
        BlockNode list;
        list.id = NodeId{next_id++};
        list.kind = BlockKind::List;

        BlockNode item;
        item.id = NodeId{next_id++};
        item.kind = BlockKind::ListItem;

        BlockNode paragraph;
        paragraph.id = NodeId{next_id++};
        paragraph.kind = BlockKind::Paragraph;
        paragraph.inline_content.source = U"x";
        paragraph_ids[level] = paragraph.id;
        item.children.push_back(std::move(paragraph));
        if (reverse < depth) item.children.push_back(std::move(subtree));
        list.children.push_back(std::move(item));
        subtree = std::move(list);
    }
    return subtree;
}

const SerializedSourceMap* find_map(
    SerializedMarkdownProjection const& projection,
    NodeId owner) {
    for (const auto& map : projection.source_maps) {
        if (map.container_id == owner) return &map;
    }
    return nullptr;
}

} // namespace

suite deep_serializer_tests = [] {
    "deep_quote_50000_serializes_iteratively_with_exact_leaf_map"_test = [] {
        constexpr std::size_t depth = 50000;
        NodeId paragraph_id{};
        EditorDocument document;
        document.root.children.push_back(make_deep_quote_tree(depth, paragraph_id));

        const auto projection = serialize_markdown_projection(document);
        std::u32string expected;
        expected.reserve(depth * 2 + 1);
        for (std::size_t level = 0; level < depth; ++level) expected += U"> ";
        expected.push_back(U'a');

        expect(fatal(projection.text == expected));
        const auto* map = find_map(projection, paragraph_id);
        expect(fatal(map != nullptr));
        if (map != nullptr) {
            expect(fatal(map->source_to_serialized == std::vector<std::size_t>{depth * 2, depth * 2 + 1}));
        }
    };

    "deep_nested_lists_1000_preserve_indentation_and_every_leaf_map"_test = [] {
        constexpr std::size_t depth = 1000;
        std::vector<NodeId> paragraph_ids;
        EditorDocument document;
        document.root.children.push_back(make_deep_list_tree(depth, paragraph_ids));

        const auto projection = serialize_markdown_projection(document);
        std::u32string expected;
        expected.reserve(depth * depth + depth * 5);
        for (std::size_t level = 0; level < depth; ++level) {
            expected.append(level * 2, U' ');
            expected += U"- x";
            if (level + 1 < depth) expected.push_back(U'\n');
        }
        expect(fatal(projection.text == expected));
        expect(fatal(projection.source_maps.size() == depth));

        for (std::size_t level = 0; level < depth; ++level) {
            const auto* map = find_map(projection, paragraph_ids[level]);
            expect(fatal(map != nullptr));
            if (map == nullptr) continue;
            const auto source_begin = level * level + 5 * level + 2;
            expect(fatal(map->source_to_serialized == std::vector<std::size_t>{source_begin, source_begin + 1}));
        }
    };
};
