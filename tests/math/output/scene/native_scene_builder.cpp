#include "support/folia_test.hpp"
import folia.math.output.scene.builder;
import folia.math.output.scene.html_snapshot;
import folia.math.adaptors.lite_adaptor;

namespace {
using namespace boost::ut;
using namespace folia::math::output::scene;

suite native_scene_builder = [] {
    "table builder geometry edits stay typed and reparenting validates ancestry"_test = [] {
        Builder background_builder;
        auto background = background_builder.add_group();
        SemanticMetadata background_metadata;
        background_metadata.ordered_attributes.push_back({"data-bgcolor", "red"});
        Rect const original{0.1, -0.2, 1.3, 0.4};
        background_builder.add_primitive(
            background, Rule{original, {}, background_metadata, ElementRole::background_box});
        expect(background_builder.background_bounds(background) == original);
        Rect const adjusted{0.2, -0.3, 1.4, 0.5};
        expect(background_builder.set_background_bounds(background, adjusted));
        expect(background_builder.set_background_width(background, 2.25));
        auto resized = adjusted;
        resized.width = 2.25;
        expect(background_builder.background_bounds(background) == resized);
        auto no_background = background_builder.add_group();
        expect(!background_builder.set_background_bounds(no_background, adjusted));
        bool invalid_bounds = false;
        try {
            (void)background_builder.set_background_bounds(background, Rect{0, 0, -1, 1});
        } catch (std::invalid_argument const &) {
            invalid_bounds = true;
        }
        expect(invalid_bounds);

        Builder width_builder;
        auto width_group = width_builder.add_group();
        Path width_path;
        width_path.role = ElementRole::background_box;
        width_builder.add_primitive(width_group, width_path);
        expect(width_builder.set_background_width(width_group, 3.049));
        auto non_background_first = width_builder.add_group();
        width_builder.add_primitive(non_background_first, Path{});
        Path later_background;
        later_background.role = ElementRole::background_box;
        width_builder.add_primitive(non_background_first, later_background);
        expect(!width_builder.set_background_width(non_background_first, 9));
        bool negative_width = false;
        try {
            (void)width_builder.set_background_width(width_group, -1);
        } catch (std::invalid_argument const &) {
            negative_width = true;
        }
        bool nonfinite_width = false;
        try {
            (void)width_builder.set_background_width(
                width_group, std::numeric_limits<Unit>::quiet_NaN());
        } catch (std::invalid_argument const &) {
            nonfinite_width = true;
        }
        expect(negative_width && nonfinite_width);
        auto frozen_width_scene = std::move(width_builder).freeze();
        auto const &frozen_width_group =
            *std::get<std::shared_ptr<Group const>>(frozen_width_scene.root().entries.front());
        expect(std::get<Path>(std::get<Primitive>(frozen_width_group.entries.front())).layout_width ==
               3.049);

        Path owned_width_path;
        owned_width_path.layout_width = 2.75;
        Group owned_width_root;
        owned_width_root.entries.emplace_back(Primitive{owned_width_path});
        Scene owned_width_scene{owned_width_root, {}, {}};
        std::get<Path>(std::get<Primitive>(owned_width_root.entries.front())).layout_width = 8;
        expect(std::get<Path>(std::get<Primitive>(owned_width_scene.root().entries.front()))
                   .layout_width == 2.75);
        auto rejects_layout_width = [](Unit invalid_width) {
            Path invalid_path;
            invalid_path.layout_width = invalid_width;
            Group invalid_root;
            invalid_root.entries.emplace_back(Primitive{std::move(invalid_path)});
            try {
                Scene invalid_scene{std::move(invalid_root), {}, {}};
            } catch (std::invalid_argument const &) {
                return true;
            }
            return false;
        };
        expect(rejects_layout_width(-1));
        expect(rejects_layout_width(std::numeric_limits<Unit>::quiet_NaN()));

        Builder builder;
        auto owner = builder.add_group();
        auto child = builder.add_group(owner);
        auto destination = builder.add_group();
        builder.add_primitive(child, TextRun{"owned"});
        auto viewport =
            builder.add_group(0, {}, {}, {}, {}, {}, ElementRole::table_content_viewport, {},
                              Viewport{{0, 0, 1, 1}, {0, 0, 1, 1}, {}, false});
        Rect const completed{0.4, -0.7, 2.2, 1.1};
        builder.set_viewport_bounds(ElementRole::table_content_viewport, completed);
        builder.reparent_group(child, destination);
        expect(builder.child_groups(owner).empty());
        expect(builder.child_groups(destination) == std::vector<NodeId>{child});
        bool cycle = false;
        try {
            builder.reparent_group(destination, child);
        } catch (std::invalid_argument const &) {
            cycle = true;
        }
        bool root = false;
        try {
            builder.reparent_group(0, destination);
        } catch (std::invalid_argument const &) {
            root = true;
        }
        bool missing = false;
        try {
            builder.reparent_group(999, destination);
        } catch (std::out_of_range const &) {
            missing = true;
        }
        expect(cycle && root && missing);
        auto frozen = std::move(builder).freeze();
        auto const find_group = [&](auto const &self, Group const &group,
                                    NodeId target) -> Group const * {
            if (group.id == target)
                return &group;
            for (auto const &entry : group.entries)
                if (auto const *nested = std::get_if<std::shared_ptr<Group const>>(&entry);
                    nested && *nested)
                    if (auto const *found = self(self, **nested, target))
                        return found;
            return nullptr;
        };
        auto const *viewport_group = find_group(find_group, frozen.root(), viewport);
        auto const *saved_child = find_group(find_group, frozen.root(), child);
        expect(viewport_group && viewport_group->viewport->bounds == completed);
        expect(viewport_group && !viewport_group->viewport->clip_content);
        expect(saved_child &&
               std::get<TextRun>(std::get<Primitive>(saved_child->entries.front())).utf8 ==
                   "owned");
    };

    "builder edits existing group values and wraps contents in place"_test = [] {
        Builder builder;
        auto id = builder.add_group();
        builder.add_primitive(id, TextRun{"a", {}, {}, {}, {}});
        builder.add_primitive(id, TextRun{"b", {}, {}, {}, {}});
        builder.set_transform(id, Transform{std::vector<TransformOperation>{Translation{2, 3}}});
        SemanticMetadata metadata;
        metadata.math_role = "row";
        builder.set_metadata(id, metadata);
        Paint paint;
        paint.opacity = 0.5;
        builder.set_paint(id, paint);
        expect(builder.transform(id).matrix().e == 2.0_d);
        expect(builder.metadata(id).math_role == "row");
        expect(builder.paint(id).opacity == 0.5_d);
        builder.insert_primitive(id, 1, TextRun{"middle", {}, {}, {}, {}});
        expect(builder.entry_count(id) == 3_u);
        auto wrapped =
            builder.wrap_contents(id, Transform{std::vector<TransformOperation>{Translation{0, 1}}},
                                  {}, ElementRole::anonymous_group);
        expect(builder.entry_count(id) == 1_u);
        expect(builder.entry_count(wrapped) == 3_u);
        auto scene = std::move(builder).freeze();
        auto const &group = *std::get<std::shared_ptr<Group const>>(scene.root().entries.front());
        expect(group.id == id);
        expect(group.entries.size() == 1_u);
        auto const &wrapper = *std::get<std::shared_ptr<Group const>>(group.entries.front());
        expect(wrapper.id == wrapped);
        expect(wrapper.entries.size() == 3_u);
        expect(std::get<TextRun>(std::get<Primitive>(wrapper.entries[1])).utf8 == "middle");
    };

    "builder reports moved from and invalid ids or insertion positions"_test = [] {
        Builder builder;
        bool invalid_id = false, invalid_index = false;
        try {
            (void)builder.entry_count(999);
        } catch (std::out_of_range const &) {
            invalid_id = true;
        }
        try {
            builder.insert_primitive(0, 1, TextRun{});
        } catch (std::out_of_range const &) {
            invalid_index = true;
        }
        expect(invalid_id && invalid_index);
        Builder moved{std::move(builder)};
        bool moved_from = false;
        try {
            (void)builder.display_layout();
        } catch (std::logic_error const &) {
            moved_from = true;
        }
        expect(moved_from);
        expect(moved.display_layout().alignment == "center");
    };

    "discard group removes its subtree and releases owned values in place"_test = [] {
        Builder builder;
        auto left = builder.add_group();
        builder.add_primitive(left, TextRun{"left", {}, {}, {}, {}});
        auto discarded = builder.add_group();
        auto child = builder.add_group(discarded);
        auto grandchild = builder.add_group(child);
        std::weak_ptr<GlyphResource const> discarded_resource;
        {
            auto resource = GlyphResource::make(
                "temporary", GlyphOutline{PathGeometry{{MoveTo{{0, 0}}, LineTo{{1, 1}}}}});
            discarded_resource = resource;
            builder.add_primitive(
                grandchild, GlyphRun{{Glyph{resource, {"normal", "temporary", 0x61}, {}, 1, {}}}});
        }
        auto right = builder.add_group();
        builder.add_primitive(right, TextRun{"right", {}, {}, {}, {}});
        auto const last_allocated = right;
        expect(!discarded_resource.expired());
        expect(builder.child_groups(0) == (std::vector<NodeId>{left, discarded, right}));

        builder.discard_group(discarded);
        expect(discarded_resource.expired());
        expect(builder.child_groups(0) == (std::vector<NodeId>{left, right}));
        expect(builder.entry_count(left) == 1_u && builder.entry_count(right) == 1_u);
        for (auto removed : {discarded, child, grandchild}) {
            bool missing = false;
            try {
                (void)builder.entry_count(removed);
            } catch (std::out_of_range const &) {
                missing = true;
            }
            expect(missing);
        }

        auto const next = builder.add_group();
        expect(next > last_allocated);
        builder.discard_group(next);
        auto scene = std::move(builder).freeze();
        expect(scene.root().entries.size() == 2_u);
        auto const &left_group = *std::get<std::shared_ptr<Group const>>(scene.root().entries[0]);
        auto const &right_group = *std::get<std::shared_ptr<Group const>>(scene.root().entries[1]);
        expect(left_group.id == left && right_group.id == right);
        expect(std::get<TextRun>(std::get<Primitive>(left_group.entries[0])).utf8 == "left");
        expect(std::get<TextRun>(std::get<Primitive>(right_group.entries[0])).utf8 == "right");
    };

    "discard group rejects root and unknown ids without changing the builder"_test = [] {
        Builder builder;
        auto keep = builder.add_group();
        auto drop = builder.add_group();
        auto child = builder.add_group(drop);
        bool root_rejected = false;
        bool unknown_rejected = false;
        try {
            builder.discard_group(0);
        } catch (std::invalid_argument const &) {
            root_rejected = true;
        }
        try {
            builder.discard_group(999);
        } catch (std::out_of_range const &) {
            unknown_rejected = true;
        }
        expect(root_rejected && unknown_rejected);
        expect(builder.child_groups(0) == (std::vector<NodeId>{keep, drop}));
        expect(builder.child_groups(drop) == std::vector<NodeId>{child});
        builder.add_primitive(keep, TextRun{"kept", {}, {}, {}, {}});
        builder.discard_group(drop);
        expect(builder.child_groups(0) == std::vector<NodeId>{keep});
        expect(builder.entry_count(keep) == 1_u);
    };

    "split group contents atomically extracts descendant drawing ids in order"_test = [] {
        Builder builder;
        Viewport viewport{{0.1, -0.2, 1, 2}, {-0.3, -0.4, 2, 3}, {}};
        auto shell =
            builder.add_group(0, Transform{std::vector<TransformOperation>{Translation{0.1, 0.2}}},
                              {}, {}, {}, {}, ElementRole::math_wrapper, {}, viewport);
        auto row = builder.add_group(shell);
        auto line_a = builder.add_group(row);
        auto drawing_a = builder.add_group(line_a);
        builder.add_primitive(drawing_a, TextRun{"A", {}, {0.4, 0.5, 0.1}, {}, {}});
        auto line_b = builder.add_group(row);
        auto drawing_b = builder.add_group(line_b);
        builder.add_primitive(drawing_b, TextRun{"B", {}, {0.3, 0.4, 0.1}, {}, {}});
        std::array<NodeId, 2> sources{line_a, line_b};
        auto roots = builder.split_group_contents(shell, sources);
        expect(roots.size() == 2_u);
        expect(roots[0] == shell);
        expect(roots[1] != shell);
        expect(builder.child_groups(0) == roots);
        expect(builder.child_groups(roots[0]) == std::vector<NodeId>{drawing_a});
        expect(builder.child_groups(roots[1]) == std::vector<NodeId>{drawing_b});
        builder.set_frame_layout(
            {0.2,
             {{roots[0], {0.4, 0.5, 0.1}, {}},
              {roots[1], {0.3, 0.4, 0.1}, InlineBreak{InlineBreakKind::opportunity, 0}}}});
        auto scene = std::move(builder).freeze();
        expect(scene.frame_layout().inline_fragments.size() == 2_u);
        expect(scene.frame_layout().inline_fragments[0].root == roots[0]);
        expect(scene.frame_layout().inline_fragments[1].root == roots[1]);
        auto const &first_shell = *std::get<std::shared_ptr<Group const>>(scene.root().entries[0]);
        auto const &second_shell = *std::get<std::shared_ptr<Group const>>(scene.root().entries[1]);
        expect(first_shell.viewport && first_shell.viewport->bounds == viewport.bounds);
        expect(second_shell.viewport && second_shell.viewport->view_box == viewport.view_box);
        expect(first_shell.id == shell && second_shell.id == roots[1]);
        expect(first_shell.entries.size() == 1_u && second_shell.entries.size() == 1_u);
        auto const &first_drawing = *std::get<std::shared_ptr<Group const>>(first_shell.entries[0]);
        auto const &second_drawing =
            *std::get<std::shared_ptr<Group const>>(second_shell.entries[0]);
        expect(first_drawing.id == drawing_a && second_drawing.id == drawing_b);
        expect(std::get<TextRun>(std::get<Primitive>(first_drawing.entries[0])).utf8 == "A");
        expect(std::get<TextRun>(std::get<Primitive>(second_drawing.entries[0])).utf8 == "B");
    };

    "split group contents rejects invalid ancestry and overlaps without mutation"_test = [] {
        Builder builder;
        auto shell = builder.add_group(0);
        auto line = builder.add_group(shell);
        auto drawing = builder.add_group(line);
        builder.add_primitive(drawing, TextRun{"kept", {}, {}, {}, {}});
        auto outside = builder.add_group(0);
        bool missing = false, wrong_ancestry = false, overlap = false;
        std::array<NodeId, 1> bad_id{999};
        try {
            (void)builder.split_group_contents(shell, bad_id);
        } catch (std::out_of_range const &) {
            missing = true;
        }
        std::array<NodeId, 1> wrong{outside};
        try {
            (void)builder.split_group_contents(shell, wrong);
        } catch (std::invalid_argument const &) {
            wrong_ancestry = true;
        }
        std::array<NodeId, 2> overlapping{line, drawing};
        try {
            (void)builder.split_group_contents(shell, overlapping);
        } catch (std::invalid_argument const &) {
            overlap = true;
        }
        expect(missing && wrong_ancestry && overlap);
        expect(builder.child_groups(0) == (std::vector<NodeId>{shell, outside}));
        expect(builder.child_groups(shell) == std::vector<NodeId>{line});
        expect(builder.child_groups(line) == std::vector<NodeId>{drawing});
        expect(builder.entry_count(drawing) == 1_u);
        builder.add_primitive(drawing, TextRun{"still usable", {}, {}, {}, {}});
        expect(builder.entry_count(drawing) == 2_u);
    };

    "scene rejects a cyclic shared group before recursive copying"_test = [] {
        Group root;
        auto cycle = std::make_shared<Group>();
        cycle->id = 1;
        cycle->entries.emplace_back(std::shared_ptr<Group const>{cycle});
        root.entries.emplace_back(std::shared_ptr<Group const>{cycle});
        bool threw = false;
        try {
            Scene scene{std::move(root), Metrics{}, FontMetrics{}};
        } catch (std::invalid_argument const &) {
            threw = true;
        }
        expect(threw);
        cycle->entries.clear(); // break the deliberately constructed shared_ptr cycle
    };


};
} // namespace
