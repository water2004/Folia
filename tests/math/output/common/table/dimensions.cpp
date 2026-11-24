#include "math/support/common_table_test.hpp"

suite common_table_dimensions = [] {
    "factory_created_table_keeps_rows_cells_dimensions_spacing_and_frame"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        RejectMeasurement measurement;
        mml::MmlDocument doc;
        mml::AttributeSet s1;
        s1.set(u"width", u"1.25em");
        s1.set(u"height", u".8em");
        s1.set(u"depth", u".2em");
        auto &a = doc.factory().make(mml::NodeKind::mspace, std::move(s1));
        mml::AttributeSet s2;
        s2.set(u"width", u".7em");
        s2.set(u"height", u".3em");
        s2.set(u"depth", u".1em");
        auto &b = doc.factory().make(mml::NodeKind::mspace, std::move(s2));
        auto &inferredA = inferredCellContent(doc, a);
        auto &inferredB = inferredCellContent(doc, b);
        mml::MmlNode *ca[]{&inferredA};
        mml::MmlNode *cb[]{&inferredB};
        auto &cellA = doc.factory().make(mml::NodeKind::mtd, {}, ca);
        auto &cellB = doc.factory().make(mml::NodeKind::mtd, {}, cb);
        mml::MmlNode *rowCells[]{&cellA, &cellB};
        auto &row = doc.factory().make(mml::NodeKind::mtr, {}, rowCells);
        mml::AttributeSet s3;
        s3.set(u"width", u".7em");
        s3.set(u"height", u".3em");
        s3.set(u"depth", u".1em");
        auto &c = doc.factory().make(mml::NodeKind::mspace, std::move(s3));
        mml::AttributeSet s4;
        s4.set(u"width", u"1.25em");
        s4.set(u"height", u".8em");
        s4.set(u"depth", u".2em");
        auto &d = doc.factory().make(mml::NodeKind::mspace, std::move(s4));
        auto &inferredC = inferredCellContent(doc, c);
        auto &inferredD = inferredCellContent(doc, d);
        mml::MmlNode *cc[]{&inferredC};
        mml::MmlNode *cd[]{&inferredD};
        auto &cellC = doc.factory().make(mml::NodeKind::mtd, {}, cc);
        auto &cellD = doc.factory().make(mml::NodeKind::mtd, {}, cd);
        mml::MmlNode *rowCells2[]{&cellC, &cellD};
        auto &row2 = doc.factory().make(mml::NodeKind::mtr, {}, rowCells2);
        mml::MmlNode *rows[]{&row, &row2};
        mml::AttributeSet attrs;
        attrs.set(u"columnspacing", u".4em");
        attrs.set(u"rowspacing", u".3em");
        attrs.set(u"columnlines", u"solid");
        attrs.set(u"rowlines", u"solid");
        attrs.set(u"frame", u"solid");
        attrs.set(u"framespacing", u".5em .6em");
        attrs.set(u"width", u"auto");
        attrs.set(u"align", u"axis");
        auto &tableNode = doc.factory().make(mml::NodeKind::mtable, std::move(attrs), rows);
        expect(tableNode.properties().table.use_height.value_or(false));
        auto &mathNode = mathTableRoot(doc, tableNode);
        doc.setRoot(mathNode);
        mathNode.setInheritedAttributes();
        common::CommonOutputJaxConfig config{
            {16, 8, 1280, 1, "MathJaxNewcm", true}, sourceOptions(), bundle.fonts, "MathJaxNewcm"};
        common::LayoutJob job(doc, config, measurement, tableCatalog(), makeLinebreaks);
        auto &root = job.build();
        auto *table = dynamic_cast<common::CommonMtable *>(job.jax().wrapperFor(tableNode));
        auto *rowWrapper = dynamic_cast<common::CommonMtr *>(job.jax().wrapperFor(row));
        auto *firstCell = dynamic_cast<common::CommonMtd *>(job.jax().wrapperFor(cellA));
        expect(table != nullptr);
        expect(rowWrapper != nullptr);
        expect(firstCell != nullptr);
        if (!table || !rowWrapper || !firstCell)
            return;
        expect(root.node == &mathNode);
        expect(table->numRows == 2_i);
        expect(table->numCols == 2_i);
        expect(table->tableRows().size() == 2_i);
        expect(rowWrapper->numCells() == 2_i);
        expect(rowWrapper->getChild(0) == job.jax().wrapperFor(cellA));
        expect(rowWrapper->getChildBBoxes().size() == 2_i);
        const auto &dimensions = table->getTableData();
        expect(dimensions.W.size() == 2_i);
        expect(dimensions.H.size() == 2_i);
        expect(dimensions.D.size() == 2_i);
        expect(eq(dimensions.W[0], 1.25));
        expect(eq(dimensions.W[1], 1.25));
        expect(eq(dimensions.H[0], .8));
        expect(eq(dimensions.H[1], .8));
        expect(eq(dimensions.D[0], .25));
        expect(eq(dimensions.D[1], .25));
        expect(table->cSpace.size() == 1_i);
        expect(eq(table->cSpace[0], .4));
        expect(table->rSpace.size() == 1_i);
        expect(eq(table->rSpace[0], .3));
        expect(table->frame);
        expect(table->fframe);
        expect(eq(table->fLine, .07));
        expect(eq(table->fSpace[0], .5));
        expect(eq(table->fSpace[1], .6));
        expect(table->cLines.size() == 1_i);
        expect(eq(table->cLines[0], .07));
        expect(table->rLines.size() == 1_i);
        expect(eq(table->rLines[0], .07));
        expect(eq(firstCell->getWrapWidth(0), dimensions.W[0]));
        expect(!firstCell->fixesPWidth());
        expect(job.jax().table() == table);
        expect(eq(table->getBBox().w, 4.109999999999999));
        expect(eq(table->getBBox().h, 2.155));
        expect(eq(table->getBBox().d, 1.6549999999999998));
    };

    "table_width_modes_match_locked_NewCM_Source_values"_test = [] {
        struct WidthCase {
            std::u16string width, columns;
            bool equal;
            double first, second, tableWidth;
        };
        const WidthCase cases[]{{u"4em", u"auto fit", false, 1.25, 1.25, 4.5},
                                {u"75%", u"30% auto", false, 15.312, 35.728, 53.04},
                                {u"auto", u"", true, 1.25, 1.25, 4.5}};
        auto bundle = folia::math::fonts::makeNewcmFontData();
        for (const auto &expected : cases) {
            RejectMeasurement measurement;
            mml::MmlDocument doc;
            const auto makeCell = [&](double width, double height, double depth) -> mml::MmlNode & {
                mml::AttributeSet spaceAttrs;
                spaceAttrs.set(u"width", width == 1.25 ? u"1.25em" : u".7em");
                spaceAttrs.set(u"height", height == .8 ? u".8em" : u".3em");
                spaceAttrs.set(u"depth", depth == .2 ? u".2em" : u".1em");
                auto &space = doc.factory().make(mml::NodeKind::mspace, std::move(spaceAttrs));
                auto &inferred = inferredCellContent(doc, space);
                mml::MmlNode *children[]{&inferred};
                return doc.factory().make(mml::NodeKind::mtd, {}, children);
            };
            auto &a = makeCell(1.25, .8, .2);
            auto &b = makeCell(.7, .3, .1);
            auto &c = makeCell(.7, .3, .1);
            auto &d = makeCell(1.25, .8, .2);
            mml::MmlNode *firstCells[]{&a, &b};
            mml::MmlNode *secondCells[]{&c, &d};
            auto &firstRow = doc.factory().make(mml::NodeKind::mtr, {}, firstCells);
            auto &secondRow = doc.factory().make(mml::NodeKind::mtr, {}, secondCells);
            mml::MmlNode *rows[]{&firstRow, &secondRow};
            mml::AttributeSet tableAttrs;
            setArrayDefaults(tableAttrs, u"center center");
            tableAttrs.set(u"width", expected.width);
            if (!expected.columns.empty())
                tableAttrs.set(u"columnwidth", expected.columns);
            if (expected.equal)
                tableAttrs.set(u"equalcolumns", true);
            auto &tableNode =
                doc.factory().make(mml::NodeKind::mtable, std::move(tableAttrs), rows);
            expect(tableNode.properties().table.use_height.value_or(false));
            auto &mathNode = mathTableRoot(doc, tableNode);
            doc.setRoot(mathNode);
            mathNode.setInheritedAttributes();
            common::CommonOutputJaxConfig config{{16, 8, 1280, 1, "MathJaxNewcm", true},
                                                 sourceOptions(),
                                                 bundle.fonts,
                                                 "MathJaxNewcm"};
            common::LayoutJob job(doc, config, measurement, tableCatalog(), makeLinebreaks);
            auto& root = job.build();
            (void)root.getBBox();
            auto *table = dynamic_cast<common::CommonMtable *>(job.jax().wrapperFor(tableNode));
            expect(table != nullptr);
            if (!table)
                continue;
            const auto &data = table->getTableData();
            expect(eq(data.W[0], 1.25));
            expect(eq(data.W[1], 1.25));
            const auto *first = std::get_if<double>(&table->cWidths[0]);
            const auto *second = std::get_if<double>(&table->cWidths[1]);
            expect(first != nullptr);
            expect(second != nullptr);
            if (first)
                expect(eq(*first, expected.first));
            if (second)
                expect(eq(*second, expected.second));
            expect(eq(table->getBBox().w, expected.tableWidth));
        }
    };

    "labeled_rows_keep_the_label_slot_out_of_column_metrics"_test = [] {
        auto bundle = folia::math::fonts::makeNewcmFontData();
        RejectMeasurement measurement;
        mml::MmlDocument doc;
        const auto makeMeasuredCell = [&](std::u16string width, std::u16string height,
                                          std::u16string depth) -> mml::MmlNode & {
            mml::AttributeSet attrs;
            attrs.set(u"width", std::move(width));
            attrs.set(u"height", std::move(height));
            attrs.set(u"depth", std::move(depth));
            auto &space = doc.factory().make(mml::NodeKind::mspace, std::move(attrs));
            auto &inferred = inferredCellContent(doc, space);
            mml::MmlNode *children[]{&inferred};
            return doc.factory().make(mml::NodeKind::mtd, {}, children);
        };
        auto &labelA = makeMeasuredCell(u"1.528em", u".748em", u".248em");
        auto &a = makeMeasuredCell(u".572em", u".442em", u".011em");
        auto &b = makeMeasuredCell(u"1.8335555555555554em", u".666em", u"0em");
        mml::MmlNode *firstCells[]{&labelA, &a, &b};
        auto &first = doc.factory().make(mml::NodeKind::mlabeledtr, {}, firstCells);
        auto &labelB = makeMeasuredCell(u"1.486em", u".748em", u".248em");
        auto &c = makeMeasuredCell(u".49em", u".442em", u".205em");
        auto &d = makeMeasuredCell(u"1.8335555555555554em", u".666em", u"0em");
        mml::MmlNode *secondCells[]{&labelB, &c, &d};
        auto &second = doc.factory().make(mml::NodeKind::mlabeledtr, {}, secondCells);
        mml::MmlNode *rows[]{&first, &second};
        mml::AttributeSet tableAttrs;
        tableAttrs.set(u"displaystyle", true);
        tableAttrs.set(u"columnalign", u"right left");
        tableAttrs.set(u"columnspacing", u"0em");
        tableAttrs.set(u"rowspacing", u"3pt");
        tableAttrs.set(u"data-break-align", u"bottom top");
        auto &tableNode = doc.factory().make(mml::NodeKind::mtable, std::move(tableAttrs), rows);
        expect(tableNode.properties().table.use_height.value_or(false));
        auto &mathNode = mathTableRoot(doc, tableNode);
        doc.setRoot(mathNode);
        mathNode.setInheritedAttributes();
        common::CommonOutputJaxConfig config{
            {16, 8, 1280, 1, "MathJaxNewcm", true}, sourceOptions(), bundle.fonts, "MathJaxNewcm"};
        common::LayoutJob job(doc, config, measurement, tableCatalog(), makeLinebreaks);
        (void)job.build();
        auto *table = dynamic_cast<common::CommonMtable *>(job.jax().wrapperFor(tableNode));
        auto *row = dynamic_cast<common::CommonMlabeledtr *>(job.jax().wrapperFor(first));
        expect(table != nullptr);
        expect(row != nullptr);
        if (!table || !row)
            return;
        expect(table->hasLabels);
        expect(table->numRows == 2_u);
        expect(table->numCols == 2_u);
        expect(row->labeled());
        expect(row->numCells() == 2_u);
        expect(row->tableCells().size() == 2_u);
        expect(eq(table->getTableData().L, 1.528));
        expect(eq(table->getTableData().W[0], .572));
        expect(eq(table->getTableData().W[1], 1.8335555555555554));
    };


};
