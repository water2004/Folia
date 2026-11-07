#include "support/folia_test.hpp"

import folia.markdown.ids;
import folia.markdown.selection;
import folia.markdown.text_edit;
import folia.platform.editor_display_mapping;

using namespace boost::ut;
using namespace folia;
using namespace folia::platform::editor;

namespace {
std::size_t ScalarDisplayPosition(std::vector<EditorDisplayPosition> const& mapping, TextPosition source) {
    std::optional<std::size_t> last, exactSource, generated, greater;
    for (std::size_t i = 0; i < mapping.size(); ++i) {
        auto point = mapping[i];
        if (point.container_id != source.container_id) continue;
        last = i;
        if (point.source_offset < source.source_offset) continue;
        if (point.source_offset > source.source_offset) { if (!greater) greater = i; continue; }
        if (point.kind == EditorDisplayPositionKind::GeneratedPreview && point.affinity == source.affinity) return i;
        if (point.kind == EditorDisplayPositionKind::Source) {
            if (point.affinity == source.affinity) return i;
            if (!exactSource) exactSource = i;
        } else if (point.affinity == source.affinity) {
            if (point.kind == EditorDisplayPositionKind::BoundaryDecoration && source.affinity == TextAffinity::Downstream)
                generated = (std::min)(i + 1, mapping.size() - 1);
            else if (!generated) generated = i;
        }
    }
    return generated.value_or(exactSource.value_or(greater.value_or(last.value_or(0))));
}
}

suite display_mapping_tests = [] {

"display mapping compresses long source runs and copies by runs"_test = [] {
    EditorDisplayMapping mapping;
    for (std::size_t i = 0; i != 10 * 1024 * 1024; ++i)
        mapping.push_back({NodeId{1}, i, TextAffinity::Downstream});
    expect(mapping.storage_bytes() < 256_u);
    auto copy = mapping;
    expect(copy.storage_bytes() < 256_u);
    mapping.resize(12);
    expect(copy.size() == 10485760_u);
    expect(mapping.back().source_offset == 11_u);
    expect(copy.back().source_offset == 10485759_u);
    auto moved = std::move(mapping);
    expect(mapping.empty());
    expect(moved.size() == 12_u);
};

"display mapping range mutations match uncompressed positions"_test = [] {
    std::vector<EditorDisplayPosition> expected;
    EditorDisplayMapping actual;
    std::uint64_t random = 19;
    auto next = [&] { random = random * 6364136223846793005ULL + 1; return random >> 32; };
    auto point = [&] {
        return EditorDisplayPosition{NodeId{next() % 3 + 1}, next() % 20,
            next() % 2 ? TextAffinity::Downstream : TextAffinity::Upstream,
            static_cast<EditorDisplayPositionKind>(next() % 4)};
    };
    for (int step = 0; step != 3000; ++step) {
        auto at = next() % (expected.size() + 1);
        auto count = next() % 5;
        auto value = point();
        switch (next() % 4) {
        case 0:
            actual.insert(actual.begin() + at, count, value);
            expected.insert(expected.begin() + at, count, value);
            break;
        case 1: {
            auto end = (std::min)(expected.size(), at + count);
            actual.erase(actual.begin() + at, actual.begin() + end);
            expected.erase(expected.begin() + at, expected.begin() + end);
            break;
        }
        case 2:
            actual.push_back(value); expected.push_back(value); break;
        case 3: {
            auto end = (std::min)(expected.size(), at + count);
            auto saved = std::vector<EditorDisplayPosition>(expected.begin() + at, expected.begin() + end);
            actual.insert(actual.begin() + at, actual.begin() + at, actual.begin() + end);
            expected.insert(expected.begin() + at, saved.begin(), saved.end());
            break;
        }
        }
        expect(fatal(actual.size() == expected.size()));
        std::size_t i = 0;
        for (auto value : actual) {
            expect(static_cast<TextPosition>(value) == static_cast<TextPosition>(expected[i]));
            expect(value.kind == expected[i].kind);
            expect(actual[i].source_offset == expected[i].source_offset);
            ++i;
        }
        if (step % 10 == 0) {
            auto query = static_cast<TextPosition>(point());
            expect(DisplayPositionForSource(actual, query) == ScalarDisplayPosition(expected, query));
            auto begin = next() % (expected.size() + 1);
            auto end = (std::min)(expected.size(), begin + 20);
            auto cursor = begin;
            actual.for_each_run(begin, end, [&](auto first, auto last, auto a, auto b) {
                expect(first == cursor);
                expect(static_cast<TextPosition>(a) == static_cast<TextPosition>(expected[first]));
                expect(static_cast<TextPosition>(b) == static_cast<TextPosition>(expected[last - 1]));
                cursor = last;
                return true;
            });
            expect(cursor == end);
        }
    }
};

"leading layout spacing cannot hide exact positions earlier in the same source block"_test = [] {
    const auto owner = NodeId{99};
    EditorDisplayMapping mapping{
        {owner, 3, TextAffinity::Downstream, EditorDisplayPositionKind::BoundaryDecoration},
        {owner, 3, TextAffinity::Downstream, EditorDisplayPositionKind::BoundaryDecoration},
        {owner, 0, TextAffinity::Downstream}, {owner, 1, TextAffinity::Downstream},
        {owner, 2, TextAffinity::Downstream}, {owner, 3, TextAffinity::Downstream},
        {owner, 4, TextAffinity::Downstream},
    };
    for (auto affinity : {TextAffinity::Upstream, TextAffinity::Downstream})
        for (std::size_t i = 0; i <= 4; ++i)
            expect(DisplayPositionForSource(mapping, {owner, i, affinity}) == i + 2);
};

"hidden format source offsets map to their visible boundary"_test = [] {
    const auto owner = NodeId{50};
    // **ab** tail: every delimiter remains in the block-local source.
    const EditorDisplayMapping mapping{
        {owner, 2, TextAffinity::Downstream},
        {owner, 3, TextAffinity::Downstream},
        {owner, 6, TextAffinity::Downstream},
        {owner, 7, TextAffinity::Downstream},
        {owner, 8, TextAffinity::Downstream},
        {owner, 9, TextAffinity::Downstream},
        {owner, 10, TextAffinity::Downstream},
        {owner, 11, TextAffinity::Upstream},
    };
    for (auto affinity : {TextAffinity::Upstream, TextAffinity::Downstream}) {
        expect(DisplayPositionForSource(mapping, {owner, 0, affinity}) == 0_u);
        expect(DisplayPositionForSource(mapping, {owner, 1, affinity}) == 0_u);
        expect(DisplayPositionForSource(mapping, {owner, 4, affinity}) == 2_u);
        expect(DisplayPositionForSource(mapping, {owner, 5, affinity}) == 2_u);
        expect(DisplayPositionForSource(mapping, {owner, 8, affinity}) == 4_u);
    }
};

"downstream_empty_content_caret_follows_generated_prefix"_test = [] {
    const auto owner = NodeId{42};
    const EditorDisplayMapping mapping{
        {owner, 0, TextAffinity::Downstream, EditorDisplayPositionKind::BoundaryDecoration},
        {owner, 0, TextAffinity::Downstream, EditorDisplayPositionKind::BoundaryDecoration},
        {owner, 0, TextAffinity::Upstream, EditorDisplayPositionKind::BoundaryDecoration},
    };

    expect(DisplayPositionForSource(
        mapping,
        {owner, 0, TextAffinity::Downstream}) == 2u);
    expect(DisplayPositionForSource(
        mapping,
        {owner, 0, TextAffinity::Upstream}) == 2u);
};

"real_source_mapping_still_wins_after_generated_prefix"_test = [] {
    const auto owner = NodeId{43};
    const EditorDisplayMapping mapping{
        {owner, 0, TextAffinity::Downstream, EditorDisplayPositionKind::BoundaryDecoration},
        {owner, 0, TextAffinity::Downstream, EditorDisplayPositionKind::BoundaryDecoration},
        {owner, 0, TextAffinity::Downstream, EditorDisplayPositionKind::Source},
        {owner, 1, TextAffinity::Downstream, EditorDisplayPositionKind::Source},
    };

    expect(DisplayPositionForSource(
        mapping,
        {owner, 0, TextAffinity::Downstream}) == 2u);
};

"non_boundary_generated_overlays_keep_their_original_affinity_choice"_test = [] {
    const auto owner = NodeId{44};
    const EditorDisplayMapping mapping{
        {owner, 0, TextAffinity::Downstream, EditorDisplayPositionKind::Generated},
        {owner, 0, TextAffinity::Downstream, EditorDisplayPositionKind::Generated},
        {owner, 1, TextAffinity::Downstream, EditorDisplayPositionKind::Source},
    };

    expect(DisplayPositionForSource(
        mapping,
        {owner, 0, TextAffinity::Downstream}) == 0u);
};

"source_display_range_excludes_nested_boundary_decorations"_test = [] {
    const auto parent = NodeId{45};
    const auto code = NodeId{46};
    const EditorDisplayMapping mapping{
        {parent, 0, TextAffinity::Downstream, EditorDisplayPositionKind::BoundaryDecoration},
        {code, 0, TextAffinity::Downstream, EditorDisplayPositionKind::BoundaryDecoration},
        {code, 0, TextAffinity::Downstream, EditorDisplayPositionKind::Source},
        {code, 1, TextAffinity::Downstream, EditorDisplayPositionKind::Source},
        {code, 2, TextAffinity::Downstream, EditorDisplayPositionKind::Source},
        {code, 3, TextAffinity::Upstream, EditorDisplayPositionKind::BoundaryDecoration},
        {parent, 1, TextAffinity::Downstream, EditorDisplayPositionKind::Source},
    };

    auto range = SourceDisplayRangeForContainer(mapping, code, 7);
    expect(range.has_value());
    expect(range->first == 2u);
    expect(range->second == 5u);
};

"source_display_range_respects_visible_character_limit"_test = [] {
    const auto owner = NodeId{47};
    const EditorDisplayMapping mapping{
        {owner, 0, TextAffinity::Downstream, EditorDisplayPositionKind::Source},
        {owner, 1, TextAffinity::Downstream, EditorDisplayPositionKind::Source},
        {owner, 2, TextAffinity::Downstream, EditorDisplayPositionKind::Source},
    };

    auto range = SourceDisplayRangeForContainer(mapping, owner, 2);
    expect(range.has_value());
    expect(range->first == 0u);
    expect(range->second == 2u);
};


"selection intervals agree with scalar selection across owners and mapping kinds"_test = [] {
    std::mt19937 random(98172);
    auto less = [](TextPosition a, TextPosition b) {
        return a.container_id.v < b.container_id.v
            || (a.container_id == b.container_id && a.source_offset < b.source_offset);
    };
    for (int trial = 0; trial < 250; ++trial) {
        EditorDisplayMapping mapping;
        for (std::size_t run = 0; run < 20; ++run) {
            auto owner = NodeId{1 + random() % 4};
            auto offset = random() % 100;
            auto linear = random() % 2 != 0;
            auto kind = static_cast<EditorDisplayPositionKind>(random() % 4);
            auto affinity = random() % 2 ? TextAffinity::Upstream : TextAffinity::Downstream;
            for (std::size_t i = 0, length = 1 + random() % 30; i < length; ++i)
                mapping.push_back({owner, offset + (linear ? i : 0), affinity, kind});
        }
        TextPosition start{NodeId{1 + random() % 4}, random() % 130};
        TextPosition end{NodeId{1 + random() % 4}, random() % 130};
        if (less(end, start)) std::swap(start, end);
        auto actual = DisplayRangesForSourceSelection(mapping, start, end, less);
        std::vector<std::pair<std::size_t, std::size_t>> expected;
        for (std::size_t i = 0; i + 1 < mapping.size(); ++i) {
            if (less(mapping[i], start) || !less(mapping[i], end)) continue;
            if (!expected.empty() && expected.back().second == i) expected.back().second = i + 1;
            else expected.emplace_back(i, i + 1);
        }
        expect(std::ranges::equal(actual, expected));
    }
};

}; // suite display_mapping_tests
