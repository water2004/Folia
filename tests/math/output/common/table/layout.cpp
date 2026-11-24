#include "math/support/common_table_test.hpp"

suite common_table_layout = [] {
    "fixed_column_break_matches_locked_text_break_measurements"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        RejectMeasurement measurement;
        mml::MmlDocument doc;
        auto &text = doc.factory().make(mml::NodeKind::text);
        text.setText(u"x x");
        mml::MmlNode *textChildren[]{&text};
        auto &content = doc.factory().make(mml::NodeKind::mtext, {}, textChildren);
        auto &inferred = inferredCellContent(doc, content);
        mml::MmlNode *cellChildren[]{&inferred};
        auto &cell = doc.factory().make(mml::NodeKind::mtd, {}, cellChildren);
        mml::MmlNode *rowCells[]{&cell};
        auto &row = doc.factory().make(mml::NodeKind::mtr, {}, rowCells);
        mml::MmlNode *rows[]{&row};
        mml::AttributeSet tableAttrs;
        setArrayDefaults(tableAttrs, u"center");
        tableAttrs.set(u"width", u"auto");
        tableAttrs.set(u"columnwidth", u".5em");
        auto &tableNode = doc.factory().make(mml::NodeKind::mtable, std::move(tableAttrs), rows);
        expect(tableNode.properties().table.use_height.value_or(false));
        mml::AttributeSet mathAttrs;
        mathAttrs.set(u"overflow", u"linebreak");
        auto &mathNode = mathTableRoot(doc, tableNode, std::move(mathAttrs));
        doc.setRoot(mathNode);
        mathNode.setInheritedAttributes();
        common::CommonOptions options;
        options.linebreaks.inlineBreaks = true;
        options.displayOverflow = "linebreak";
        common::CommonWrapperCatalog catalog{
            reg<common::CommonMath>(mml::NodeKind::math),
            reg<common::CommonMtable>(mml::NodeKind::mtable),
            reg<common::CommonMtr>(mml::NodeKind::mtr),
            reg<common::CommonMtd>(mml::NodeKind::mtd),
            reg<common::CommonMtext>(mml::NodeKind::mtext),
            reg<common::CommonTextNode>(mml::NodeKind::text),
            reg<common::CommonMrow>(mml::NodeKind::mrow),
            reg<common::CommonInferredMrow>(mml::NodeKind::inferred_mrow)};
        common::CommonOutputJaxConfig config{
            {16, 8, 1280, 1, "MathJaxNewcm", true}, options, bundle.fonts, "MathJaxNewcm"};
        common::LayoutJob job(doc, config, measurement, std::move(catalog), makeLinebreakVisitor);
        (void)job.build();
        auto *table = dynamic_cast<common::CommonMtable *>(job.jax().wrapperFor(tableNode));
        auto *rowWrapper = dynamic_cast<common::CommonMtr *>(job.jax().wrapperFor(row));
        expect(table != nullptr);
        expect(rowWrapper != nullptr);
        if (!table || !rowWrapper)
            return;
        auto *cellWrapper = rowWrapper->getChild(0);
        expect(cellWrapper != nullptr);
        if (!cellWrapper || cellWrapper->childNodes.empty())
            return;
        auto *line = cellWrapper->childNodes.front();
        expect(line->breakCount() == 1_u);
        expect(eq(cellWrapper->getBBox().w, .528));
        expect(eq(cellWrapper->getBBox().h, .75));
        expect(eq(cellWrapper->getBBox().d, 1.45));
        expect(eq(std::get<double>(table->cWidths[0]), .528));
        expect(eq(table->getBBox().w, 1.528));
        expect(eq(table->getBBox().h, 1.475));
        expect(eq(table->getBBox().d, .9750000000000001));
    };

    "table_stretches_real_NewCM_delimiters_to_the_locked_matrix_dimensions"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        RejectMeasurement measurement;
        mml::MmlDocument doc;
        const auto makeMi = [&](std::u16string value) -> mml::MmlNode & {
            auto &text = doc.factory().make(mml::NodeKind::text);
            text.setText(std::move(value));
            mml::MmlNode *children[]{&text};
            return doc.factory().make(mml::NodeKind::mi, {}, children);
        };
        const auto makeFrac = [&](std::u16string numerator,
                                  std::u16string denominator) -> mml::MmlNode & {
            auto &n = makeMi(std::move(numerator));
            auto &d = makeMi(std::move(denominator));
            mml::MmlNode *children[]{&n, &d};
            return doc.factory().make(mml::NodeKind::mfrac, {}, children);
        };
        const auto makeFence = [&](std::u16string glyph, std::u16string form) -> mml::MmlNode & {
            auto &text = doc.factory().make(mml::NodeKind::text);
            text.setText(std::move(glyph));
            mml::MmlNode *children[]{&text};
            mml::AttributeSet attrs;
            attrs.set(u"stretchy", true);
            attrs.set(u"symmetric", true);
            attrs.set(u"fence", true);
            attrs.set(u"form", std::move(form));
            return doc.factory().make(mml::NodeKind::mo, std::move(attrs), children);
        };
        auto &left = makeFence(u"(", u"prefix");
        auto &numerator = makeFrac(u"x", u"y");
        auto &right = makeFence(u")", u"postfix");
        left.properties().tex.explicit_tex_class = mml::TexClass::open;
        right.properties().tex.explicit_tex_class = mml::TexClass::close;
        mml::MmlNode *bracketChildren[]{&left, &numerator, &right};
        auto &bracketed = doc.factory().make(mml::NodeKind::mrow, {}, bracketChildren);
        bracketed.properties().tex.open = u"(";
        bracketed.properties().tex.close = u")";
        bracketed.properties().tex.explicit_tex_class = mml::TexClass::inner;
        auto &other = makeFrac(u"a", u"b");
        auto &bracketContent = inferredCellContent(doc, bracketed);
        auto &otherContent = inferredCellContent(doc, other);
        mml::MmlNode *firstCells[]{&bracketContent, &otherContent};
        auto &row = doc.factory().make(mml::NodeKind::mtr, {}, firstCells);
        mml::MmlNode *rows[]{&row};
        mml::AttributeSet tableAttrs;
        tableAttrs.set(u"columnspacing", u"1em");
        tableAttrs.set(u"rowspacing", u"4pt");
        auto &tableNode = doc.factory().make(mml::NodeKind::mtable, std::move(tableAttrs), rows);
        expect(tableNode.properties().table.use_height.value_or(false));
        auto &mathNode = mathTableRoot(doc, tableNode);
        doc.setRoot(mathNode);
        mathNode.setInheritedAttributes();
        common::CommonWrapperCatalog catalog{
            reg<common::CommonMath>(mml::NodeKind::math),
            reg<common::CommonMtable>(mml::NodeKind::mtable),
            reg<common::CommonMtr>(mml::NodeKind::mtr),
            reg<common::CommonMtd>(mml::NodeKind::mtd),
            reg<common::CommonMrow>(mml::NodeKind::mrow),
            reg<common::CommonMi>(mml::NodeKind::mi),
            reg<common::CommonMfrac>(mml::NodeKind::mfrac),
            reg<common::CommonMo>(mml::NodeKind::mo),
            reg<common::CommonTextNode>(mml::NodeKind::text),
            reg<common::CommonInferredMrow>(mml::NodeKind::inferred_mrow)};
        common::CommonOutputJaxConfig config{
            {16, 8, 1280, 1, "MathJaxNewcm", true}, sourceOptions(), bundle.fonts, "MathJaxNewcm"};
        common::LayoutJob job(doc, config, measurement, std::move(catalog), makeLinebreaks);
        (void)job.build();
        auto *table = dynamic_cast<common::CommonMtable *>(job.jax().wrapperFor(tableNode));
        auto *leftWrapper = dynamic_cast<common::CommonMo *>(job.jax().wrapperFor(left));
        auto *rightWrapper = dynamic_cast<common::CommonMo *>(job.jax().wrapperFor(right));
        expect(table != nullptr);
        expect(leftWrapper != nullptr);
        expect(rightWrapper != nullptr);
        if (!table || !leftWrapper || !rightWrapper)
            return;
        expect(leftWrapper->canStretch(common::StretchDirection::vertical));
        expect(rightWrapper->canStretch(common::StretchDirection::vertical));
        for (auto* delimiter : {leftWrapper, rightWrapper}) {
            expect(delimiter->size.has_value());
            if (delimiter->size) {
                auto* selected = std::get_if<common::FixedStretchSize>(&*delimiter->size);
                expect(selected != nullptr);
                if (selected)
                    expect(selected->index == 3_u);
            }
            expect(delimiter->variant == "-size3");
        }
        expect(eq(table->getTableData().H[0], .972));
        expect(eq(table->getTableData().D[0], .4899568901432422));
        expect(eq(table->getTableData().W[0], 1.8904650788387052));
        expect(eq(table->getTableData().W[1], .8140594872476836));
        expect(eq(table->getBBox().w, 3.7045245660863886));
        expect(eq(table->getBBox().h, .980978445071621));
        expect(eq(table->getBBox().d, .480978445071621));
    };

    "table_breaks_an_operator_expression_at_the_locked_long_case_width"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        RejectMeasurement measurement;
        mml::MmlDocument doc;
        std::vector<mml::MmlNode *> expression;
        const auto makeToken = [&](mml::NodeKind kind, std::u16string textValue) -> mml::MmlNode & {
            auto &text = doc.factory().make(mml::NodeKind::text);
            text.setText(std::move(textValue));
            mml::MmlNode *children[]{&text};
            return doc.factory().make(kind, {}, children);
        };
        for (char16_t ch : std::u16string_view(u"abcdefghij"))
            expression.push_back(&makeToken(mml::NodeKind::mi, std::u16string(1, ch)));
        expression.push_back(&makeToken(mml::NodeKind::mo, u"+"));
        for (char16_t ch : std::u16string_view(u"klmnopqrst"))
            expression.push_back(&makeToken(mml::NodeKind::mi, std::u16string(1, ch)));
        auto &rowContent = doc.factory().make(mml::NodeKind::inferred_mrow, {}, expression);
        mml::MmlNode *cellChildren[]{&rowContent};
        auto &cell = doc.factory().make(mml::NodeKind::mtd, {}, cellChildren);
        mml::MmlNode *rowCells[]{&cell};
        auto &row = doc.factory().make(mml::NodeKind::mtr, {}, rowCells);
        mml::MmlNode *rows[]{&row};
        mml::AttributeSet tableAttrs;
        setArrayDefaults(tableAttrs, u"center");
        tableAttrs.set(u"width", u"auto");
        tableAttrs.set(u"columnwidth", u"2em");
        auto &tableNode = doc.factory().make(mml::NodeKind::mtable, std::move(tableAttrs), rows);
        expect(tableNode.properties().table.use_height.value_or(false));
        mml::AttributeSet mathAttrs;
        mathAttrs.set(u"overflow", u"linebreak");
        auto &mathNode = mathTableRoot(doc, tableNode, std::move(mathAttrs));
        doc.setRoot(mathNode);
        mathNode.setInheritedAttributes();
        common::CommonOptions options;
        options.linebreaks.inlineBreaks = true;
        options.displayOverflow = "linebreak";
        common::CommonWrapperCatalog catalog{
            reg<common::CommonMath>(mml::NodeKind::math),
            reg<common::CommonMtable>(mml::NodeKind::mtable),
            reg<common::CommonMtr>(mml::NodeKind::mtr),
            reg<common::CommonMtd>(mml::NodeKind::mtd),
            reg<common::CommonMrow>(mml::NodeKind::mrow),
            reg<common::CommonInferredMrow>(mml::NodeKind::inferred_mrow),
            reg<common::CommonMi>(mml::NodeKind::mi),
            reg<common::CommonMo>(mml::NodeKind::mo),
            reg<common::CommonTextNode>(mml::NodeKind::text)};
        common::CommonOutputJaxConfig config{
            {16, 8, 100, 1, "MathJaxNewcm", true}, options, bundle.fonts, "MathJaxNewcm"};
        common::LayoutJob job(doc, config, measurement, std::move(catalog), makeLinebreakVisitor);
        (void)job.build();
        auto *table = dynamic_cast<common::CommonMtable *>(job.jax().wrapperFor(tableNode));
        auto *rowWrapper = dynamic_cast<common::CommonMtr *>(job.jax().wrapperFor(row));
        expect(table != nullptr);
        expect(rowWrapper != nullptr);
        if (!table || !rowWrapper)
            return;
        auto *cellWrapper = rowWrapper->getChild(0);
        expect(cellWrapper != nullptr);
        if (!cellWrapper || cellWrapper->childNodes.empty())
            return;
        expect(cellWrapper->childNodes.front()->breakCount() == 1_u);
        expect(eq(cellWrapper->getBBox().w, 6.018222222222222));
        expect(eq(cellWrapper->getBBox().h, .75));
        expect(eq(cellWrapper->getBBox().d, 1.45));
        expect(eq(table->getTableData().W[0], 10.979444444444445));
        expect(eq(table->cWidths[0].index() == 1 ? std::get<double>(table->cWidths[0]) : 0,
                  6.018222222222222));
        expect(eq(table->getBBox().w, 7.018222222222222));
        expect(eq(table->getBBox().h, 1.475));
        expect(eq(table->getBBox().d, .9750000000000001));
    };


};
