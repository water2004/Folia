#include "support/folia_test.hpp"

import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.math;
import folia.math.core.mml_tree.mml_nodes.mrow;
import folia.math.core.mml_tree.mml_nodes.mspace;
import folia.math.core.mml_tree.mml_nodes.mtext;
import folia.math.core.mml_tree.mml_nodes.maction;
import folia.math.core.mml_tree.html_adaptor;
import folia.math.fonts.newcm_font_module_provider;
import folia.math.output.common;
import folia.math.output.scene;
import folia.math.output.scene.builder;
import folia.math.output.svg.maction;
import folia.math.output.svg.wrapper_factory;

using namespace boost::ut;
namespace common = folia::math::output::common;
namespace mml = folia::math::mml;
namespace scene = folia::math::output::scene;
namespace svg = folia::math::output::svg;

namespace {
class NoMeasurements final : public common::MeasurementTarget {
  public:
    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        throw std::logic_error("maction fixture supplies metrics");
    }
    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &) override {
        throw std::logic_error("maction fixture uses known geometry");
    }
};

std::unique_ptr<common::Linebreaks> makeLinebreaks(common::CommonWrapperFactory &factory) {
    return std::make_unique<common::LinebreakVisitor>(factory);
}

common::CommonOutputJaxConfig config(common::FontRepository const &fonts) {
    common::ExtendedMetrics metrics{16, 8, 640, 1, "MathJaxNewcm", false};
    return {metrics, {}, fonts, "MathJaxNewcm"};
}

mml::MmlMspace &makeSpace(mml::MmlDocument &document, std::u16string width = u"1em") {
    auto &space = dynamic_cast<mml::MmlMspace &>(document.factory().make(mml::NodeKind::mspace));
    space.attributes().set(u"width", std::move(width));
    space.attributes().set(u"height", u".6em");
    space.attributes().set(u"depth", u".2em");
    return space;
}

scene::Group const *findMaction(scene::Group const &group) {
    if (group.metadata.source_tag == "maction")
        return &group;
    for (const auto &entry : group.entries) {
        if (const auto *child = std::get_if<std::shared_ptr<scene::Group const>>(&entry))
            if (*child)
                if (auto *found = findMaction(**child))
                    return found;
    }
    return nullptr;
}

} // namespace

suite native_svg_maction = [] {
    "SVG maction catalog installs the concrete native owner and Source CSS rules"_test = [] {
        const auto catalog = svg::SvgWrapperFactory::defaultNodes();
        const auto found = std::ranges::find(catalog, mml::NodeKind::maction,
                                             &common::CommonWrapperRegistration::kind);
        expect(found != catalog.end());
        if (found == catalog.end())
            return;
        expect(found->data.styles.size() == 7u)
            << "all Source light and dark class rules are registered";
        expect(found->data.styles.front().name == u"[jax=\"SVG\"] mjx-tool");
        expect(found->data.styles.back().name ==
               u"@media (prefers-color-scheme: dark) /* svg maction */");
    };

    "SVG maction emits selected geometry with a typed toggle action and per-line hitbox"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        NoMeasurements measurement;
        mml::MmlDocument document;
        auto &first = makeSpace(document, u"1em");
        auto &second = makeSpace(document, u"2em");
        mml::AttributeSet attributes;
        attributes.set(u"actiontype", u"toggle");
        attributes.set(u"selection", 2.0);
        const std::array<mml::MmlNode *, 2> actionChildren{&first, &second};
        auto &action =
            document.factory().make(mml::NodeKind::maction, std::move(attributes), actionChildren);
        const std::array<mml::MmlNode *, 1> mathChildren{&action};
        auto &math = document.factory().make(mml::NodeKind::math, {}, mathChildren);
        document.setRoot(math);
        math.setInheritedAttributes();
        math.setTeXclass(nullptr);

        common::LayoutJob job(document, config(bundle.fonts), measurement,
                              svg::SvgWrapperFactory::defaultNodes(), &makeLinebreaks);
        auto result = job.buildScene();
        const auto *maction = findMaction(result.root());
        expect(maction != nullptr);
        if (!maction)
            return;
        expect(maction->action.has_value());
        if (maction->action) {
            const auto *toggle = std::get_if<scene::ToggleAction>(&*maction->action);
            expect(toggle != nullptr);
            if (toggle) {
                expect(toggle->selection == 2.0);
                expect(toggle->child_count == 2u);
                expect(!toggle->enriched);
            }
        }
        std::vector<scene::Rule> hitboxes;
        for (const auto &entry : maction->entries)
            if (const auto *region = std::get_if<scene::ActionRegion>(&entry))
                hitboxes.push_back(region->hitbox);
        expect(hitboxes.size() == 1u);
        if (!hitboxes.empty()) {
            expect(hitboxes[0].bounds.width == 2.0);
            expect(hitboxes[0].bounds.height == .8);
            expect(hitboxes[0].bounds.x == 0.0)
                << "the first own segment uses zero left offset without style dx";
            expect(std::holds_alternative<scene::NoPaint>(hitboxes[0].paint.fill));
        }
        expect(std::ranges::any_of(maction->metadata.ordered_attributes, [](auto const &attribute) {
            return attribute.name == "data-toggle" && attribute.value == "2";
        }));
    };

    "SVG maction text tooltip orders a semantic title before its hitbox"_test = [] {
        const auto bundle = folia::math::fonts::makeNewcmFontData();
        NoMeasurements measurement;
        mml::MmlDocument document;
        auto &selected = makeSpace(document);
        auto &text = dynamic_cast<mml::TextNode &>(document.factory().make(mml::NodeKind::text));
        text.setText(u"tip text");
        const std::array<mml::MmlNode *, 1> textChildren{&text};
        auto &tip = document.factory().make(mml::NodeKind::mtext, {}, textChildren);
        mml::AttributeSet attributes;
        attributes.set(u"actiontype", u"tooltip");
        const std::array<mml::MmlNode *, 2> actionChildren{&selected, &tip};
        auto &action =
            document.factory().make(mml::NodeKind::maction, std::move(attributes), actionChildren);
        const std::array<mml::MmlNode *, 1> mathChildren{&action};
        auto &math = document.factory().make(mml::NodeKind::math, {}, mathChildren);
        document.setRoot(math);
        math.setInheritedAttributes();
        math.setTeXclass(nullptr);

        common::LayoutJob job(document, config(bundle.fonts), measurement,
                              svg::SvgWrapperFactory::defaultNodes(), &makeLinebreaks);
        const auto result = job.buildScene();
        const auto *maction = findMaction(result.root());
        expect(maction != nullptr);
        if (!maction)
            return;
        expect(maction->entries.size() >= 2u);
        if (maction->entries.size() >= 2u) {
            const auto *primitive = std::get_if<scene::Primitive>(&maction->entries[0]);
            const auto *title = primitive ? std::get_if<scene::Title>(primitive) : nullptr;
            const auto *region = std::get_if<scene::ActionRegion>(&maction->entries[1]);
            expect(title != nullptr);
            if (title)
                expect(title->text == "tip text");
            expect(region != nullptr);
            if (region)
                expect(!region->tooltip);
        }
        expect(!maction->action)
            << "text tooltip uses the raw Source title entry without a timer action";
        expect(tip.parent() == &action) << "text tooltip does not alter the MML tree";
    };

    "SVG maction math tooltip freezes an auxiliary Scene and restores the original parent"_test =
        [] {
            const auto bundle = folia::math::fonts::makeNewcmFontData();
            NoMeasurements measurement;
            mml::MmlDocument document;
            auto &selected = makeSpace(document);
            auto &tipContent = makeSpace(document, u"1.5em");
            const std::array<mml::MmlNode *, 1> tipChildren{&tipContent};
            auto &tip = document.factory().make(mml::NodeKind::mrow, {}, tipChildren);
            mml::AttributeSet attributes;
            attributes.set(u"actiontype", u"tooltip");
            attributes.set(u"data-offsets", u".3em .2em");
            const std::array<mml::MmlNode *, 2> actionChildren{&selected, &tip};
            auto &action = document.factory().make(mml::NodeKind::maction, std::move(attributes),
                                                   actionChildren);
            const std::array<mml::MmlNode *, 1> mathChildren{&action};
            auto &math = document.factory().make(mml::NodeKind::math, {}, mathChildren);
            document.setRoot(math);
            math.setInheritedAttributes();
            math.setTeXclass(nullptr);

            common::LayoutJob job(document, config(bundle.fonts), measurement,
                                  svg::SvgWrapperFactory::defaultNodes(), &makeLinebreaks);
            const auto result = job.buildScene();
            const auto *maction = findMaction(result.root());
            expect(maction != nullptr);
            if (!maction)
                return;
            expect(maction->action.has_value());
            if (maction->action) {
                const auto *tooltip = std::get_if<scene::MathTooltipAction>(&*maction->action);
                expect(tooltip != nullptr);
                if (tooltip) {
                    expect(std::abs(tooltip->dx - .3) < 1e-10);
                    expect(std::abs(tooltip->dy - .2) < 1e-10);
                    expect(tooltip->post_delay_ms == 600u);
                    expect(tooltip->clear_delay_ms == 100u);
                }
            }
            const auto region = std::ranges::find_if(maction->entries, [](auto const &entry) {
                return std::holds_alternative<scene::ActionRegion>(entry);
            });
            expect(region != maction->entries.end());
            if (region != maction->entries.end()) {
                const auto &actionRegion = std::get<scene::ActionRegion>(*region);
                expect(actionRegion.tooltip != nullptr);
                if (actionRegion.tooltip)
                    expect(actionRegion.tooltip->metrics().width > 0.0);
            }
            expect(tip.parent() == &action)
                << "auxiliary math traversal restores the tip's original MML parent";
            expect(action.children().size() == 2u);
            if (action.children().size() == 2u)
                expect(action.children()[1] == &tip)
                    << "the original child slot survives nested Scene construction";
            expect(document.root() == &math)
                << "nested Scene construction does not replace the request root";
        };

    "SVG maction restores an auxiliary tooltip parent when nested Scene construction throws"_test =
        [] {
            const auto bundle = folia::math::fonts::makeNewcmFontData();
            NoMeasurements measurement;
            mml::MmlDocument document;
            auto &selected = makeSpace(document);
            auto &tip = document.factory().make(mml::NodeKind::xml);
            mml::AttributeSet attributes;
            attributes.set(u"actiontype", u"tooltip");
            const std::array<mml::MmlNode *, 2> actionChildren{&selected, &tip};
            auto &action = document.factory().make(mml::NodeKind::maction, std::move(attributes),
                                                   actionChildren);
            const std::array<mml::MmlNode *, 1> mathChildren{&action};
            auto &math = document.factory().make(mml::NodeKind::math, {}, mathChildren);
            document.setRoot(math);
            math.setInheritedAttributes();
            math.setTeXclass(nullptr);

            common::LayoutJob job(document, config(bundle.fonts), measurement,
                                  svg::SvgWrapperFactory::defaultNodes(), &makeLinebreaks);
            bool failed = false;
            try {
                (void)job.buildScene();
            } catch (std::logic_error const &) {
                failed = true;
            } catch (std::runtime_error const &) {
                failed = true;
            }
            expect(failed) << "unavailable XML measurement rejects the nested tooltip Scene";
            expect(tip.parent() == &action)
                << "the exception path restores the original MML parent";
            expect(action.children().size() == 2u);
            if (action.children().size() == 2u)
                expect(action.children()[1] == &tip)
                    << "the exception path restores the original child slot";
            expect(document.root() == &math)
                << "an auxiliary failure leaves the request root unchanged";
        };
};
