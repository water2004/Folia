#include "support/folia_test.hpp"
import folia.math.output.scene.builder;
import folia.math.output.svg.writer;

namespace {
using namespace boost::ut;
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;

std::string serialized_transform(scene::ElementRole role,
    std::vector<scene::TransformOperation> operations) {
    scene::Builder builder;
    builder.set_metrics({1, 0.5, 0.1});
    builder.set_font_metrics({1, 0.5, 0.5, 0.7});
    (void)builder.add_group(0, scene::Transform{std::move(operations)}, {}, {}, {}, {}, role);
    const auto output = svg::write(std::move(builder).freeze(),
        {.font_cache = svg::FontCacheMode::none});
    const auto first = output.find("transform=\"");
    const auto start = first == output.npos ? output.npos : output.find("transform=\"", first + 1);
    if (start == output.npos) return {};
    const auto value_start = start + std::string_view{"transform=\""}.size();
    return output.substr(value_start, output.find('"', value_start) - value_start);
}
}

suite native_scene_element_roles = [] {
    "builder freeze preserves explicit and default Scene element roles"_test = [] {
        scene::Builder builder;
        const auto explicit_id = builder.add_group(0, {}, {}, {}, {}, {},
            scene::ElementRole::enclosure_content);
        const auto default_id = builder.add_group();
        auto frozen = std::move(builder).freeze();
        auto const& root = frozen.root();
        const auto& explicit_group = *std::get<std::shared_ptr<scene::Group const>>(root.entries[0]);
        const auto& default_group = *std::get<std::shared_ptr<scene::Group const>>(root.entries[1]);
        expect(explicit_group.id == explicit_id);
        expect(explicit_group.role == scene::ElementRole::enclosure_content);
        expect(default_group.id == default_id);
        expect(default_group.role == scene::ElementRole::math_wrapper);
    };

    "role separators preserve observed ordered Source transform strings"_test = [] {
        // MathJax 4.1.3 fixture, continuous/1 mathbb: Wrapper.place plus scale.
        expect(serialized_transform(scene::ElementRole::math_wrapper,
            {scene::Translation{0.719, 0.363}, scene::Scaling{0.707, 0.707}}) ==
            "translate(719,363) scale(0.707)");

        // Fixture fresh-session-per-case/53 textcomp-symbols: text placement
        // prepended before the intentional local inverse text reflection.
        expect(serialized_transform(scene::ElementRole::unknown_text,
            {scene::Translation{0.375, 0}, scene::Scaling{1, -1}}) ==
            "translate(375,0) scale(1,-1)");

        // Fixture continuous/21 amscd-diagram: inferred-row adjustment.
        expect(serialized_transform(scene::ElementRole::inferred_row_adjustment,
            {scene::Translation{0, 0.1315}}) == "translate(0 131.5)");

        // Fixture continuous/29 cancel-lines: menclose child-content grammar.
        expect(serialized_transform(scene::ElementRole::enclosure_content,
            {scene::Translation{0.267, 0}}) == "translate(267, 0)");

        // Fixture continuous/29 cancel-lines: rotate centered on the arrow.
        expect(serialized_transform(scene::ElementRole::enclosure_mark,
            {scene::Rotation{40.575, {0.519, 0.2155}}}) ==
            "rotate(40.575 519 215.5)");

        // Fixture continuous/20 ams-align-tag: sequence, zero components and
        // the reflection matrix remain distinct operations in original order.
        expect(serialized_transform(scene::ElementRole::table_content_viewport,
            {scene::Translation{0, 1.4}, scene::Affine{1, 0, 0, -1, 0, 0},
                scene::Scaling{55.25, 55.25}}) ==
            "translate(0 1400) matrix(1 0 0 -1 0 0) scale(55.25)");

        // Same fixture's table label frame uses the Source matrix spelling.
        expect(serialized_transform(scene::ElementRole::table_label_frame,
            {scene::Affine{1, 0, 0, -1, 0, 0}}) == "matrix(1 0 0 -1 0 0)");

        // Fixture continuous/52 texhtml-explicitly-enabled: scale precedes
        // reflection in the foreign viewport. Foreign payload remains outside
        // this role-only packet's implemented serializer scope.
        expect(serialized_transform(scene::ElementRole::foreign_content_viewport,
            {scene::Scaling{62.5, 62.5}, scene::Affine{1, 0, 0, -1, 0, 0}}) ==
            "scale(62.5) matrix(1 0 0 -1 0 0)");
    };
};
