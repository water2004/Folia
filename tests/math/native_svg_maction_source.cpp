#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "support/source_svg_paint.hpp"
#include "support/folia_test.hpp"

import folia.math.core.mml_tree.mml_document;
import folia.math.fonts.newcm_font_module_provider;
import folia.math.output.common;
import folia.math.output.scene;
import folia.math.output.svg.maction;
import folia.math.output.svg.wrapper_factory;
import folia.math.output.svg.writer;

#include "support/mathml_scene.hpp"

using namespace boost::ut;
namespace common = folia::math::output::common;
namespace mml = folia::math::mml;
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;

#include "fixtures/maction_scene_source.hpp"

namespace {
class RejectMeasurement final : public common::MeasurementTarget {
  public:
    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        throw std::logic_error("maction fixture unexpectedly requested font metrics");
    }
    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &) override {
        throw std::logic_error("maction fixture unexpectedly measured unknown text");
    }
};

std::unique_ptr<common::Linebreaks> makeLinebreakVisitor(common::CommonWrapperFactory &factory) {
    return std::make_unique<common::LinebreakVisitor>(factory);
}

bool close(double a, double b) {
    return std::abs(a - b) <= 1e-9;
}

struct FlatStyle {
    std::u16string selector;
    std::u16string property;
    bool numeric{};
    std::u16string text;
    double number{};
};

void flattenStyleRule(folia::math::style_json::StyleEntry const &rule, std::u16string prefix,
                      std::vector<FlatStyle> &out) {
    using folia::math::style_json::StyleEntry;
    auto selector = prefix.empty() ? rule.name : prefix + u" | " + rule.name;
    const auto *children = std::get_if<std::vector<StyleEntry>>(&rule.value);
    if (children) {
        for (auto const &child : *children) {
            if (std::holds_alternative<std::vector<StyleEntry>>(child.value))
                flattenStyleRule(child, selector, out);
            else if (auto const *text = std::get_if<std::u16string>(&child.value))
                out.push_back({selector, child.name, false, *text, 0});
            else if (auto const *number = std::get_if<double>(&child.value))
                out.push_back({selector, child.name, true, {}, *number});
            else
                throw std::logic_error("unsupported Source maction class style value");
        }
    } else {
        throw std::logic_error("Source maction stylesheet root is not a rule");
    }
}

bool sourceStylesMatch(common::CommonWrapperClassData const &data) {
    std::vector<FlatStyle> actual;
    for (auto const &rule : data.styles)
        flattenStyleRule(rule, {}, actual);
    if (actual.size() != std::size(sourceMactionStyles))
        return false;
    for (std::size_t i = 0; i < actual.size(); ++i) {
        auto const &a = actual[i];
        auto const &e = sourceMactionStyles[i];
        if (a.selector != e.selector || a.property != e.property || a.numeric != e.numeric)
            return false;
        if (e.numeric ? !close(a.number, e.number) : a.text != e.text)
            return false;
    }
    return true;
}

struct ActionCounts {
    std::size_t toggle{}, tooltip{}, statusline{}, maction{};
};

void collectActions(scene::Group const &group, ActionCounts &counts) {
    if (group.metadata.source_tag == "maction")
        ++counts.maction;
    if (group.action) {
        if (std::holds_alternative<scene::ToggleAction>(*group.action))
            ++counts.toggle;
        else if (std::holds_alternative<scene::MathTooltipAction>(*group.action))
            ++counts.tooltip;
        else if (std::holds_alternative<scene::StatuslineAction>(*group.action))
            ++counts.statusline;
    }
    for (auto const &entry : group.entries) {
        if (auto const *child = std::get_if<std::shared_ptr<scene::Group const>>(&entry)) {
            if (*child)
                collectActions(**child, counts);
        } else if (auto const *region = std::get_if<scene::ActionRegion>(&entry)) {
            if (region->tooltip)
                collectActions(region->tooltip->root(), counts);
        }
    }
}

struct Captured {
    scene::Scene value;
    std::string before;
    ActionCounts actions;
    bool wrapper_values_match{};
    struct RootBox {
        double w{}, h{}, d{};
    } root_bbox;
};

std::string writeScene(scene::Scene const &value,
                       folia::math::fonts::NewcmFontDataBundle const &bundle,
                       MactionSourceCase const &source) {
    svg::SvgWriteOptions options;
    options.font_resources = &bundle.svg_resources;
    options.font_cache =
        source.font_cache == "local" ? svg::FontCacheMode::local : svg::FontCacheMode::none;
    options.local_cache_prefixes.reserve(source.local_cache_prefixes.size());
    for (const auto prefix : source.local_cache_prefixes)
        options.local_cache_prefixes.emplace_back(prefix);
    return svg::write(value, options);
}

Captured capture(MactionSourceCase const &source,
                 folia::math::fonts::NewcmFontDataBundle const &bundle) {
    std::optional<scene::Scene> completed;
    std::string before;
    ActionCounts actionCounts;
    bool wrappersMatch = true;
    Captured::RootBox rootBox;
    {
        mml::MmlDocument document;
        auto &root = mathml_scene_test::constructRoot(document, source.nodes, source.attributes);
        document.setRoot(root);
        root.setInheritedAttributes();
        RejectMeasurement measurement;
        common::CommonOptions options;
        options.displayOverflow = "linebreak";
        options.linebreaks.inlineBreaks = true;
        common::CommonOutputJaxConfig config{
            {source.em, source.ex, source.width, 1.0, "MathJaxNewcm", source.display},
            std::move(options),
            bundle.fonts,
            "MathJaxNewcm",
            common::InlineBreakPolicy::forced};
        common::LayoutJob job(document, std::move(config), measurement,
                              svg::SvgWrapperFactory::defaultNodes(), makeLinebreakVisitor);
        auto &commonRoot = job.build();
        const auto rootBBox = commonRoot.getBBox();
        rootBox = {rootBBox.w, rootBBox.h, rootBBox.d};

        std::vector<mml::MmlNode *> flattenedNodes;
        std::vector<std::size_t> actionIndices;
        const auto visit = [&](auto &&self, mml::MmlNode &node) -> void {
            const auto index = flattenedNodes.size();
            flattenedNodes.push_back(&node);
            if (node.kind() == mml::NodeKind::maction)
                actionIndices.push_back(index);
            for (auto *child : node.children())
                if (child)
                    self(self, *child);
        };
        visit(visit, root);
        if (actionIndices.size() != source.wrapper_count ||
            flattenedNodes.size() != source.nodes.size())
            wrappersMatch = false;
        for (std::size_t i = 0; i < source.wrapper_count; ++i) {
            const auto index = source.wrappers[i].node_index;
            if (index >= flattenedNodes.size() ||
                flattenedNodes[index]->kind() != mml::NodeKind::maction) {
                wrappersMatch = false;
                continue;
            }
            auto *base = job.jax().wrapperFor(*flattenedNodes[index]);
            auto *action = dynamic_cast<common::CommonMaction *>(base);
            if (!action) {
                wrappersMatch = false;
                continue;
            }
            const auto box = action->getBBox();
            auto const &expected = source.wrappers[i];
            auto *selected = action->selected();
            wrappersMatch = wrappersMatch && selected &&
                            mml::nodeKindName(selected->node->kind()) == expected.selected_kind;
            wrappersMatch = wrappersMatch && close(action->tipDx(), expected.tip_dx) &&
                            close(action->tipDy(), expected.tip_dy);
            wrappersMatch = wrappersMatch && close(box.w, expected.w) && close(box.h, expected.h) &&
                            close(box.d, expected.d) && close(box.scale, expected.scale) &&
                            close(box.rscale, expected.rscale) && close(box.L, expected.L) &&
                            close(box.R, expected.R) && box.pwidth == expected.pwidth;
        }

        auto value = job.jax().buildScene(commonRoot);
        collectActions(value.root(), actionCounts);
        before = writeScene(value, bundle, source);
        completed.emplace(std::move(value));
    }
    return {std::move(*completed), std::move(before), actionCounts, wrappersMatch, rootBox};
}

void reportDifference(std::string_view name, std::string_view expected, std::string_view actual) {
    std::size_t first = 0;
    while (first < expected.size() && first < actual.size() && expected[first] == actual[first])
        ++first;
    const auto begin = first > 48 ? first - 48 : 0;
    std::cerr << "maction SVG mismatch case=" << name << " first_byte=" << first
              << " expected_size=" << expected.size() << " actual_size=" << actual.size()
              << "\nexpected: " << expected.substr(begin, 112)
              << "\nactual:   " << actual.substr(begin, 112) << '\n';
}

bool sourceListenerShapeValid(MactionSourceCase const &source) {
    std::size_t over = 0, out = 0;
    for (std::size_t i = 0; i < source.listener_count; ++i) {
        auto const &listener = source.listeners[i];
        if (!listener.target || !listener.host || listener.before != listener.after)
            return false;
        if (listener.type == "click")
            continue;
        else if (listener.type == "mouseover")
            ++over;
        else if (listener.type == "mouseout")
            ++out;
        else
            return false;
    }
    return over == out;
}
} // namespace

suite native_svg_maction_source = [] {
    "pinned_MathML_maction_Source_cases_build_real_Scenes_and_survive_retirement"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        const auto catalog = svg::SvgWrapperFactory::defaultNodes();
        auto registration = std::find_if(catalog.begin(), catalog.end(), [](auto const &entry) {
            return entry.kind == mml::NodeKind::maction;
        });
        expect(bool(registration != catalog.end()))
            << "SVG catalog has native maction registration";
        if (registration == catalog.end())
            return;
        expect(bool(sourceStylesMatch(registration->data)))
            << "native maction registration matches typed pinned Source styles";

        for (auto const &source : sourceMactionCases) {
            const auto captured = capture(source, bundle);
            const bool sourceListenersValid = sourceListenerShapeValid(source);
            expect(bool(sourceListenersValid))
                << "Source listener descriptor shape case=" << source.name;
            expect(bool(captured.wrapper_values_match))
                << "CommonMaction observations match actual Source wrappers case=" << source.name;
            std::size_t sourceOutputGroups = 0;
            for (std::size_t offset = 0;
                 (offset = source.svg.find("data-mml-node=\"maction\"", offset)) !=
                 std::string_view::npos;
                 offset += 23)
                ++sourceOutputGroups;
            expect(captured.actions.maction == sourceOutputGroups)
                << "Scene contains each actual Source Maction output segment case=" << source.name;
            expect(bool(close(captured.root_bbox.w, source.root_w) &&
                        close(captured.root_bbox.h, source.root_h) &&
                        close(captured.root_bbox.d, source.root_d)))
                << "root Math BBox is compared separately from maction BBox case=" << source.name;
            if (source.font_cache == "local")
                expect(!source.local_cache_prefixes.empty())
                    << "local Source case carries actual resolved prefixes case=" << source.name;
            else
                expect(source.local_cache_prefixes.empty())
                    << "none Source case has no local cache prefixes case=" << source.name;

            const std::size_t expectedClick = [&] {
                std::size_t count = 0;
                for (std::size_t i = 0; i < source.listener_count; ++i)
                    if (source.listeners[i].type == "click")
                        ++count;
                return count;
            }();
            std::size_t sourceOver = 0, sourceOut = 0;
            for (std::size_t i = 0; i < source.listener_count; ++i) {
                sourceOver += source.listeners[i].type == "mouseover";
                sourceOut += source.listeners[i].type == "mouseout";
            }
            const auto nativeOver = captured.actions.tooltip + captured.actions.statusline;
            expect(expectedClick == captured.actions.toggle)
                << "passive click registrations correspond to typed ToggleAction case="
                << source.name;
            expect(sourceOver == nativeOver && sourceOut == nativeOver)
                << "passive tooltip/status registrations correspond to typed actions case="
                << source.name;

            const auto after = writeScene(captured.value, bundle, source);
            const auto expectedSvg = source_svg_paint::materialize(source.svg);
            if (captured.before != expectedSvg)
                reportDifference(source.name, expectedSvg, captured.before);
            expect(bool(captured.before == expectedSvg))
                << "pinned Source SVG bytes match case=" << source.name;
            expect(bool(after == expectedSvg))
                << "Scene serializes to pinned SVG after MML document/job retirement case="
                << source.name;
            std::cout << source.name << ": "
                      << (captured.before == expectedSvg ? "SVG byte-equal" : "SVG mismatch")
                      << '\n';
        }
    };
};
