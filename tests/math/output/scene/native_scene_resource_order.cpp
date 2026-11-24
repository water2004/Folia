#include "support/folia_test.hpp"
import folia.math.output.scene.builder;

namespace {
using namespace boost::ut;
using namespace folia::math::output::scene;

GlyphRun run(std::uint64_t supplied_order = 0) {
    auto resource = GlyphResource::make(
        "resource-order-test",
        GlyphOutline{PathGeometry{{MoveTo{{0, 0}}, LineTo{{1, 1}}, ClosePath{}}}});
    Glyph glyph{resource, {"normal", "resource-order-test", 0x61}, {}, 1, {}};
    return GlyphRun{{std::move(glyph)}, supplied_order};
}

std::uint64_t run_order(Primitive const &primitive) {
    return std::get<GlyphRun>(primitive).resource_order;
}

std::uint64_t only_run_order(Group const &group) {
    for (auto const &entry : group.entries)
        if (auto primitive = std::get_if<Primitive>(&entry))
            if (std::holds_alternative<GlyphRun>(*primitive))
                return run_order(*primitive);
    return 0;
}

Scene tooltip_with_run() {
    Group root;
    root.entries.emplace_back(Primitive{run()});
    return Scene{std::move(root), {}, {}};
}

suite native_scene_resource_order = [] {
    "builder resource order follows generation instead of drawing order"_test = [] {
        Builder builder;
        auto first_drawn = builder.add_group();
        auto second_drawn = builder.add_group();
        builder.add_primitive(second_drawn, Primitive{run(77)});
        builder.insert_primitive(first_drawn, 0, Primitive{run(88)});
        auto scene = std::move(builder).freeze();
        auto const &first = *std::get<std::shared_ptr<Group const>>(scene.root().entries[0]);
        auto const &second = *std::get<std::shared_ptr<Group const>>(scene.root().entries[1]);
        expect(only_run_order(first) == std::uint64_t{2});
        expect(only_run_order(second) == std::uint64_t{1});
    };

    "late tooltip attachment stamps and clearing retires its event order"_test = [] {
        Builder builder;
        auto group = builder.add_group();
        auto const region_index =
            builder.add_action_region(group, ActionRegion{Rule{{0, 0, 1, 1}}, {}});
        builder.add_primitive(group, Primitive{run(99)});
        auto tooltip = std::make_shared<Scene>(tooltip_with_run());
        builder.set_action_region_tooltip(group, region_index, tooltip);
        builder.set_action_region_tooltip(group, region_index, {});
        builder.add_primitive(group, Primitive{run()});
        auto scene = std::move(builder).freeze();
        auto const &saved = *std::get<std::shared_ptr<Group const>>(scene.root().entries[0]);
        auto const *region = std::get_if<ActionRegion>(&saved.entries[0]);
        expect(region != nullptr);
        if (region) {
            expect(!region->tooltip);
            expect(region->resource_order == std::uint64_t{0});
        }
        expect(run_order(std::get<Primitive>(saved.entries[1])) == std::uint64_t{1});
        expect(run_order(std::get<Primitive>(saved.entries[2])) == std::uint64_t{3});
    };

    "split and discard preserve finalized orders with gaps"_test = [] {
        Builder builder;
        auto shell = builder.add_group();
        auto line_a = builder.add_group(shell);
        auto line_b = builder.add_group(shell);
        auto discarded = builder.add_group();
        builder.add_primitive(discarded, Primitive{run()});
        builder.add_primitive(line_b, Primitive{run()});
        auto tip = std::make_shared<Scene>(tooltip_with_run());
        builder.add_action_region(line_a, ActionRegion{Rule{{0, 0, 1, 1}}, tip});
        builder.add_primitive(line_a, Primitive{run()});
        builder.discard_group(discarded);
        std::array<NodeId, 2> sources{line_a, line_b};
        auto ids = builder.split_group_contents(shell, sources);
        expect(ids.size() == 2_u);
        auto scene = std::move(builder).freeze();
        auto const &first = *std::get<std::shared_ptr<Group const>>(scene.root().entries[0]);
        auto const &second = *std::get<std::shared_ptr<Group const>>(scene.root().entries[1]);
        auto const *region = std::get_if<ActionRegion>(&first.entries[0]);
        expect(region != nullptr);
        if (region)
            expect(region->resource_order == std::uint64_t{3});
        expect(run_order(std::get<Primitive>(first.entries[1])) == std::uint64_t{4});
        expect(only_run_order(second) == std::uint64_t{2});
    };

    "direct Scene resource order normalizes omitted values per scope"_test = [] {
        Group root;
        auto child = std::make_shared<Group>();
        child->id = 1;
        auto nested_source = std::make_shared<Scene>(tooltip_with_run());
        child->entries.emplace_back(Primitive{run()});
        child->entries.emplace_back(ActionRegion{Rule{{0, 0, 1, 1}}, nested_source});
        child->entries.emplace_back(ActionRegion{Rule{{0, 0, 1, 1}}, nested_source});
        child->entries.emplace_back(Primitive{run()});
        root.entries.emplace_back(std::shared_ptr<Group const>{child});
        auto scene = Scene{std::move(root), {}, {}};
        auto const &saved = *std::get<std::shared_ptr<Group const>>(scene.root().entries[0]);
        expect(run_order(std::get<Primitive>(saved.entries[0])) == std::uint64_t{1});
        auto const *region = std::get_if<ActionRegion>(&saved.entries[1]);
        auto const *second_region = std::get_if<ActionRegion>(&saved.entries[2]);
        expect(region != nullptr);
        expect(second_region != nullptr);
        if (region && second_region) {
            expect(region->resource_order == std::uint64_t{2});
            expect(second_region->resource_order == std::uint64_t{3});
            expect(region->tooltip != nullptr);
            expect(second_region->tooltip != nullptr);
            if (region->tooltip && second_region->tooltip) {
                expect(region->tooltip.get() != second_region->tooltip.get());
                expect(only_run_order(region->tooltip->root()) == std::uint64_t{1});
                expect(only_run_order(second_region->tooltip->root()) == std::uint64_t{1});
            }
        }
        expect(run_order(std::get<Primitive>(saved.entries[3])) == std::uint64_t{4});
    };

    "direct Scene rejects mixed and duplicate explicit resource orders"_test = [] {
        auto rejects = [](Group root) {
            try {
                auto scene = Scene{std::move(root), {}, {}};
            } catch (std::invalid_argument const &) {
                return true;
            }
            return false;
        };
        Group mixed;
        mixed.entries.emplace_back(Primitive{run(4)});
        mixed.entries.emplace_back(Primitive{run()});
        expect(rejects(std::move(mixed)));

        Group duplicate;
        duplicate.entries.emplace_back(Primitive{run(4)});
        duplicate.entries.emplace_back(Primitive{run(4)});
        expect(rejects(std::move(duplicate)));

        Group empty_runs;
        empty_runs.entries.emplace_back(Primitive{GlyphRun{{}, 4}});
        empty_runs.entries.emplace_back(Primitive{GlyphRun{{}, 0}});
        auto scene = Scene{std::move(empty_runs), {}, {}};
        expect(scene.root().entries.size() == 2_u);
    };
};
} // namespace
