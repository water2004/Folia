#include "support/folia_test.hpp"

import folia.math.output.scene;
import folia.math.output.svg.font_cache;

using namespace boost::ut;
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;

namespace {
scene::Glyph glyph(std::uint32_t codepoint, std::string cacheId = "v1") {
    return {.resource = scene::GlyphResource::make("font", {}),
            .key = {"normal", std::move(cacheId), codepoint}};
}

scene::Scene sceneWithRun(scene::GlyphRun run) {
    scene::Group root;
    root.id = 0;
    root.entries.emplace_back(scene::Primitive{std::move(run)});
    return scene::Scene(std::move(root), {}, {});
}

scene::Scene sceneWithNoGlyphs() {
    scene::Group root;
    root.id = 0;
    return scene::Scene(std::move(root), {}, {});
}

scene::Scene sceneWithRegion(scene::Scene const &tooltip, std::uint64_t order = 1) {
    scene::Group root;
    root.id = 0;
    scene::ActionRegion region{.hitbox = {},
                               .tooltip = std::make_shared<scene::Scene const>(tooltip),
                               .resource_order = order};
    root.entries.emplace_back(std::move(region));
    return scene::Scene(std::move(root), {}, {});
}

scene::ActionRegion const *firstRegion(scene::Scene const &value) {
    for (const auto &entry : value.root().entries)
        if (const auto *region = std::get_if<scene::ActionRegion>(&entry))
            return region;
    return nullptr;
}

std::string referenceFor(scene::Scene const &value, svg::CacheScope const &scope,
                         std::uint32_t codepoint) {
    for (const auto &[glyphPtr, id] : scope.references)
        if (glyphPtr && glyphPtr->key.codepoint == codepoint)
            return id;
    return {};
}

std::size_t definitionCount(svg::CacheScope const &scope, std::string_view id) {
    return static_cast<std::size_t>(std::ranges::count_if(
        scope.definitions, [id](auto const &definition) { return definition.id == id; }));
}
} // namespace

suite native_svg_font_cache_plan = [] {
    "FontCachePlan uses caller resolved prefixes verbatim, including fixed and empty IDs"_test =
        [] {
            auto value = sceneWithRun({{glyph(0x41)}, 1});
            const std::array<std::string, 1> fixed{"fixed-"};
            svg::FontCachePlan fixedPlan(value, fixed);
            expect(referenceFor(value, fixedPlan.scope(0), 0x41) == "MJX-fixed-v1-41");

            const std::array<std::string, 1> empty{std::string{}};
            svg::FontCachePlan emptyPlan(value, empty);
            expect(referenceFor(value, emptyPlan.scope(0), 0x41) == "MJX-v1-41");

            const std::array<std::string, 1> automatic{"3-"};
            svg::FontCachePlan automaticPlan(value, automatic);
            expect(referenceFor(value, automaticPlan.scope(0), 0x41) == "MJX-3-v1-41");
        };

    "FontCachePlan deduplicates same-scope cache IDs but records each glyph reference"_test = [] {
        scene::GlyphRun run{{glyph(0x41), glyph(0x41)}, 1};
        auto value = sceneWithRun(std::move(run));
        const std::array<std::string, 1> prefixes{"same-"};
        svg::FontCachePlan plan(value, prefixes);
        expect(plan.scope(0).references.size() == 2u);
        expect(plan.scope(0).definitions.size() == 1u);
        expect(definitionCount(plan.scope(0), "MJX-same-v1-41") == 1u);
    };

    "FontCachePlan follows sorted nested tooltip order and leaves its cache active afterward"_test =
        [] {
            auto tooltip = sceneWithRun({{glyph(0x42)}, 5});
            auto nested = std::make_shared<scene::Scene const>(tooltip);
            scene::Group root;
            root.id = 0;
            root.entries.emplace_back(scene::Primitive{scene::GlyphRun{{glyph(0x41)}, 10}});
            root.entries.emplace_back(
                scene::ActionRegion{.hitbox = {}, .tooltip = nested, .resource_order = 20});
            root.entries.emplace_back(scene::Primitive{scene::GlyphRun{{glyph(0x43)}, 30}});
            scene::Scene value(std::move(root), {}, {});

            const std::array<std::string, 2> prefixes{"outer-", "tip-"};
            svg::FontCachePlan plan(value, prefixes);
            const auto *region = firstRegion(value);
            expect(region != nullptr);
            expect(svg::FontCachePlan::scopeCount(value) == 2u);
            if (!region)
                return;
            const auto nestedScope = plan.scope(0).tooltips.find(region);
            expect(nestedScope != plan.scope(0).tooltips.end());
            if (nestedScope == plan.scope(0).tooltips.end())
                return;
            expect(nestedScope->second == 1u);
            expect(referenceFor(value, plan.scope(0), 0x41) == "MJX-outer-v1-41");
            expect(referenceFor(value, plan.scope(0), 0x43) == "MJX-v1-43");
            expect(referenceFor(*region->tooltip, plan.scope(1), 0x42) == "MJX-tip-v1-42");
            expect(plan.scope(0).definitions.size() == 1u);
            expect(plan.scope(1).definitions.size() == 2u);
            if (plan.scope(1).definitions.size() >= 2u) {
                expect(plan.scope(1).definitions[0].id == "MJX-tip-v1-42");
                expect(plan.scope(1).definitions[1].id == "MJX-v1-43");
            }
        };

    "FontCachePlan orders resources across groups, not by drawing subtree"_test = [] {
        scene::Group nestedGroup;
        nestedGroup.id = 1;
        nestedGroup.entries.emplace_back(scene::Primitive{scene::GlyphRun{{glyph(0x44)}, 30}});
        auto tooltip = sceneWithRun({{glyph(0x42)}, 1});
        scene::Group root;
        root.id = 0;
        root.entries.emplace_back(std::make_shared<scene::Group const>(std::move(nestedGroup)));
        root.entries.emplace_back(
            scene::ActionRegion{.hitbox = {},
                                .tooltip = std::make_shared<scene::Scene const>(tooltip),
                                .resource_order = 20});
        root.entries.emplace_back(scene::Primitive{scene::GlyphRun{{glyph(0x41)}, 10}});
        scene::Scene value(std::move(root), {}, {});
        const std::array<std::string, 2> prefixes{"root-", "nested-"};
        svg::FontCachePlan plan(value, prefixes);
        const auto *region = firstRegion(value);
        expect(region != nullptr);
        if (!region)
            return;
        const auto nestedScope = plan.scope(0).tooltips.find(region);
        expect(nestedScope != plan.scope(0).tooltips.end());
        if (nestedScope == plan.scope(0).tooltips.end())
            return;
        expect(referenceFor(value, plan.scope(0), 0x41) == "MJX-root-v1-41");
        expect(referenceFor(value, plan.scope(0), 0x44) == "MJX-v1-44");
        expect(plan.scope(0).definitions.size() == 1u);
        if (plan.scope(0).definitions.size() >= 1u)
            expect(plan.scope(0).definitions[0].id == "MJX-root-v1-41");
        expect(plan.scope(nestedScope->second).definitions.size() == 2u)
            << "later outer group glyph is defined in the active nested defs bucket";
        if (plan.scope(nestedScope->second).definitions.size() >= 2u) {
            expect(plan.scope(nestedScope->second).definitions[0].id == "MJX-nested-v1-42");
            expect(plan.scope(nestedScope->second).definitions[1].id == "MJX-v1-44");
        }
    };

    "FontCachePlan allocates a fresh tooltip scope for each shared Scene occurrence"_test = [] {
        auto sharedTooltip = std::make_shared<scene::Scene const>(sceneWithRun({{glyph(0x4A)}, 1}));
        scene::Group root;
        root.id = 0;
        root.entries.emplace_back(
            scene::ActionRegion{.hitbox = {}, .tooltip = sharedTooltip, .resource_order = 10});
        root.entries.emplace_back(
            scene::ActionRegion{.hitbox = {}, .tooltip = sharedTooltip, .resource_order = 20});
        scene::Scene value(std::move(root), {}, {});
        const std::array<std::string, 3> prefixes{"root-", "first-", "second-"};
        svg::FontCachePlan plan(value, prefixes);
        expect(svg::FontCachePlan::scopeCount(value) == 3u);
        expect(plan.scope(0).tooltips.size() == 2u);
        expect(plan.scope(1).definitions.size() == 1u);
        expect(plan.scope(2).definitions.size() == 1u);
        if (!plan.scope(1).definitions.empty())
            expect(plan.scope(1).definitions[0].id == "MJX-first-v1-4A");
        if (!plan.scope(2).definitions.empty())
            expect(plan.scope(2).definitions[0].id == "MJX-second-v1-4A");
    };

    "FontCachePlan accepts empty prefix omission only when the entire tree has no glyphs"_test =
        [] {
            auto empty = sceneWithNoGlyphs();
            const std::array<std::string, 0> noPrefixes{};
            svg::FontCachePlan emptyPlan(empty, noPrefixes);
            expect(emptyPlan.scope(0).definitions.empty());

            auto noGlyphTooltip = sceneWithRegion(sceneWithNoGlyphs());
            expect(svg::FontCachePlan::scopeCount(noGlyphTooltip) == 2u);
            svg::FontCachePlan noGlyphPlan(noGlyphTooltip, noPrefixes);
            expect(noGlyphPlan.scope(0).definitions.empty());
            expect(noGlyphPlan.scope(1).definitions.empty());

            auto hasGlyphs = sceneWithRun({{glyph(0x41)}, 1});
            bool rejected = false;
            try {
                svg::FontCachePlan invalid(hasGlyphs, noPrefixes);
                (void)invalid;
            } catch (std::invalid_argument const &) {
                rejected = true;
            }
            expect(rejected) << "glyph IDs require an explicitly resolved prefix list";
        };

    "FontCachePlan rejects invalid scope prefix counts and missing lookup scopes"_test = [] {
        auto nestedTooltip = sceneWithRegion(sceneWithRun({{glyph(0x42)}, 1}));
        auto value = sceneWithRegion(nestedTooltip);
        const std::array<std::string, 1> tooFew{"root-"};
        bool rejected = false;
        try {
            svg::FontCachePlan invalid(value, tooFew);
            (void)invalid;
        } catch (std::invalid_argument const &) {
            rejected = true;
        }
        expect(rejected);

        const std::array<std::string, 3> exact{"root-", "outer-tip-", "inner-tip-"};
        svg::FontCachePlan plan(value, exact);
        bool missingRejected = false;
        try {
            (void)plan.scope(3);
        } catch (std::out_of_range const &) {
            missingRejected = true;
        }
        expect(missingRejected);
    };
};
