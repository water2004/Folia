// Candidate UT for the Source-mapped CommonMo family implementation.
#include "support/folia_test.hpp"
import folia.math.output.common;
import folia.math.core.mml_tree.mml_nodes.math;
import folia.math.core.mml_tree.mml_nodes.mo;
import folia.math.core.mml_tree.mml_nodes.mrow;

namespace {
using namespace boost::ut;
namespace mml = folia::math::mml;
namespace common = folia::math::output::common;
namespace scene = folia::math::output::scene;

struct RejectUnexpectedMeasurement final : common::MeasurementTarget {
    common::ExtendedMetrics measureMetrics(common::MetricRequest const &) override {
        throw std::logic_error("CommonMo focused test did not request metrics");
    }

    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &) override {
        throw std::logic_error("CommonMo focused test did not measure unknown text");
    }
};

struct RejectUnexpectedLinebreaks final : common::Linebreaks {
    using Linebreaks::Linebreaks;

    void breakToWidth(common::CommonWrapper &, double) override {
        throw std::logic_error("CommonMo focused test did not request algorithmic line breaking");
    }
};

std::unique_ptr<common::Linebreaks> makeLinebreaks(common::CommonWrapperFactory &factory) {
    return std::make_unique<RejectUnexpectedLinebreaks>(factory);
}

template <class Wrapper> common::CommonWrapperRegistration registration(mml::NodeKind kind) {
    return {
        kind,
        {[](common::WrapperBuildArguments const &args) -> std::unique_ptr<common::CommonWrapper> {
             return std::make_unique<Wrapper>(args.factory, args.node, args.parent);
         },
         [](common::CommonWrapper const &wrapper) {
             return dynamic_cast<Wrapper const *>(&wrapper) != nullptr;
         }},
        common::CommonWrapperClassData::commonDefaults()};
}

common::CommonWrapperCatalog catalog() {
    return {registration<common::CommonMo>(mml::NodeKind::mo),
            registration<common::CommonWrapper>(mml::NodeKind::text),
            registration<common::CommonWrapper>(mml::NodeKind::math),
            registration<common::CommonInferredMrow>(mml::NodeKind::inferred_mrow),
            registration<common::CommonWrapper>(mml::NodeKind::mrow)};
}

class OperatorCreatorProbe final : public common::CommonMo {
  public:
    using CommonMo::CommonMo;
    using CommonWrapper::createMo;
};

auto makeRepository() {
    common::FontRepositoryBuilder builder;
    common::FontFaceData face;
    face.name = "test-face";
    face.cssFamilyPrefix = "MathJax_";
    face.cssFontMap.emplace("normal", common::CssFontData{"MathJax_Main", false, false});
    face.size_variants = {"size-0", "size-1", "size-2"};
    face.stretch_variants = {"stretch-0"};
    face.parameters.values = {{"x_height", .5},         {"axis_height", .25},
                              {"delimiterfactor", 900}, {"delimitershortfall", .3},
                              {"extender_factor", .5},  {"nulldelimiterspace", .12}};
    common::FontVariant normal;
    normal.name = "normal";
    normal.cache_id = "test-cache";
    const std::array<std::pair<std::uint32_t, double>, 3> sceneGlyphs{
        {{0x3E, .778}, {0x3C, .778}, {0x62, .6}}};
    for (const auto &[codepoint, width] : sceneGlyphs) {
        scene::PathGeometry outline;
        outline.segments = {scene::MoveTo{{0, 0}}, scene::LineTo{{.5, .5}}, scene::LineTo{{0, .5}},
                            scene::ClosePath{}};
        common::FontGlyph glyph;
        glyph.metrics = {.7, .2, width};
        glyph.path_defined = true;
        glyph.resource =
            scene::GlyphResource::make("test-face", scene::GlyphOutline{std::move(outline)});
        normal.glyphs.emplace(codepoint, std::move(glyph));
    }
    face.variants.emplace("normal", std::move(normal));
    face.operatorRemap.emplace(0x2B, u"⊕");
    face.accentRemap.emplace(0x5E, u"ˆ");
    builder.addFace(std::move(face));

    common::DelimiterRecipe recipe;
    recipe.direction = common::StretchDirection::vertical;
    recipe.sizes = {.5, 1, 1.5};
    recipe.variants = {0, 1, 2};
    recipe.schar = {0x239A, 0x239B, 0x239B};
    recipe.stretch = {0x239B, 0x239C, 0x239D};
    recipe.stretchv = {0, 0, 0};
    recipe.c = 0x28;
    recipe.ext = 1;
    recipe.min = 0;
    recipe.heightDepth = std::array<double, 2>{.75, .25};
    recipe.heightDepthWidth = std::array<double, 3>{.75, .25, .8};
    recipe.fullExtent = std::array<double, 2>{1, 2};
    builder.addDelimiter("test-face", 0x28, std::move(recipe));
    return std::move(builder).freeze();
}

mml::MmlMo &makeMo(mml::MmlDocument &document, std::u16string text) {
    auto &mo = dynamic_cast<mml::MmlMo &>(document.factory().make(mml::NodeKind::mo));
    auto &token = dynamic_cast<mml::TextNode &>(document.factory().make(mml::NodeKind::text));
    token.setText(std::move(text));
    mo.appendChild(token);
    return mo;
}

void collectScene(scene::Group const &group, std::vector<scene::Group const *> &groups,
                  std::vector<std::uint32_t> &codepoints) {
    groups.push_back(&group);
    for (auto const &entry : group.entries) {
        if (auto const *primitive = std::get_if<scene::Primitive>(&entry)) {
            if (auto const *run = std::get_if<scene::GlyphRun>(primitive))
                for (auto const &glyph : run->glyphs)
                    codepoints.push_back(glyph.key.codepoint);
        } else if (auto const *child = std::get_if<std::shared_ptr<scene::Group const>>(&entry)) {
            collectScene(**child, groups, codepoints);
        }
    }
}
} // namespace

suite common_mo_native_impl = [] {
    "generated_operator_preserves_physical_inferred_parent_and_arena_ownership"_test = [] {
        auto fonts = makeRepository();
        RejectUnexpectedMeasurement measurement;
        mml::MmlDocument document;
        auto &source = makeMo(document, u"x");
        std::array<mml::MmlNode *, 1> children{&source};
        auto &math = document.factory().make(mml::NodeKind::math, {}, children);
        document.setRoot(math);
        auto *physicalParent = source.parent();
        expect(physicalParent != nullptr && physicalParent->isInferred());
        expect(source.Parent() == &math);
        auto creators = catalog();
        creators.front() = registration<OperatorCreatorProbe>(mml::NodeKind::mo);
        common::CommonOutputJax jax(document, {{}, {}, fonts, "test-face"}, measurement,
                                    std::move(creators), &makeLinebreaks);
        (void)jax.factory().wrap(math);
        auto *sourceWrapper = jax.wrapperFor(source);
        expect(sourceWrapper != nullptr);
        if (!sourceWrapper) return;
        auto const normalCount = physicalParent->children().size();
        auto &generated = dynamic_cast<OperatorCreatorProbe &>(*sourceWrapper).createMo(u"(");
        expect(generated.node->parent() == physicalParent);
        expect(generated.node->Parent() == &math);
        expect(physicalParent->children().size() == normalCount);
        expect(jax.wrapperFor(*generated.node) == &generated);
        expect(generated.parent == sourceWrapper);
        expect(generated.getText() == u"(");
    };

    "CommonMo factory creation preserves Source delimiter decisions"_test = [] {
        auto fonts = makeRepository();
        RejectUnexpectedMeasurement measurement;
        mml::MmlDocument document;
        auto &source = makeMo(document, u"(");
        source.attributes().set(u"stretchy", true);
        source.attributes().set(u"symmetric", false);
        source.attributes().set(u"linebreak", u"newline");
        source.attributes().set(u"linebreakstyle", u"before");
        document.setRoot(source);

        common::CommonOptions options;
        common::ExtendedMetrics metrics;
        common::CommonOutputJax jax(document, {metrics, options, fonts, "test-face"}, measurement,
                                    catalog(), &makeLinebreaks);
        auto *moPointer = dynamic_cast<common::CommonMo *>(&jax.factory().wrap(source));
        expect(moPointer != nullptr);
        if (!moPointer)
            return;
        auto &mo = *moPointer;
        expect(mo.breakStyle == "before");
        expect(mo.breakCount() == 1_u);
        expect(mo.canStretch(common::StretchDirection::vertical));
        expect(std::holds_alternative<common::ActiveStretch>(mo.stretch));

        const std::array<double, 1> one{1.25};
        const std::array<double, 2> pair{2, 1};
        expect(mo.getWH(std::span<double const>{}) == 0.0);
        expect(mo.getWH(one) == 1.25);
        expect(mo.getWH(pair) == 3.0);
        source.attributes().set(u"symmetric", true);
        expect(mo.getWH(pair) == 3.5);
        source.attributes().set(u"symmetric", false);

        const std::array<double, 1> fixedRequest{1.2};
        mo.childNodes.front()->bboxComputed = true;
        mo.getStretchedVariant(fixedRequest, true);
        expect(mo.size.has_value());
        expect(std::get<common::FixedStretchSize>(*mo.size).index == 2_u);
        expect(mo.variant == "size-2");
        expect(std::get<common::ActiveStretch>(mo.stretch).selectedCodepoint ==
               std::uint32_t{0x239B});
        expect(!mo.childNodes.front()->bboxComputed);

        mo.size.reset();
        mo.stretch = common::ActiveStretch{0x28, {}};
        const std::array<double, 1> zeroSizeRequest{.2};
        mo.getStretchedVariant(zeroSizeRequest, true);
        expect(std::get<common::FixedStretchSize>(*mo.size).index == 0_u);
        expect(std::get<common::ActiveStretch>(mo.stretch).selectedCodepoint ==
               std::uint32_t{0x239A});
        const std::array<double, 1> zeroSizeRepeat{.1};
        mo.getStretchedVariant(zeroSizeRepeat, true);
        expect(std::get<common::ActiveStretch>(mo.stretch).selectedCodepoint ==
               std::uint32_t{0x239A});

        mo.size.reset();
        mo.stretch = common::ActiveStretch{0x28, {}};
        mo.bbox = folia::math::BBox::zero();
        const std::array<double, 2> assemblyRequest{3, 2};
        mo.getStretchedVariant(assemblyRequest);
        expect(std::holds_alternative<common::AssemblyStretchSize>(*mo.size));
        expect(mo.bbox.h == 3.0);
        expect(mo.bbox.d == 2.0);
        expect(mo.bbox.w == .8);
        expect(mo.checkExtendedHeight(4.1, *fonts.delimiter("test-face", 0x28)) == 5.0);
    };

    "CommonMo baseline uses supplied dimensions and face axis height"_test = [] {
        auto fonts = makeRepository();
        RejectUnexpectedMeasurement measurement;
        mml::MmlDocument document;
        auto &source = makeMo(document, u"(");
        source.attributes().set(u"symmetric", false);
        document.setRoot(source);
        common::CommonOptions options;
        common::ExtendedMetrics metrics;
        common::CommonOutputJax jax(document, {metrics, options, fonts, "test-face"}, measurement,
                                    catalog(), &makeLinebreaks);
        auto *moPointer = dynamic_cast<common::CommonMo *>(&jax.factory().wrap(source));
        expect(moPointer != nullptr);
        if (!moPointer)
            return;
        auto &mo = *moPointer;
        auto const *recipe = fonts.delimiter("test-face", 0x28);
        expect(recipe != nullptr);
        expect(mo.getBaseline(std::array<double, 2>{3, 1}, 4, *recipe) ==
               std::array<double, 2>{3, 1});
        source.attributes().set(u"symmetric", true);
        expect(mo.getBaseline(std::array<double, 2>{3, 1}, 4, *recipe) ==
               std::array<double, 2>{3, 2.5});
        source.attributes().set(u"symmetric", false);
        expect(mo.getBaseline({}, 4, *recipe) == std::array<double, 2>{3, 1});
    };

    "CommonMo nonaccent remapping does not require a source parent"_test = [] {
        auto fonts = makeRepository();
        RejectUnexpectedMeasurement measurement;
        mml::MmlDocument document;
        auto& source = makeMo(document, u"x");
        common::CommonOptions options;
        common::ExtendedMetrics metrics;
        common::CommonOutputJax jax(document, {metrics, options, fonts, "test-face"}, measurement,
                                    catalog(), &makeLinebreaks);
        auto& mo = dynamic_cast<common::CommonMo&>(jax.factory().wrap(source));
        expect(source.parent() == nullptr);
        expect(!mo.isAccent);
        const std::array<std::uint32_t, 1> plus{0x2B};
        expect(mo.remapChars(plus) == std::vector<std::uint32_t>{0x2295});
    };

    "CommonMo non-delimiter and typed MML remap outcomes remain distinct"_test = [] {
        auto fonts = makeRepository();
        RejectUnexpectedMeasurement measurement;
        mml::MmlDocument document;
        auto &math = dynamic_cast<mml::MmlMath &>(document.factory().make(mml::NodeKind::math));
        auto &row = dynamic_cast<mml::MmlMrow &>(document.factory().make(mml::NodeKind::mrow));
        auto &source = makeMo(document, u"x");
        math.appendChild(row);
        row.appendChild(source);
        document.setRoot(math);
        common::CommonOptions options;
        common::ExtendedMetrics metrics;
        common::CommonOutputJax jax(document, {metrics, options, fonts, "test-face"}, measurement,
                                    catalog(), &makeLinebreaks);
        (void)jax.factory().wrap(math);
        auto *mo = dynamic_cast<common::CommonMo *>(jax.wrapperFor(source));
        expect(mo != nullptr);
        if (!mo)
            return;
        expect(!mo->canStretch(common::StretchDirection::vertical));
        expect(std::holds_alternative<common::NoStretch>(mo->stretch));
        const std::array<std::uint32_t, 1> plus{0x2B};
        expect(mo->remapChars(plus) == std::vector<std::uint32_t>{0x2295});
        source.properties().tex.primes = u"′′";
        expect(mo->remapChars(plus) == std::vector<std::uint32_t>{0x2032, 0x2032});
    };

    "CommonMo emits and places every distinct text child"_test = [] {
        auto fonts = makeRepository();
        RejectUnexpectedMeasurement measurement;
        mml::MmlDocument document;
        auto &source = makeMo(document, u">");
        source.attributes().set(u"mathvariant", u"normal");
        auto &secondText =
            dynamic_cast<mml::TextNode &>(document.factory().make(mml::NodeKind::text));
        secondText.setText(u"<");
        source.appendChild(secondText);

        auto &following = document.factory().make(mml::NodeKind::mi);
        following.attributes().set(u"mathvariant", u"normal");
        auto &followingText =
            dynamic_cast<mml::TextNode &>(document.factory().make(mml::NodeKind::text));
        followingText.setText(u"b");
        following.appendChild(followingText);

        auto &row = document.factory().make(mml::NodeKind::mrow);
        row.appendChild(source);
        row.appendChild(following);
        document.setRoot(row);
        row.setInheritedAttributes();

        common::CommonOptions options;
        common::ExtendedMetrics metrics;
        common::CommonOutputJax jax(document, {metrics, options, fonts, "test-face"}, measurement,
                                    common::CommonWrapperFactory::standardNodes(), &makeLinebreaks);
        auto &root = jax.factory().wrap(row);
        auto *moWrapper = jax.wrapperFor(source);
        expect(moWrapper != nullptr);
        if (!moWrapper)
            return;

        scene::Builder builder;
        const std::array<scene::NodeId, 1> parents{0};
        (void)root.emitScene(builder, parents);
        auto completed = std::move(builder).freeze();
        std::vector<scene::Group const *> groups;
        std::vector<std::uint32_t> codepoints;
        collectScene(completed.root(), groups, codepoints);

        std::vector<scene::Group const *> textGroups;
        scene::Group const *followingGroup = nullptr;
        for (auto const *group : groups) {
            if (group->metadata.source_tag == "text")
                textGroups.push_back(group);
            if (group->metadata.source_tag == "mi")
                followingGroup = group;
        }
        expect(textGroups.size() == 2_u) << "both mo text wrappers are emitted";
        expect(codepoints == std::vector<std::uint32_t>{0x3E, 0x3C, 'b'})
            << "glyphs preserve the MML child order";
        if (textGroups.size() >= 2) {
            expect(textGroups[0]->entries.size() == 1_u);
            expect(textGroups[1]->entries.size() == 1_u);
            expect(std::abs(textGroups[0]->transform.matrix().e) < 1e-12);
            const auto firstTextBox = moWrapper->childNodes.front()->getOuterBBox();
            const double secondTextX =
                (firstTextBox.L + firstTextBox.w + firstTextBox.R) * firstTextBox.rscale;
            expect(std::abs(secondTextX - .778) < 1e-12);
            expect(std::abs(textGroups[1]->transform.matrix().e - secondTextX) < 1e-12)
                << "second text child is placed after the first child's measured advance";
        }
        expect(followingGroup != nullptr) << "following sibling has a scene group";
        if (followingGroup) {
            const auto line = moWrapper->getLineBBox(0);
            const double expectedX = (line->L + line->w + line->R) * line->rscale;
            expect(std::abs(followingGroup->transform.matrix().e - expectedX) < 1e-12)
                << "following sibling starts after the full mo bbox";
        }
    };
};
