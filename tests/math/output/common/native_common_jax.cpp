#include "math/support/common_jax_test.hpp"

suite native_common_jax_behavior = [] {
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
