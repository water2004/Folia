#include <optional>
#include <string>

#include "support/folia_test.hpp"

import folia.markdown.parser_source_session;
import folia.markdown.parser_leaf_line_view;

using namespace folia::parser_source_detail;
using namespace boost::ut;

suite leaf_source_accumulator_tests = [] {
    "content_subrange_collects_discontiguous_logical_text_and_maps_boundaries"_test = [] {
        const std::u32string source = U"a>>b\r\n";
        const RootPhysicalSource root(source);
        LeafLine line;
        line.content_cursor = root.begin_line(0);
        line.content_spans = {{0, 1}, {3, 4}};
        line.ending = {4, 6};

        LeafSourceAccumulator accumulator(root);
        accumulator.append_content_range(line, 0, 2);
        expect(accumulator.materialize() == U"ab");
        expect(accumulator.physical_offset(0) == std::optional<std::size_t>{0});
        expect(accumulator.physical_offset(1) == std::optional<std::size_t>{3});
        const auto range = accumulator.physical_range(0, 2);
        expect(range.has_value());
        if (range) {
            expect(range->begin == 0u);
            expect(range->end == 4u);
        }

        accumulator.append_content_range(line, 2, 2);
        expect(accumulator.physical_offset(2) == std::optional<std::size_t>{4});
    };

    "downstream_nonempty_content_beats_prior_empty_anchor"_test = [] {
        const std::u32string source = U"abcdefgh";
        const RootPhysicalSource root(source);
        LeafSourceAccumulator accumulator(root);
        accumulator.append({1, 1});
        accumulator.append({3, 5});

        expect(accumulator.size() == 2u);
        expect(accumulator.physical_offset(0) == std::optional<std::size_t>{3});
        const auto range = accumulator.physical_range(0, 0);
        expect(range.has_value());
        if (range) {
            expect(range->begin == 3u);
            expect(range->end == 3u);
        }
    };

    "repeated_empty_boundaries_choose_the_latest_terminal_anchor"_test = [] {
        const std::u32string source = U"abcdefgh";
        const RootPhysicalSource root(source);
        LeafSourceAccumulator accumulator(root);
        accumulator.append({2, 4});
        accumulator.append({6, 6});
        accumulator.append({7, 7});

        const auto range = accumulator.physical_range(2, 2);
        expect(range.has_value());
        if (range) {
            expect(range->begin == 7u);
            expect(range->end == 7u);
        }
    };
};
