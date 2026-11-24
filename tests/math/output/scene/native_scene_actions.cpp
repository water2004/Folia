#include "support/folia_test.hpp"
import folia.math.output.scene.builder;

namespace {
using namespace boost::ut;
using namespace folia::math::output::scene;

Scene make_tooltip(std::string text) {
    Group root;
    root.entries.emplace_back(Primitive{TextRun{std::move(text), {}, {}, {}, {}}});
    return Scene{std::move(root), {}, {}};
}

suite native_scene_actions = [] {
    "action regions and descriptors freeze owned nested scenes and text"_test = [] {
        auto mutable_tooltip = std::make_shared<Scene>(make_tooltip("original tooltip"));
        std::weak_ptr<Scene> weak_tooltip = mutable_tooltip;
        Builder builder;
        auto group = builder.add_group();
        StatuslineAction source_action{"original status"};
        builder.set_action(group, Action{source_action});
        source_action.text = "caller changed";
        auto const region_index =
            builder.add_action_region(group, ActionRegion{Rule{{0, 0, 2, 1}}, {}});
        builder.set_action_region_tooltip(group, region_index, mutable_tooltip);

        auto scene = std::move(builder).freeze();
        expect(!weak_tooltip.expired());
        auto const &saved_group = *std::get<std::shared_ptr<Group const>>(scene.root().entries[0]);
        if (saved_group.action) {
            auto const *status = std::get_if<StatuslineAction>(&*saved_group.action);
            expect(status != nullptr);
            if (status)
                expect(status->text == "original status");
        } else {
            expect(false);
        }
        expect(saved_group.entries.size() == 1_u);
        auto const *region = std::get_if<ActionRegion>(&saved_group.entries.front());
        expect(region != nullptr);
        if (region && region->tooltip) {
            expect(region->tooltip.get() != mutable_tooltip.get());
            auto const *text =
                std::get_if<TextRun>(&std::get<Primitive>(region->tooltip->root().entries.front()));
            expect(text != nullptr);
            if (text)
                expect(text->utf8 == "original tooltip");
        } else {
            expect(false);
        }

        // Replacing an external mutable alias cannot change the frozen tooltip value.
        *mutable_tooltip = make_tooltip("replacement");
        if (region && region->tooltip) {
            auto const *text =
                std::get_if<TextRun>(&std::get<Primitive>(region->tooltip->root().entries.front()));
            expect(text != nullptr);
            if (text)
                expect(text->utf8 == "original tooltip");
        } else {
            expect(false);
        }
        mutable_tooltip.reset();
        expect(weak_tooltip.expired());
    };

    "action offsets and hitbox dimensions are validated at freeze"_test = [] {
        auto rejects_action = [](Action action) {
            Builder builder;
            auto group = builder.add_group();
            builder.set_action(group, std::move(action));
            try {
                auto scene = std::move(builder).freeze();
            } catch (std::invalid_argument const &) {
                return true;
            }
            return false;
        };
        auto const infinity = std::numeric_limits<double>::infinity();
        expect(!rejects_action(Action{ToggleAction{infinity, 1, false}}));
        expect(!rejects_action(
            Action{ToggleAction{std::numeric_limits<double>::quiet_NaN(), 1, false}}));
        expect(rejects_action(Action{MathTooltipAction{0, infinity}}));
        expect(rejects_action(Action{MathTooltipAction{infinity, 0}}));

        Builder bad_region_builder;
        auto group = bad_region_builder.add_group();
        bad_region_builder.add_action_region(group, ActionRegion{Rule{{0, 0, -1, 1}}, {}});
        bool rejected_region = false;
        try {
            auto scene = std::move(bad_region_builder).freeze();
        } catch (std::invalid_argument const &) {
            rejected_region = true;
        }
        expect(rejected_region);

        Builder invalid_index_builder;
        auto indexed_group = invalid_index_builder.add_group();
        bool rejected_index = false;
        try {
            invalid_index_builder.set_action_region_tooltip(indexed_group, 0, {});
        } catch (std::out_of_range const &) {
            rejected_index = true;
        }
        expect(rejected_index);

        Builder wrong_variant_builder;
        auto wrong_variant_group = wrong_variant_builder.add_group();
        wrong_variant_builder.add_primitive(wrong_variant_group,
                                            TextRun{"not a hitbox", {}, {}, {}, {}});
        bool rejected_variant = false;
        try {
            wrong_variant_builder.set_action_region_tooltip(wrong_variant_group, 0, {});
        } catch (std::invalid_argument const &) {
            rejected_variant = true;
        }
        expect(rejected_variant);

        Builder foreign_builder;
        auto foreign = foreign_builder.add_group(
            0, {}, {}, {}, {}, {}, ElementRole::foreign_content_viewport, {}, {},
            ForeignContent{HtmlNodePrimitive{u"div"}, ForeignObjectGeometry{0, 0, 1, 1, 1}, {}});
        bool action_rejected = false, region_rejected = false;
        try {
            foreign_builder.set_action(foreign, Action{ToggleAction{0, 1, false}});
        } catch (std::invalid_argument const &) {
            action_rejected = true;
        }
        try {
            foreign_builder.add_action_region(foreign, ActionRegion{Rule{{0, 0, 1, 1}}, {}});
        } catch (std::invalid_argument const &) {
            region_rejected = true;
        }
        expect(action_rejected && region_rejected);
    };

    "split preserves group actions and action-region order"_test = [] {
        Builder builder;
        auto shell = builder.add_group();
        auto line_a = builder.add_group(shell);
        auto line_b = builder.add_group(shell);
        builder.set_action(shell, Action{MathTooltipAction{0.25, -0.5, 700, 120}});
        builder.add_action_region(line_a, ActionRegion{Rule{{0, -0.2, 1, 0.3}}, {}});
        builder.add_primitive(line_a, TextRun{"line A", {}, {}, {}, {}});
        builder.add_primitive(line_b, TextRun{"line B", {}, {}, {}, {}});
        std::array<NodeId, 2> sources{line_a, line_b};
        auto roots = builder.split_group_contents(shell, sources);
        expect(roots.size() == 2_u);
        auto scene = std::move(builder).freeze();
        expect(scene.root().entries.size() == 2_u);
        auto const *first = std::get_if<std::shared_ptr<Group const>>(&scene.root().entries[0]);
        auto const *second = std::get_if<std::shared_ptr<Group const>>(&scene.root().entries[1]);
        expect(first != nullptr);
        expect(second != nullptr);
        if (first && second && *first && *second) {
            expect((*first)->action.has_value() && (*second)->action.has_value());
            expect((*first)->entries.size() == 2_u);
            expect(std::holds_alternative<ActionRegion>((*first)->entries[0]));
            expect(std::holds_alternative<Primitive>((*first)->entries[1]));
            expect(std::holds_alternative<Primitive>((*second)->entries[0]));
        }
    };

    "discard releases action regions and their tooltip scene"_test = [] {
        Builder builder;
        auto retained = builder.add_group();
        auto discarded = builder.add_group();
        auto tooltip = std::make_shared<Scene>(make_tooltip("retire me"));
        std::weak_ptr<Scene> weak = tooltip;
        builder.add_action_region(discarded, ActionRegion{Rule{{0, 0, 1, 1}}, tooltip});
        tooltip.reset();
        expect(!weak.expired());
        builder.discard_group(discarded);
        expect(weak.expired());
        expect(builder.child_groups(0) == std::vector<NodeId>{retained});
        auto scene = std::move(builder).freeze();
        expect(scene.root().entries.size() == 1_u);
    };
};
} // namespace
