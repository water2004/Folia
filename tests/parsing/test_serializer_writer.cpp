#include "support/folia_test.hpp"
import folia.markdown.container_source;
import folia.markdown.ids;
import folia.markdown.serializer_writer;

using namespace folia;
using namespace folia::serializer_detail;
using namespace boost::ut;

suite serializer_event_writer_tests = [] {
    "nested_prefix_eol_keeps_source_anchor_on_body_suffix"_test = [] {
        std::u32string source = U"body";
        ContainerSource outer_source;
        ContainerSource item_source;
        item_source.lines.push_back({
            {NodeId{1}, ContainerAnchorPart::Source, 0},
            U"A\r\nB", false, ContainerPrefixRole::ItemOpening});

        SerializerEventWriter writer;
        SerializerPrefixPolicy quote;
        quote.rule = SerializerPrefixRule::BlockQuote;
        writer.begin_prefix_scope(NodeId{3}, outer_source, quote);
        SerializerPrefixPolicy item;
        item.rule = SerializerPrefixRule::ListItem;
        item.marker = U"- ";
        writer.begin_prefix_scope(NodeId{2}, item_source, item);
        writer.syntax_anchor({NodeId{2}, ContainerAnchorPart::Start, 0});
        writer.source_span(NodeId{1}, source);
        writer.syntax_anchor_before_ending({NodeId{2}, ContainerAnchorPart::End, 0});
        writer.end_prefix_scope(NodeId{2});
        writer.end_prefix_scope(NodeId{3});

        SerializerPlanArena arena;
        SerializerLinePlanner planner(writer, arena);
        const auto lines = planner.build();
        const auto output = emit_serializer_lines(arena, lines, writer.events());

        expect(fatal(output.text == U"> - A\r\n> Bbody"));
        expect(fatal(output.source_maps.size() == 1u));
        if (!output.source_maps.empty()) {
            expect(fatal(output.source_maps.front().source_to_serialized.size() == 5u));
            expect(fatal(output.source_maps.front().source_to_serialized[0] == 10u));
            expect(fatal(output.source_maps.front().source_to_serialized[4] == 14u));
        }
        bool start_found = false;
        for (const auto& anchor : output.syntax_anchors) {
            if (anchor.anchor == ContainerPrefixAnchor{NodeId{2}, ContainerAnchorPart::Start, 0}) {
                start_found = anchor.serialized_offset == 10u;
            }
        }
        expect(fatal(start_found));
    };

    "source_slots_merge_one_owner_map_with_first_boundary_wins"_test = [] {
        std::u32string source = U"abcd";
        ContainerSource no_prefixes;
        SerializerEventWriter writer;
        const auto first = writer.source_span(NodeId{7}, source, 0, 2);
        const auto second = writer.source_span(NodeId{7}, source, 2, 4);
        (void)first;
        (void)second;
        SerializerPlanArena arena;
        SerializerLinePlanner planner(writer, arena);
        const auto lines = planner.build();
        const auto output = emit_serializer_lines(arena, lines, writer.events());
        expect(fatal(output.text == U"abcd"));
        expect(fatal(output.source_maps.size() == 1u));
        if (!output.source_maps.empty()) {
            const std::vector<std::size_t> expected{0, 1, 2, 3, 4};
            expect(fatal(output.source_maps.front().source_to_serialized == expected));
        }
        (void)no_prefixes;
    };

    "atomic_generated_map_has_two_endpoints_across_line_endings"_test = [] {
        SerializerEventWriter writer;
        writer.atomic_generated_span(NodeId{9}, U"a\r\nb");
        SerializerPlanArena arena;
        SerializerLinePlanner planner(writer, arena);
        const auto lines = planner.build();
        const auto output = emit_serializer_lines(arena, lines, writer.events());
        expect(fatal(output.text == U"a\r\nb"));
        expect(fatal(output.source_maps.size() == 1u));
        if (!output.source_maps.empty()) {
            const std::vector<std::size_t> expected{0, 4};
            expect(fatal(output.source_maps.front().source_to_serialized == expected));
        }
    };

    "separator_completion_does_not_reserialize_previous_block"_test = [] {
        SerializerEventWriter writer;
        writer.block_begin(NodeId{1});
        writer.generated_span(U"abc\n");
        writer.block_completion(NodeId{1});
        writer.sibling_separator(NodeId{1}, NodeId{2}, false, false, std::nullopt);
        std::u32string next = U"x";
        writer.source_span(NodeId{2}, next);
        SerializerPlanArena arena;
        SerializerLinePlanner planner(writer, arena);
        const auto lines = planner.build();
        const auto output = emit_serializer_lines(arena, lines, writer.events());
        expect(fatal(output.text == U"abc\n\nx"));
        std::vector<std::size_t> separators;
        for (const auto& anchor : output.syntax_anchors) {
            if (anchor.anchor.owner_id == NodeId{2}
                && anchor.anchor.part == ContainerAnchorPart::Separator) {
                separators.push_back(anchor.serialized_offset);
            }
        }
        const std::vector<std::size_t> expected{4, 5};
        expect(fatal(separators == expected));
    };
};
