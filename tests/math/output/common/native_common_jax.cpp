#include "support/folia_test.hpp"
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
struct BBoxSourceCase {
    std::string_view name;
    bool display;
    double w, h, d, scale, rscale, L, R, ic, oc, sk, dx;
    std::string_view pwidth;
};
#include "math/fixtures/common-source/bbox.inc"
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

suite math_native_common_jax_methods = [] {
    "common subtree bbox matches real Source profile without changing document root or tree"_test =
        [] {
            auto bundle = folia::math::fonts::makeNewcmFontData();
            for (auto const &entry : bboxSourceCases) {
                mml::MmlDocument document;
                auto &subtree = bboxSourceNode(document, entry.name);
                subtree.setInheritedAttributes({}, entry.display, 0, false);
                auto &parent = bboxNode(document, mml::NodeKind::mrow, {&subtree});
                auto &documentRoot = document.factory().make(mml::NodeKind::math);
                documentRoot.attributes().set(u"overflow", u"linebreak");
                documentRoot.properties().layout.inline_marked = true;
                document.setRoot(documentRoot);
                subtree.properties().layout.inline_marked = false;
                subtree.properties().layout.inline_forced = false;
                auto before = bboxTreeState(parent);
                auto rootBefore = bboxTreeState(documentRoot);
                Measurement target;
                auto box = common::CommonOutputJax::getBBox(
                    document, subtree, bboxConfig(bundle.fonts, entry.display), target,
                    bboxCatalog(), &makeUnexpectedLinebreaks);
                checkSourceBBox(box, entry);
                expect(target.metricCalls == 0_u);
                expect(target.textCalls == 0_u);
                expect(document.root() == &documentRoot);
                checkBBoxTreeState(before);
                checkBBoxTreeState(rootBefore);
            }
        };
    "common subtree bbox destroys fresh wrapper and visitor arenas over 192 mixed calls"_test = [] {
        auto fonts = testFonts();
        Measurement target;
        mml::MmlDocument document;
        auto &subtree = bboxSpace(document, u"1em", u".5em", u".1em");
        auto &parent = bboxNode(document, mml::NodeKind::mrow, {&subtree});
        auto &root = document.factory().make(mml::NodeKind::math);
        document.setRoot(root);
        BBoxCounts counts;
        struct Restore {
            BBoxCounts *old{bboxCounts};
            ~Restore() {
                bboxCounts = old;
            }
        } restore;
        bboxCounts = &counts;
        auto catalog = countedBBoxCatalog(counts);
        for (int i = 0; i < 192; ++i) {
            const double width = 1 + (i % 7) * .125;
            auto widthText = std::to_string(width);
            subtree.attributes().set(u"width",
                                     std::u16string(widthText.begin(), widthText.end()) + u"em");
            auto before = bboxTreeState(parent);
            counts.failInitialize = i % 3 == 1;
            counts.failCompute = i % 3 == 2;
            if (counts.failInitialize || counts.failCompute) {
                expect(throws<std::runtime_error>([&] {
                    common::CommonOutputJax::getBBox(document, subtree, config(fonts), target,
                                                     catalog, &countedBBoxVisitor);
                }));
            } else {
                auto box = common::CommonOutputJax::getBBox(document, subtree, config(fonts),
                                                            target, catalog, &countedBBoxVisitor);
                expect(std::abs(box.w - width) < 1e-12);
                expect(box.h == .5);
                expect(box.d == .1);
            }
            expect(counts.wrappers == counts.destroyed);
            expect(counts.visitors == counts.visitorDestroyed);
            expect(document.root() == &root);
            checkBBoxTreeState(before);
        }
        expect(counts.wrappers == 192_u);
        expect(counts.visitors == 192_u);
        expect(counts.computes == 128_u);
    };
    "common subtree bbox honors supplied catalog metrics and unknown text host"_test = [] {
        auto fonts = testFonts();
        Measurement target;
        mml::MmlDocument document;
        auto &space = bboxSpace(document, u"8px", u"1ex", u"0em");
        auto input = config(fonts);
        auto first = common::CommonOutputJax::getBBox(
            document, space, input, target,
            {bboxRegistration<common::CommonMspace>(mml::NodeKind::mspace)},
            &makeUnexpectedLinebreaks);
        expect(first.w == .5);
        expect(first.h == .431);
        input.metrics.ex = 16;
        auto second = common::CommonOutputJax::getBBox(
            document, space, input, target,
            {bboxRegistration<common::CommonMspace>(mml::NodeKind::mspace)},
            &makeUnexpectedLinebreaks);
        expect(second.w == .25);
        expect(second.h == .431);
        auto &text = bboxToken(document, mml::NodeKind::mtext, u"host text");
        text.attributes().set(u"style",
                              u"font-family: serif; font-weight: bold; font-style: italic;");
        auto &row = bboxNode(document, mml::NodeKind::mrow, {&text});
        row.setInheritedAttributes();
        auto box = common::CommonOutputJax::getBBox(document, row, config(fonts), target,
                                                    bboxCatalog(), &makeUnexpectedLinebreaks);
        expect(box.w == 1.0);
        expect(box.h == 2.0);
        expect(box.d == 1.0);
        expect(target.textCalls == 1_u);
        expect(target.metricCalls == 0_u);
        expect(std::ranges::equal(target.lastText.text, std::u16string_view(u"host text")));
        expect(target.lastText.variant == "-explicitFont");
        expect(target.lastText.font.bold);
        expect(target.lastText.font.italic);
        expect(target.lastText.font.family == "TestMath, serif");
        auto fresh = common::CommonOutputJax::getBBox(document, row, config(fonts), target,
                                                      bboxCatalog(), &makeUnexpectedLinebreaks);
        expect(fresh.w == 2.0);
        expect(target.textCalls == 2_u);
    };
    "common subtree bbox rejects foreign owner before TeXclass and accepts detached tables"_test =
        [] {
            auto bundle = folia::math::fonts::makeNewcmFontData();
            Measurement target;
            mml::MmlDocument document, foreign;
            auto &subtree = bboxSpace(foreign);
            subtree.setTexClass(mml::TexClass::rel);
            auto initialClass = subtree.texClass();
            BBoxCounts counts;
            struct Restore {
                BBoxCounts *old{bboxCounts};
                ~Restore() {
                    bboxCounts = old;
                }
            } restore;
            bboxCounts = &counts;
            expect(throws<std::invalid_argument>([&] {
                common::CommonOutputJax::getBBox(document, subtree, config(bundle.fonts), target,
                                                 {}, &countedBBoxVisitor);
            }));
            expect(subtree.texClass() == initialClass);
            expect(counts.visitors == 0_u);
            auto &table = bboxTable(document);
            table.setInheritedAttributes({}, false, 0, false);
            auto result =
                common::CommonOutputJax::getBBox(document, table, bboxConfig(bundle.fonts, false),
                                                 target, bboxCatalog(), &makeUnexpectedLinebreaks);
            expect(result.w == 3.0);
            expect(!table.parent());
            expect(!document.root());
            common::CommonOutputJax jax(document, bboxConfig(bundle.fonts, false), target,
                                        bboxCatalog(), &makeUnexpectedLinebreaks);
            auto &wrapper = dynamic_cast<common::CommonMtable &>(jax.factory().wrap(table));
            expect(!wrapper.container);
            expect(!wrapper.containerI.has_value());
            expect(wrapper.getChildAlign(0) == "center");
        };
    "common subtree bbox helpers remain document owned until each request ends"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        Measurement target;
        BBoxMmlCounts mmlCounts;
        BBoxCounts wrappers;
        struct Restore {
            BBoxCounts *old{bboxCounts};
            ~Restore() {
                bboxCounts = old;
            }
        } restore;
        bboxCounts = &wrappers;
        auto catalog = bboxCatalog();
        auto counted = countedBBoxCatalog(wrappers);
        for (auto &entry : catalog)
            if (entry.kind == mml::NodeKind::mspace)
                entry = counted.front();
        for (int request = 0; request < 192; ++request) {
            {
                mml::MmlDocument document;
                registerBBoxMml(document, mmlCounts);
                auto &subtree = bboxAuxiliaryTree(document);
                subtree.setInheritedAttributes({}, false, 0, false);
                auto &parent = bboxNode(document, mml::NodeKind::mrow, {&subtree});
                auto &root = document.factory().make(mml::NodeKind::math);
                document.setRoot(root);
                auto before = bboxTreeState(parent);
                auto initialSize = document.size();
                auto initialOperators = mmlCounts.operators;
                auto initialTexts = mmlCounts.texts;
                std::optional<folia::math::BBox> first;
                // Fresh wrappers create a surd and bevel operator on every measurement.
                // Their MML nodes belong to this request document, not the wrapper arena.
                for (int repeat = 0; repeat < 2; ++repeat) {
                    auto box = common::CommonOutputJax::getBBox(
                        document, subtree, bboxConfig(bundle.fonts, false), target, catalog,
                        &countedBBoxVisitor);
                    expect(document.size() == initialSize + 4 * (repeat + 1));
                    expect(mmlCounts.operators == initialOperators + 2 * (repeat + 1));
                    expect(mmlCounts.texts == initialTexts + 2 * (repeat + 1));
                    expect(mmlCounts.operatorDestroyed == initialOperators);
                    expect(mmlCounts.textDestroyed == initialTexts);
                    expect(wrappers.wrappers == wrappers.destroyed);
                    expect(wrappers.visitors == wrappers.visitorDestroyed);
                    expect(document.root() == &root);
                    checkBBoxTreeState(before);
                    if (first) {
                        expect(box.w == first->w);
                        expect(box.h == first->h);
                        expect(box.d == first->d);
                    } else
                        first = box;
                }
            }
            expect(mmlCounts.operators == mmlCounts.operatorDestroyed);
            expect(mmlCounts.texts == mmlCounts.textDestroyed);
        }
        expect(mmlCounts.operators == 768_u);
        expect(mmlCounts.texts == 768_u);
        expect(wrappers.visitors == 384_u);
        expect(wrappers.wrappers == 1152_u);
    };
    "metrics_delegate_owned_request_and_set_requested_display"_test = [] {
        auto fonts = testFonts();
        Measurement target;
        mml::MmlDocument document;
        common::CommonOutputJax jax(document, config(fonts), target, {}, &makeUnexpectedLinebreaks);

        auto measured = jax.getMetricsFor({"math-font", true, false});
        expect(target.metricCalls == 1u);
        expect(target.lastMetrics.family == "math-font");
        expect(!target.lastMetrics.includeFamily);
        expect(measured.display);
        expect(measured.em == 18.0);
    };

    "unknown_text_cache_preserves_variant_text_and_explicit_css_key"_test = [] {
        auto fonts = testFonts();
        Measurement target;
        mml::MmlDocument document;
        common::CommonOutputJax jax(document, config(fonts), target, {}, &makeUnexpectedLinebreaks);
        const common::UnknownTextRequest ordinary{u"x", "normal", {}};
        const common::UnknownTextRequest explicitA{u"x", "-explicitFont", {"serif", true, false}};
        const common::UnknownTextRequest explicitB{u"x", "-explicitFont", {"serif", false, true}};

        const auto first = jax.measureTextWithCache(ordinary);
        const auto cached = jax.measureTextWithCache(ordinary);
        const auto italic = jax.measureTextWithCache(explicitA);
        const auto bold = jax.measureTextWithCache(explicitB);
        expect(target.lastText.font.bold);
        const auto entryPoint = jax.measureText(u"y", "-explicitFont", {"sans-serif", true, true});
        const auto nodeEntry = jax.measureTextNodeWithCache({u"z", "normal", {}});
        expect(target.textCalls == 5u);
        expect(first.w == cached.w);
        expect(italic.w != bold.w);
        expect(entryPoint.w != 0.0);
        expect(nodeEntry.w != 0.0);
    };

    "linebreak_width_uses_root_maxwidth_over_configured_width"_test = [] {
        auto fonts = testFonts();
        Measurement target;
        mml::MmlDocument document;
        auto &root = document.factory().make(mml::NodeKind::math);
        root.attributes().set(u"maxwidth", u"50%");
        document.setRoot(root);
        common::CommonOutputJax jax(document, config(fonts), target, {}, &makeUnexpectedLinebreaks);

        expect(jax.getLinebreakWidth() == 12.5);
        expect(jax.containerWidth() == 12.5);
    };

    "operator_marking_and_unmarking_update_typed_layout_state"_test = [] {
        auto fonts = testFonts();
        Measurement target;
        mml::MmlDocument document;
        auto &root = document.factory().make(mml::NodeKind::math);
        auto &row = document.factory().make(mml::NodeKind::mrow);
        auto &left = document.factory().make(mml::NodeKind::mi);
        auto &op = dynamic_cast<mml::MmlMo &>(document.factory().make(mml::NodeKind::mo));
        auto &right = document.factory().make(mml::NodeKind::mi);
        op.attributes().set(u"linebreak", u"before");
        op.attributes().set(u"linebreakstyle", u"before");
        op.setTexClass(mml::TexClass::rel);
        document.append(row, left);
        document.append(row, op);
        document.append(row, right);
        document.append(root, row);
        document.setRoot(root);
        common::CommonOutputJax jax(document, config(fonts), target, {}, &makeUnexpectedLinebreaks);

        expect(!jax.forceInlineBreaks());
        jax.markInlineBreaks(row);
        expect(row.properties().layout.process_breaks.value_or(false));
        expect(root.properties().layout.process_breaks.value_or(false));
        expect(op.properties().layout.breakable.value_or(false));
        jax.unmarkInlineBreaks(root);
        expect(!row.properties().layout.process_breaks.value_or(false));
        expect(!root.properties().layout.process_breaks.value_or(false));
        expect(!op.properties().layout.breakable.value_or(false));
    };

    "nested_style_and_semantic_added_branches_mark_the_scanned_parent"_test = [] {
        auto fonts = testFonts();
        Measurement target;
        mml::MmlDocument document;
        auto &root = document.factory().make(mml::NodeKind::math);
        auto &styleScan = document.factory().make(mml::NodeKind::mrow);
        auto &style = document.factory().make(mml::NodeKind::mstyle);
        auto &styleContents = document.factory().make(mml::NodeKind::mrow);
        auto &styleMo = dynamic_cast<mml::MmlMo &>(document.factory().make(mml::NodeKind::mo));
        styleMo.setTexClass(mml::TexClass::rel);
        styleMo.attributes().set(u"linebreak", u"before");
        styleMo.attributes().set(u"linebreakstyle", u"before");
        document.append(styleContents, document.factory().make(mml::NodeKind::mi));
        document.append(styleContents, styleMo);
        document.append(style, styleContents);
        document.append(styleScan, style);
        document.append(root, styleScan);

        auto &semanticScan = document.factory().make(mml::NodeKind::mrow);
        auto &semanticAdded = document.factory().make(mml::NodeKind::mrow);
        auto &semanticMo = dynamic_cast<mml::MmlMo &>(document.factory().make(mml::NodeKind::mo));
        semanticMo.setTexClass(mml::TexClass::rel);
        semanticMo.attributes().set(u"linebreak", u"before");
        semanticMo.attributes().set(u"linebreakstyle", u"before");
        semanticAdded.attributes().set(u"data-semantic-added", true);
        document.append(semanticAdded, document.factory().make(mml::NodeKind::mi));
        document.append(semanticAdded, semanticMo);
        document.append(semanticScan, semanticAdded);
        document.append(root, semanticScan);
        document.setRoot(root);

        // Source only takes these parent-marking handoffs when the wrapper
        // already carries process-breaks. Seed that state to isolate the
        // destination chosen by the mstyle and semantic-added branches.
        style.properties().layout.process_breaks = true;
        semanticAdded.properties().layout.process_breaks = true;

        common::CommonOutputJax jax(document, config(fonts), target, {}, &makeUnexpectedLinebreaks);
        jax.markInlineBreaks(styleScan);
        expect(styleScan.parent() != nullptr);
        expect(styleScan.parent()->properties().layout.process_breaks.value_or(false));
        expect(!styleScan.properties().layout.process_breaks.value_or(false));

        jax.unmarkInlineBreaks(root);
        semanticAdded.properties().layout.process_breaks = true;
        jax.markInlineBreaks(semanticScan);
        expect(semanticScan.parent() != nullptr);
        expect(semanticScan.parent()->properties().layout.process_breaks.value_or(false));
        expect(semanticScan.properties().layout.process_breaks.value_or(false));
    };

    "font_style_collection_uses_face_and_wrapper_catalog_values"_test = [] {
        auto fonts = testFonts();
        Measurement target;
        mml::MmlDocument document;
        common::CommonOutputJax jax(document, config(fonts), target, testCatalog(mml::NodeKind::mo),
                                    &makeUnexpectedLinebreaks);
        style_json::StyleJson initial{{u"color", std::u16string(u"red")}};
        auto styles = jax.cssFontStyles({"serif", true, true}, std::move(initial));

        expect(styles.size() == 4u);
        expect(styles[0].name.compare(u"color") == 0);
        expect(std::get<std::u16string>(styles[1].value).compare(u"TestMath, serif") == 0);
        expect(std::get<std::u16string>(styles[2].value).compare(u"italic") == 0);
        expect(std::get<std::u16string>(styles.back().value).compare(u"bold") == 0);

        style_util::Styles source(u"font-family: serif; font-style: italic; font-weight: bold;");
        const auto data = jax.getFontData(source);
        expect(data.family == "TestMath, serif");
        expect(data.italic);
        expect(data.bold);

        style_json::StyleJsonSheet sheet;
        jax.addFontStyles(sheet);
        jax.addClassStyles(mml::NodeKind::mo, sheet);
        jax.addWrapperStyles(sheet);
        const auto rules = sheet.getStyleString();
        expect(rules.find(u".native-font") != std::u16string::npos);
    };
};
