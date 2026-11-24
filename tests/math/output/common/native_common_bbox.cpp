#include "math/support/common_jax_test.hpp"

suite native_common_bbox_behavior = [] {
    "common subtree bbox matches real Source profile without changing document root or tree"_test =
        [] {
            auto bundle = folia::math::fonts::makeNewcmFontData();
            for (auto const &entry : math_test::fixtures::bboxSourceCases()) {
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
};
