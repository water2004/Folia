#pragma once

#include "support/folia_test.hpp"
#include "math/support/common_bbox_fixtures.hpp"
import folia.math.output.common;
import folia.math.core.mml_tree.mml_nodes.mo;
import folia.math.util.utf16;
import folia.math.fonts.newcm_font_module_provider;

using namespace boost::ut;
namespace common = folia::math::output::common;
namespace mml = folia::math::mml;
namespace style_util = folia::math::styles;
namespace style_json = folia::math::style_json;

namespace {
class UnexpectedLinebreaks final : public common::Linebreaks {
  public:
    explicit UnexpectedLinebreaks(common::CommonWrapperFactory &factory)
        : common::Linebreaks(factory) {}

    void breakToWidth(common::CommonWrapper &, double) override {
        throw std::logic_error("unexpected linebreak visitor call in Jax method test");
    }
};

std::unique_ptr<common::Linebreaks>
makeUnexpectedLinebreaks(common::CommonWrapperFactory &factory) {
    return std::make_unique<UnexpectedLinebreaks>(factory);
}

common::CommonWrapperCatalog testCatalog(std::optional<mml::NodeKind> kind = std::nullopt) {
    common::CommonWrapperCatalog result;
    if (!kind)
        return result;

    common::CommonWrapperRegistration registration;
    registration.kind = *kind;
    registration.data = common::CommonWrapperClassData::commonDefaults();
    registration.creator.construct = [](common::WrapperBuildArguments const &args) {
        return std::make_unique<common::CommonWrapper>(args.factory, args.node, args.parent);
    };
    registration.creator.isInstance = [](common::CommonWrapper const &wrapper) {
        return dynamic_cast<common::CommonWrapper const *>(&wrapper) != nullptr;
    };
    result.push_back(std::move(registration));
    return result;
}

class Measurement final : public common::MeasurementTarget {
  public:
    std::size_t metricCalls{};
    std::size_t textCalls{};
    common::MetricRequest lastMetrics;
    common::UnknownTextRequest lastText;

    common::ExtendedMetrics measureMetrics(common::MetricRequest const &request) override {
        ++metricCalls;
        lastMetrics = request;
        return {18, 9, 600, .75, request.family, false};
    }

    common::UnknownBBox measureUnknownText(common::UnknownTextRequest const &request) override {
        ++textCalls;
        lastText = request;
        return {static_cast<double>(textCalls), 2, 1};
    }
};

common::FontRepository testFonts() {
    common::FontRepositoryBuilder builder;
    common::FontFaceData face;
    face.name = "test";
    face.cssFamilyPrefix = "TestMath";
    face.cssFontMap.emplace("-explicitFont", common::CssFontData{"serif", false, false});
    face.styles = {{u".native-font", std::vector<style_json::StyleEntry>{
                                         {u"font-family", std::u16string(u"serif")}}}};
    builder.addFace(std::move(face));
    builder.addParameter("test", "x_height", .5);
    return std::move(builder).freeze();
}

common::CommonOutputJaxConfig config(common::FontRepository const &fonts) {
    common::ExtendedMetrics metrics;
    metrics.em = 16;
    metrics.ex = 8;
    metrics.containerWidth = 400;
    common::CommonOptions options;
    options.linebreaks.width = "100%";
    return {metrics, std::move(options), fonts, "test"};
}

template <class Wrapper> common::CommonWrapperRegistration bboxRegistration(mml::NodeKind kind) {
    return {kind,
            {[](common::WrapperBuildArguments const &a) -> std::unique_ptr<common::CommonWrapper> {
                 return std::make_unique<Wrapper>(a.factory, a.node, a.parent);
             },
             [](common::CommonWrapper const &w) {
                 return dynamic_cast<Wrapper const *>(&w) != nullptr;
             }},
            common::CommonWrapperClassData::commonDefaults()};
}
common::CommonWrapperCatalog bboxCatalog() {
    using K = mml::NodeKind;
    return {bboxRegistration<common::CommonMspace>(K::mspace),
            bboxRegistration<common::CommonMrow>(K::mrow),
            bboxRegistration<common::CommonInferredMrow>(K::inferred_mrow),
            bboxRegistration<common::CommonWrapper>(K::mstyle),
            bboxRegistration<common::CommonMpadded>(K::mpadded),
            bboxRegistration<common::CommonMfrac>(K::mfrac),
            bboxRegistration<common::CommonMsqrt>(K::msqrt),
            bboxRegistration<common::CommonMroot>(K::mroot),
            bboxRegistration<common::CommonMtable>(K::mtable),
            bboxRegistration<common::CommonMtr>(K::mtr),
            bboxRegistration<common::CommonMtd>(K::mtd),
            bboxRegistration<common::CommonTextNode>(K::text),
            bboxRegistration<common::CommonMi>(K::mi),
            bboxRegistration<common::CommonMn>(K::mn),
            bboxRegistration<common::CommonMo>(K::mo),
            bboxRegistration<common::CommonMtext>(K::mtext)};
}
common::CommonOutputJaxConfig bboxConfig(common::FontRepository const &fonts, bool display) {
    return {{16, 8, 640, 1, "MathJaxNewcm", display}, {}, fonts, "MathJaxNewcm"};
}
mml::AttributeSet bboxAttrs(std::initializer_list<mml::AttributeSet::Entry> entries) {
    mml::AttributeSet result;
    for (auto const &[key, value] : entries)
        result.set(key, value);
    return result;
}
mml::MmlNode &bboxNode(mml::MmlDocument &doc, mml::NodeKind kind,
                       std::initializer_list<mml::MmlNode *> children = {},
                       mml::AttributeSet attributes = {}) {
    return doc.factory().make(kind, std::move(attributes), {children.begin(), children.size()});
}
mml::MmlNode &bboxSpace(mml::MmlDocument &doc, std::u16string width = u"1.25em",
                        std::u16string height = u".8em", std::u16string depth = u".2em") {
    return bboxNode(doc, mml::NodeKind::mspace, {},
                    bboxAttrs({{u"width", width}, {u"height", height}, {u"depth", depth}}));
}
mml::MmlNode &bboxToken(mml::MmlDocument &doc, mml::NodeKind kind, std::u16string text) {
    auto &content = doc.factory().make(mml::NodeKind::text);
    content.setText(std::move(text));
    return bboxNode(doc, kind, {&content});
}
mml::MmlNode &bboxTable(mml::MmlDocument &doc, bool frame = false) {
    using K = mml::NodeKind;
    auto &a = bboxSpace(doc), &b = bboxSpace(doc, u".7em", u".3em", u".1em");
    auto &c = bboxSpace(doc, u".7em", u".3em", u".1em"), &d = bboxSpace(doc);
    auto &ca = bboxNode(doc, K::mtd, {&a}), &cb = bboxNode(doc, K::mtd, {&b});
    auto &cc = bboxNode(doc, K::mtd, {&c}), &cd = bboxNode(doc, K::mtd, {&d});
    auto &r1 = bboxNode(doc, K::mtr, {&ca, &cb}), &r2 = bboxNode(doc, K::mtr, {&cc, &cd});
    auto attrs = bboxAttrs({{u"columnspacing", std::u16string(u".5em")},
                            {u"rowspacing", std::u16string(u".2em")},
                            {u"framespacing", std::u16string(frame ? u".5em .125em" : u"0 0")}});
    if (frame) {
        attrs.set(u"frame", u"solid");
        attrs.set(u"rowlines", u"dashed");
    }
    return bboxNode(doc, K::mtable, {&r1, &r2}, std::move(attrs));
}
mml::MmlNode &bboxSourceNode(mml::MmlDocument &doc, std::string_view name) {
    using K = mml::NodeKind;
    if (name == "space")
        return bboxSpace(doc);
    if (name == "negative-space")
        return bboxSpace(doc, u"-.25em", u".3em", u".1em");
    if (name == "px-space")
        return bboxSpace(doc, u"8px", u"16px", u"4px");
    if (name == "ex-space")
        return bboxSpace(doc, u"1.5ex", u"1ex", u".5ex");
    if (name == "ex-padded")
        return bboxNode(doc, K::mpadded, {&bboxSpace(doc, u"1ex", u".5ex", u".25ex")},
                        bboxAttrs({{u"width", std::u16string(u"+.5ex")},
                                   {u"lspace", std::u16string(u".25ex")}}));
    if (name == "padded")
        return bboxNode(doc, K::mpadded, {&bboxSpace(doc, u"1em", u".4em", u".2em")},
                        bboxAttrs({{u"width", std::u16string(u"+.5em")},
                                   {u"lspace", std::u16string(u".2em")},
                                   {u"height", std::u16string(u"+.1em")},
                                   {u"depth", std::u16string(u"+.1em")},
                                   {u"voffset", std::u16string(u".05em")}}));
    if (name == "styled")
        return bboxNode(
            doc, K::mrow, {&bboxSpace(doc)},
            bboxAttrs({{u"style", std::u16string(u"margin-left: .2em; margin-right: .3em; padding: "
                                                 u".1em; border: .05em solid red")}}));
    if (name == "fraction" || name == "atop" || name == "bevelled") {
        auto attrs = mml::AttributeSet{};
        if (name == "atop")
            attrs.set(u"linethickness", 0.0);
        if (name == "bevelled")
            attrs.set(u"bevelled", true);
        return bboxNode(doc, K::mfrac,
                        {&bboxSpace(doc), &bboxSpace(doc, u".7em", u".3em", u".1em")},
                        std::move(attrs));
    }
    if (name == "sqrt")
        return bboxNode(doc, K::msqrt, {&bboxSpace(doc)});
    if (name == "root")
        return bboxNode(doc, K::mroot,
                        {&bboxSpace(doc), &bboxSpace(doc, u".3em", u".3em", u"0em")});
    if (name == "table" || name == "framed-table")
        return bboxTable(doc, name == "framed-table");
    if (name == "nested") {
        auto &radical = bboxNode(doc, K::msqrt, {&bboxSpace(doc)});
        auto &padded = bboxNode(doc, K::mpadded, {&bboxTable(doc)},
                                bboxAttrs({{u"width", std::u16string(u"+.2em")}}));
        return bboxNode(doc, K::mfrac, {&radical, &padded});
    }
    if (name == "tokens")
        return bboxNode(doc, K::mrow,
                        {&bboxToken(doc, K::mi, u"x"), &bboxToken(doc, K::mo, u"+"),
                         &bboxToken(doc, K::mn, u"12")});
    if (name == "bold-tokens") {
        auto &row = bboxNode(doc, K::mrow,
                             {&bboxToken(doc, K::mi, u"AB"), &bboxToken(doc, K::mo, u"="),
                              &bboxToken(doc, K::mi, u"C")});
        return bboxNode(doc, K::mstyle, {&row},
                        bboxAttrs({{u"mathvariant", std::u16string(u"bold")}}));
    }
    if (name == "unicode")
        return bboxNode(doc, K::mrow,
                        {&bboxToken(doc, K::mi, u"α"), &bboxToken(doc, K::mo, u"→"),
                         &bboxToken(doc, K::mi, u"β")});
    throw std::logic_error("Unknown Common bbox Source case");
}

struct BBoxTreeState {
    mml::MmlNode *node, *parent;
    mml::AttributeSet attrs;
    mml::LayoutNodeProperties layout;
    std::vector<mml::MmlNode *> children;
};
std::vector<BBoxTreeState> bboxTreeState(mml::MmlNode &root) {
    std::vector<BBoxTreeState> result;
    std::vector<mml::MmlNode *> pending{&root};
    while (!pending.empty()) {
        auto &node = *pending.back();
        pending.pop_back();
        std::vector<mml::MmlNode *> children(node.children().begin(), node.children().end());
        result.push_back(
            {&node, node.parent(), node.attributes(), node.properties().layout, children});
        for (auto *child : children)
            if (child)
                pending.push_back(child);
    }
    return result;
}
void checkBBoxTreeState(std::span<BBoxTreeState const> saved) {
    for (auto const &entry : saved) {
        expect(entry.node->parent() == entry.parent);
        bool attrsUnchanged =
            entry.node->attributes().ownEntries() == entry.attrs.ownEntries() &&
            entry.node->attributes().inheritedEntries() == entry.attrs.inheritedEntries() &&
            entry.node->attributes().defaultEntries() == entry.attrs.defaultEntries() &&
            entry.node->attributes().globalDefaultEntries() == entry.attrs.globalDefaultEntries();
        expect(attrsUnchanged);
        expect(std::ranges::equal(entry.node->children(), entry.children));
        expect(entry.node->properties().layout == entry.layout);
    }
}
void checkSourceBBox(folia::math::BBox const &box, BBoxSourceCase const &expected) {
    auto check = [&](double actual, double value, char const *field) {
        expect(std::abs(actual - value) < 1e-11)
            << expected.name << " " << field << " " << actual << " " << value;
    };
    check(box.w, expected.w, "w");
    check(box.h, expected.h, "h");
    check(box.d, expected.d, "d");
    check(box.scale, expected.scale, "scale");
    check(box.rscale, expected.rscale, "rscale");
    check(box.L, expected.L, "L");
    check(box.R, expected.R, "R");
    check(box.ic, expected.ic, "ic");
    check(box.oc, expected.oc, "oc");
    check(box.sk, expected.sk, "sk");
    check(box.dx, expected.dx, "dx");
    expect(box.pwidth == expected.pwidth);
}
struct BBoxCounts {
    std::size_t wrappers{}, destroyed{}, visitors{}, visitorDestroyed{}, computes{};
    bool failInitialize{}, failCompute{};
};
thread_local BBoxCounts *bboxCounts{};
class CountedBBoxSpace final : public common::CommonMspace {
    BBoxCounts &counts_;

  protected:
    void computeBBox(folia::math::BBox &box, bool recompute) override {
        ++counts_.computes;
        if (counts_.failCompute)
            throw std::runtime_error("bbox compute probe");
        CommonMspace::computeBBox(box, recompute);
    }

  public:
    CountedBBoxSpace(common::CommonWrapperFactory &f, mml::MmlNode &node,
                     common::CommonWrapper *parent, BBoxCounts &counts)
        : CommonMspace(f, node, parent), counts_(counts) {
        ++counts_.wrappers;
    }
    ~CountedBBoxSpace() override {
        ++counts_.destroyed;
    }
    void initializeSource() override {
        CommonMspace::initializeSource();
        if (counts_.failInitialize)
            throw std::runtime_error("bbox initialize probe");
    }
};
class CountedBBoxVisitor final : public common::Linebreaks {
    BBoxCounts &counts_;

  public:
    CountedBBoxVisitor(common::CommonWrapperFactory &factory, BBoxCounts &counts)
        : Linebreaks(factory), counts_(counts) {
        ++counts_.visitors;
    }
    ~CountedBBoxVisitor() override {
        ++counts_.visitorDestroyed;
    }
    void breakToWidth(common::CommonWrapper &, double) override {
        throw std::logic_error("bbox must not line-break");
    }
};
std::unique_ptr<common::Linebreaks> countedBBoxVisitor(common::CommonWrapperFactory &f) {
    if (!bboxCounts)
        throw std::logic_error("bbox counts require scoped test owner");
    return std::make_unique<CountedBBoxVisitor>(f, *bboxCounts);
}
common::CommonWrapperCatalog countedBBoxCatalog(BBoxCounts &counts) {
    return {{mml::NodeKind::mspace,
             {[&counts](common::WrapperBuildArguments const &a)
                  -> std::unique_ptr<common::CommonWrapper> {
                  return std::make_unique<CountedBBoxSpace>(a.factory, a.node, a.parent, counts);
              },
              [](common::CommonWrapper const &w) {
                  return dynamic_cast<CountedBBoxSpace const *>(&w) != nullptr;
              }},
             common::CommonWrapperClassData::commonDefaults()}};
}
struct BBoxMmlCounts {
    std::size_t operators{}, operatorDestroyed{}, texts{}, textDestroyed{};
};
class CountedBBoxMo final : public mml::MmlMo {
    BBoxMmlCounts &counts_;

  public:
    CountedBBoxMo(mml::MmlDocument &document, BBoxMmlCounts &counts)
        : MmlMo(document), counts_(counts) {
        ++counts_.operators;
    }
    ~CountedBBoxMo() override {
        ++counts_.operatorDestroyed;
    }
};
class CountedBBoxText final : public mml::TextNode {
    BBoxMmlCounts &counts_;

  public:
    CountedBBoxText(mml::MmlDocument &document, BBoxMmlCounts &counts)
        : TextNode(document), counts_(counts) {
        ++counts_.texts;
    }
    ~CountedBBoxText() override {
        ++counts_.textDestroyed;
    }
};
void registerBBoxMml(mml::MmlDocument &document, BBoxMmlCounts &counts) {
    document.factory().setNodeClass(
        mml::NodeKind::mo, {[&counts](mml::MmlCreationArguments const &a) {
                                return std::make_unique<CountedBBoxMo>(a.document, counts);
                            },
                            [](mml::MmlNode const &node) {
                                return dynamic_cast<CountedBBoxMo const *>(&node) != nullptr;
                            }});
    document.factory().setNodeClass(
        mml::NodeKind::text, {[&counts](mml::MmlCreationArguments const &a) {
                                  return std::make_unique<CountedBBoxText>(a.document, counts);
                              },
                              [](mml::MmlNode const &node) {
                                  return dynamic_cast<CountedBBoxText const *>(&node) != nullptr;
                              }});
}
mml::MmlNode &bboxAuxiliaryTree(mml::MmlDocument &document) {
    auto &radical = bboxSourceNode(document, "sqrt");
    auto &fraction = bboxSourceNode(document, "bevelled");
    return bboxNode(document, mml::NodeKind::mrow, {&radical, &fraction});
}
} // namespace
