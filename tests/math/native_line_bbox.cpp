#include "../support/folia_test.hpp"
import folia.math.core.mml_tree.mml_document;
import folia.math.output.common.line_bbox;
import folia.math.util.constants;

using namespace boost::ut;
using namespace folia::math;
using namespace folia::math::mml;
using namespace folia::math::output::common;

namespace {
struct LineBBoxProbe : LineBBox { using LineBBox::copyIndentData; };
}

suite native_line_bbox = [] {
    "BBox native numeric primitives preserve Source constructor and mutation values"_test = [] {
        BBox defaults;
        expect(defaults.w == 0.0 && defaults.h == -BIGDIMEN && defaults.d == -BIGDIMEN);
        BBox zero = BBox::zero();
        expect(zero.w == 0.0 && zero.h == 0.0 && zero.d == 0.0);
        BBox empty = BBox::makeEmpty();
        expect(empty.w == 0.0 && empty.h == -BIGDIMEN && empty.d == -BIGDIMEN);
        empty.clean();
        expect(empty.w == 0.0 && empty.h == 0.0 && empty.d == 0.0);
        BBox values{BBoxData{.w = 2.0, .h = 1.0, .d = 0.5}};
        values.scale = 2.0; values.rscale = 0.5; values.L = 0.25; values.R = 0.25;
        values.ic = 0.1; values.oc = 0.2; values.sk = 0.3; values.dx = 0.4;
        values.pwidth = "83%";
        BBox child{BBoxData{.w = 4.0, .h = 3.0, .d = 1.0}};
        child.rscale = 0.25; child.L = 0.5; child.R = 0.5;
        values.combine(child, 1.0, -0.5);
        expect(values.w == 2.25 && values.h == 1.0 && values.d == 0.75);
        values.append(child);
        expect(values.w == 3.5 && values.h == 1.0 && values.d == 0.75);
        BBox replacement{BBoxData{.w = 7.0, .h = 2.0, .d = 1.0}};
        replacement.pwidth = "40%";
        values.updateFrom(replacement);
        expect(values.w == 7.0 && values.h == 2.0 && values.d == 1.0);
        expect(values.pwidth == "40%");
        values.rescale(2.0);
        expect(values.w == 14.0 && values.h == 4.0 && values.d == 2.0);
        values.empty();
        expect(values.w == 0.0 && values.h == -BIGDIMEN && values.d == -BIGDIMEN);
        BBox copied = replacement.copy();
        replacement.w = 9.0;
        expect(copied.w == 7.0 && copied.h == 2.0 && copied.d == 1.0);
        expect(BBox::boxSides[0].name == "Top" && BBox::boxSides[3].dimension == "w");
        expect(BBox::fullWidth == "100%");
    };
    "LineBBox from and copy preserve source geometry and identity"_test = [] {
        BBox source{BBoxData{.w=2.4, .h=1.2, .d=0.3}};
        source.L=0.25; source.R=0.15; source.rscale=0.75; source.scale=1.3;
        source.ic=0.1; source.pwidth="40%";
        auto from=LineBBox::from(source, 0.2);
        expect(from.w==2.4 && from.h==1.2 && from.d==0.3 && from.L==0.25 && from.R==0.15);
        expect(from.rscale==0.75 && from.scale==1.3 && from.ic==0.1 && from.pwidth=="40%");
        expect(from.lineLeading==0.2 && from.originalL==0);
        LineBBox constructed{BBoxData{.w=1}, std::array<double,2>{5,6}};
        std::array<double,2> expected_start{5,6};
        expect(constructed.start==expected_start && constructed.originalL==0);

        auto shared=std::make_shared<IndentData>();
        shared->lines[1]={std::u16string(u"right"), std::u16string(u"0.5em")};
        from.indentData=shared; from.start=std::array<double,2>{1,2};
        from.end=std::array<double,2>{3,4}; from.originalL=0.7; from.isFirst=true;
        auto copied=from.copy();
        expect(copied.w==from.w && copied.h==from.h && copied.d==from.d && copied.L==from.L);
        expect(copied.indentData==shared && copied.originalL==0.7 && copied.isFirst);
        expect(copied.start==from.start && copied.end==from.end && copied.lineLeading==from.lineLeading);
        LineBBoxProbe probe;
        auto independent=probe.copyIndentData(from);
        expect(independent!=shared && *independent==*shared);
        independent->lines[1].alignment=u"left";
        expect(shared->lines[1].alignment==std::u16string(u"right"));
    };

    "LineBBox append retains Source dimensions and first-line mutation"_test = [] {
        auto shared=std::make_shared<IndentData>();
        LineBBox parent{BBoxData{.w=1, .h=0.2, .d=0.1}};
        parent.isFirst=true;
        LineBBox child{BBoxData{.w=2, .h=0.8, .d=0.2}};
        child.L=0.3; child.R=0.2; child.rscale=0.5; child.lineLeading=0.4;
        child.indentData=shared; child.isFirst=false;
        parent.append(child);
        expect(parent.w==2.1 && parent.h==0.4 && parent.d==0.1);
        expect(child.L==0 && child.originalL==0.3 && parent.indentData==shared);
        expect(parent.lineLeading==0.4 && !parent.isFirst);
        auto copied=parent.copy();
        expect(copied.w==2.1 && copied.h==0.4 && copied.d==0.1);
        expect(copied.indentData==shared && copied.lineLeading==0.4 && !copied.isFirst);
    };

    "LineBBox indentation preserves inherited current and absent values"_test = [] {
        MmlDocument document;
        auto& inherited=document.make(NodeKind::mrow);
        auto& attrs=inherited.attributes();
        attrs.set(u"indentalign", std::u16string(u"right"));
        attrs.set(u"indentshift", std::u16string(u"1em"));
        attrs.set(u"indentalignfirst", std::u16string(u"indentalign"));
        attrs.set(u"indentshiftfirst", std::u16string(u"indentshift"));
        attrs.set(u"indentalignlast", std::u16string(u"indentalign"));
        attrs.set(u"indentshiftlast", std::u16string(u"indentshift"));
        attrs.setInherited(u"indentalign", std::u16string(u"center"));
        attrs.setInherited(u"indentshift", std::u16string(u"2em"));
        LineBBox line; line.getIndentData(inherited);
        expect(line.indentData->lines[0].alignment==std::u16string(u"center"));
        expect(line.indentData->lines[0].shift==std::u16string(u"2em"));
        expect(line.indentData->lines[1].alignment==std::u16string(u"right"));
        expect(line.indentData->lines[1].shift==std::u16string(u"1em"));
        expect(line.indentData->lines[2]==line.indentData->lines[1]);

        auto& explicit_node=document.make(NodeKind::mrow);
        explicit_node.attributes().set(u"indentalignfirst", std::u16string(u"left"));
        explicit_node.attributes().set(u"indentshiftfirst", std::u16string(u"3em"));
        LineBBox explicit_line; explicit_line.getIndentData(explicit_node);
        expect(explicit_line.indentData->lines[0].alignment==std::u16string(u"left"));
        expect(!explicit_line.indentData->lines[1].alignment && !explicit_line.indentData->lines[1].shift);
        expect(!explicit_line.indentData->lines[2].alignment && !explicit_line.indentData->lines[2].shift);

        // Source reads getAllAttributes directly, while get() resolves the
        // special sentinel. LineBBox must retain the raw operation's value.
        auto& sentinelNode = document.make(NodeKind::mrow);
        sentinelNode.attributes().setGlobalDefault(u"indentalign", std::u16string(u"center"));
        sentinelNode.attributes().set(u"indentalign", std::u16string(u"_inherit_"));
        LineBBox sentinelLine;
        sentinelLine.getIndentData(sentinelNode);
        expect(sentinelLine.indentData->lines[1].alignment == std::u16string(u"_inherit_"));
        expect(std::get<std::u16string>(*sentinelNode.attributes().lookup(u"indentalign")) == u"center");

        auto& invalid=document.make(NodeKind::mrow);
        invalid.attributes().set(u"indentalign", true);
        bool rejected=false;
        try { LineBBox invalid_line; invalid_line.getIndentData(invalid); }
        catch (std::invalid_argument const&) { rejected=true; }
        expect(rejected);
    };
};

// Append to the existing native BBox owner; imports folia.math.util.bbox.
namespace {
using namespace boost::ut;
using folia::math::BBox;
using folia::math::BBoxData;

struct SourceBBoxSnapshot {
    std::array<double, 11> values;
    std::string_view pwidth;
};
enum class SourceBBoxOp { combine, append, update_from, rescale, empty };
struct SourceBBoxStep {
    SourceBBoxSnapshot child;
    SourceBBoxOp operation;
    double x, y;
};
struct SourceBBoxSide {
    std::string_view name;
    int index;
    std::string_view dimension;
};

#include "fixtures/tex-source/primitive_bbox_values.inc"

constexpr std::array<double BBox::*, 11> sourceBBoxFields{
    &BBox::w, &BBox::h, &BBox::d, &BBox::scale, &BBox::rscale, &BBox::L,
    &BBox::R, &BBox::ic, &BBox::oc, &BBox::sk, &BBox::dx};

bool sourcePrimitiveNumberEqual(double actual, double expected) {
    if (std::isnan(expected)) return std::isnan(actual);
    if (std::isinf(expected)) return actual == expected;
    if (expected == 0) {
        return actual == 0 && std::signbit(actual) == std::signbit(expected);
    }
    return std::isfinite(actual) &&
           std::abs(actual - expected) <= 1e-10 + 1e-10 * std::abs(expected);
}

void setSourceBBoxExtras(BBox &box, const SourceBBoxSnapshot &input) {
    for (std::size_t i = 3; i < sourceBBoxFields.size(); ++i) {
        box.*sourceBBoxFields[i] = input.values[i];
    }
    box.pwidth = input.pwidth;
}

suite native_primitive_bbox_values = [] {
    "native complete primitive bbox values"_test = [] {
        std::size_t snapshot = 0;
        const auto verify = [&](const BBox &box) {
            const auto &expected = sourceBBoxExpected[snapshot++];
            for (std::size_t i = 0; i < sourceBBoxFields.size(); ++i) {
                expect(sourcePrimitiveNumberEqual(
                    box.*sourceBBoxFields[i], expected.values[i]));
            }
            expect(box.pwidth == expected.pwidth);
        };
        for (const auto &definition : sourceBBoxDefinitions) {
            verify(BBox(definition));
        }

        BBox box = BBox::makeEmpty();
        verify(box);
        box.clean();
        verify(box);
        box = BBox::zero();
        verify(box);
        setSourceBBoxExtras(box, sourceBBoxZeroSetup);

        for (const auto &step : sourceBBoxSteps) {
            BBox child(BBoxData{
                .w = step.child.values[0],
                .h = step.child.values[1],
                .d = step.child.values[2]});
            setSourceBBoxExtras(child, step.child);
            switch (step.operation) {
            case SourceBBoxOp::combine: box.combine(child, step.x, step.y); break;
            case SourceBBoxOp::append: box.append(child); break;
            case SourceBBoxOp::update_from: box.updateFrom(child); break;
            case SourceBBoxOp::rescale: box.rescale(step.x); break;
            case SourceBBoxOp::empty: expect(&box.empty() == &box); break;
            }
            verify(box);
        }

        BBox copied = box.copy();
        verify(copied);
        box.w = 99;
        verify(copied);
        box = BBox(BBoxData{.w = 1, .h = 2, .d = 3});
        box.rscale = 0.7;
        box.L = 0.1;
        box.R = 0.2;
        box.append(box);
        verify(box);
        box.combine(box, 1, -1);
        verify(box);

        expect(snapshot == std::size(sourceBBoxExpected));
        expect(BBox::fullWidth == sourceBBoxFullWidth);
        for (std::size_t i = 0; i < std::size(sourceBBoxSides); ++i) {
            expect(BBox::boxSides[i].name == sourceBBoxSides[i].name);
            expect(BBox::boxSides[i].index == sourceBBoxSides[i].index);
            expect(BBox::boxSides[i].dimension == sourceBBoxSides[i].dimension);
        }
    };
};
}
