#include "support/folia_test.hpp"

import folia.core.ids;
import folia.core.selection;
import folia.core.text_edit;
import folia.platform.editor_display_mapping;

using namespace boost::ut;
using namespace folia;
using namespace folia::platform::editor;

suite display_mapping_tests = [] {

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

}; // suite display_mapping_tests
