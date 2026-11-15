#include "support/folia_test.hpp"

import folia.markdown.ast;
import folia.markdown.ids;
import folia.markdown.parser;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.markdown.serializer;
import folia.markdown.serializer_visitor;
import folia.markdown.serializer_writer;
import folia.markdown.source_offset_map;
import folia.markdown.utf;
import folia.core.source_projection;

using namespace folia;
using namespace boost::ut;

suite source_projection_storage_tests = [] {
    "source_projection_storage_linear_owners_remain_compact_after_indexing"_test = [] {
        const std::string source = "# first\n\n" + std::string(250'000, 'x')
            + "\n\n" + std::string(250'000, 'y');
        const auto parsed = parse_text(1, source);
        const auto projection = serialize_markdown_projection(parsed.document);
        expect(projection.text == utf8_to_cps(source));
        expect(fatal(projection.source_maps.size() == 3));
        SourceProjectionIndex index;
        index.reset(parsed.document, projection);
        expect(index.blocks().size() == 3_u);
        for (const auto& map : projection.source_maps) {
            expect(map.source_to_serialized.run_count() == 1_u);
            expect(map.source_to_serialized.allocated_bytes() == 0_u);
            for (auto offset : {std::size_t{0}, map.source_to_serialized.size() / 2,
                    map.source_to_serialized.size() - 1}) {
                const TextPosition position{map.container_id, offset, TextAffinity::Downstream};
                expect(index.offset(position) == std::optional{map.source_to_serialized[offset]});
            }
        }
        for (const auto& block : index.blocks()) {
            for (const auto& map : block.owners) {
                expect(map.source_to_serialized.run_count() == 1_u);
                expect(map.source_to_serialized.allocated_bytes() == 0_u);
            }
        }
        // The source-view index rebases its own value; the public projection
        // remains in whole-output coordinates for callers still using it.
        expect(projection.source_maps.back().source_to_serialized.front() > 250'000_u);
    };

    "source_projection_storage_matches_writer_boundaries_across_source_shapes"_test = [] {
        for (const auto& source : std::vector<std::string>{
                "> first\r\n> continuation\r\n>\r\n> last\r\n",
                "- first\n  - nested $x$\n\n    continuation\n- last\n",
                "| A | B |\r\n|---|---|\r\n| x\\|y | z |\r\n",
                "<section><p class='x'>A&amp;B</p><p>next</p></section>\n",
                "> [!NOTE]\r\n>\r\n> text\r\n\r\n$$\r\nx+y\r\n$$\r\n"}) {
            const auto parsed = parse_text(1, source);
            const auto dense = serializer_detail::serialize_document(parsed.document);
            const auto projection = serialize_markdown_projection(parsed.document);
            expect(projection.text == dense.text);
            expect(fatal(projection.source_maps.size() == dense.source_maps.size()));
            SourceProjectionIndex index;
            index.reset(parsed.document, projection);
            for (std::size_t owner = 0; owner < projection.source_maps.size(); ++owner) {
                const auto& map = projection.source_maps[owner];
                const auto& expected = dense.source_maps[owner];
                expect(map.container_id == expected.owner);
                expect(std::ranges::equal(map.source_to_serialized, expected.source_to_serialized));
                for (std::size_t offset = 0; offset < expected.source_to_serialized.size(); ++offset) {
                    const TextPosition position{map.container_id, offset, TextAffinity::Downstream};
                    expect(index.offset(position) == std::optional{expected.source_to_serialized[offset]});
                }
            }
        }
    };

    "source_projection_storage_preserves_first_wins_and_nonmonotonic_slot_maps"_test = [] {
        const std::u32string source = U"abcd";
        serializer_detail::SerializerEventWriter writer;
        writer.source_span(NodeId{7}, source, 2, 4);
        writer.source_span(NodeId{7}, source, 0, 2);
        serializer_detail::SerializerPlanArena arena;
        serializer_detail::SerializerLinePlanner planner(writer, arena);
        const auto lines = planner.build();
        auto emitted = serializer_detail::emit_serializer_lines(arena, lines, writer.events());
        const auto projection = serializer_facade_detail::to_public_projection(std::move(emitted));
        expect(projection.text == U"cdab");
        expect(fatal(projection.source_maps.size() == 1));
        const auto& positions = projection.source_maps.front().source_to_serialized;
        expect(std::ranges::equal(positions, std::vector<std::size_t>{2, 3, 0, 1, 2}));
        const auto first = source_position_for_serialized_offset(projection, 0);
        expect(fatal(first.has_value()));
        expect(first->container_id == NodeId{7});
        expect(first->source_offset == 2_u);
        const auto shared = source_position_for_serialized_offset(projection, 2);
        expect(fatal(shared.has_value()));
        expect(shared->source_offset == 0_u);

        serializer_detail::SerializerEventWriter gap_writer;
        gap_writer.generated_span(U"xx");
        gap_writer.source_span(NodeId{7}, source, 2, 4);
        gap_writer.generated_span(U"---");
        gap_writer.source_span(NodeId{7}, source, 0, 2);
        serializer_detail::SerializerPlanArena gap_arena;
        serializer_detail::SerializerLinePlanner gap_planner(gap_writer, gap_arena);
        const auto gap_lines = gap_planner.build();
        const auto gap_projection = serializer_facade_detail::to_public_projection(
            serializer_detail::emit_serializer_lines(gap_arena, gap_lines, gap_writer.events()));
        expect(gap_projection.text == U"xxcd---ab");
        expect(fatal(gap_projection.source_maps.size() == 1));
        expect(std::ranges::equal(gap_projection.source_maps.front().source_to_serialized,
            std::vector<std::size_t>{7, 8, 2, 3, 4}));
        const auto upstream = source_position_for_serialized_offset(gap_projection, 5, TextAffinity::Upstream);
        const auto downstream = source_position_for_serialized_offset(gap_projection, 5, TextAffinity::Downstream);
        expect(fatal(upstream.has_value() && downstream.has_value()));
        expect(upstream->container_id == NodeId{7});
        expect(downstream->container_id == NodeId{7});
        expect(upstream->source_offset == 4_u);
        expect(downstream->source_offset == 0_u);
    };
};
