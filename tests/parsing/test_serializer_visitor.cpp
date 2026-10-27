#include "support/folia_test.hpp"
import folia.markdown.ast;
import folia.markdown.document;
import folia.markdown.ids;
import folia.markdown.serializer;
import folia.markdown.utf;

using namespace folia;

using namespace boost::ut;

suite serializer_action_visitor_tests = [] {
    "quote_event_frame_preserves_terminal_newline_and_source_projection"_test = [] {
        BlockNode quote;
        quote.id = NodeId{2};
        quote.kind = BlockKind::BlockQuote;
        BlockNode paragraph;
        paragraph.id = NodeId{3};
        paragraph.kind = BlockKind::Paragraph;
        paragraph.inline_content.source = U"body\n";
        quote.children.push_back(std::move(paragraph));
        BlockVec blocks;
        blocks.push_back(std::move(quote));

        EditorDocument document;
        document.root.children = std::move(blocks);
        const auto output = serialize_markdown_projection(document);
        expect(fatal(output.text == U"> body\n"));
        expect(fatal(output.source_maps.size() == 1u));
        if (!output.source_maps.empty()) {
            expect(fatal(output.source_maps.front().source_to_serialized[0] == 2u));
            expect(fatal(output.source_maps.front().source_to_serialized[5] == 7u));
        }
    };

    "list_item_frames_apply_marker_and_continuation_indent_once"_test = [] {
        BlockNode list;
        list.id = NodeId{10};
        list.kind = BlockKind::List;
        BlockNode item;
        item.id = NodeId{11};
        item.kind = BlockKind::ListItem;
        item.ensure_item_special().marker = U"- ";
        BlockNode paragraph;
        paragraph.id = NodeId{12};
        paragraph.kind = BlockKind::Paragraph;
        paragraph.inline_content.source = U"first\nsecond";
        item.children.push_back(std::move(paragraph));
        list.children.push_back(std::move(item));
        BlockVec blocks;
        blocks.push_back(std::move(list));

        EditorDocument document;
        document.root.children = std::move(blocks);
        const auto output = serialize_markdown_projection(document);
        expect(bool(output.text == U"- first\n  second"))
            << "serialized text=" << cps_to_utf8(output.text);
        expect(fatal(output.source_maps.size() == 1u));
        if (!output.source_maps.empty()) {
            const auto& map = output.source_maps.front();
            expect(fatal(map.container_id == NodeId{12}));
            expect(bool(map.source_to_serialized[0] == 2u))
                << "source map[0]=" << map.source_to_serialized[0];
            expect(bool(map.source_to_serialized[6] == 10u))
                << "source map[6]=" << map.source_to_serialized[6];
        }
    };
};
