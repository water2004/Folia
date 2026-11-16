#include "support/folia_test.hpp"
import folia.mermaid.block_svg;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

// This is an opaque label contract fixture with explicit box metrics. It does
// not measure fonts or implement TeX. The separate native integration target
// renders real MathJax formulas through the production C ABI.
MathLabel box_label(double height=80) {
    MathFragment fragment{
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"90\" height=\""+
        std::to_string(height)+
        "\" viewBox=\"0 0 90 80\" preserveAspectRatio=\"none\"><path d=\"M0 0H90V80H0Z\"/></svg>",
        90,height,height-8};
    MathLabelAtom atom;
    atom.content=std::move(fragment);
    atom.width=90;atom.height=height;atom.baseline=height-8;
    MathLabelLine line;
    line.atoms.push_back(std::move(atom));
    line.width=90;line.height=height;
    return {{std::move(line)},90,height,16};
}

BlockRenderInput input(std::u16string_view source) {
    auto session=ConfigSession::create();
    expect(fatal(session.has_value()));
    auto prepared=prepare_block(source,*session);
    expect(fatal(prepared.has_value()))<<(prepared?"":prepared.error());
    return std::move(*prepared);
}

BlockMeasure reject_text() {
    return [](std::u16string_view,double)->std::expected<BlockTextBox,std::string> {
        return std::unexpected("Unexpected ordinary text measurement");
    };
}

std::size_t count(std::string_view text,std::string_view needle) {
    std::size_t total=0,at=0;
    while((at=text.find(needle,at))!=text.npos) {++total;at+=needle.size();}
    return total;
}

suite block_math=[] {
    "mermaid_block_math_boxes_determine_shapes_and_viewport"_test=[] {
        const MathLabelMeasure math=[](std::u16string_view,std::u16string_view,
            double,unsigned,bool)->std::expected<std::optional<MathLabel>,std::string> {
            return box_label();
        };
        for(const auto source:{u"block-beta\nA[\"$$x$$\"]",
            u"block-beta\nA{\"$$x$$\"}",u"block-beta\nA<[\"$$x$$\"]>(right)"}) {
            auto prepared=input(source);
            auto svg=render_block_svg(prepared,reject_text(),math);
            expect(fatal(svg.has_value()))<<(svg?"":svg.error());
            expect(svg->height>=106.0);
            expect(svg->svg.find("block-math-label")!=std::string::npos);
            expect(svg->svg.find("<foreignObject")==std::string::npos);
            expect(svg->svg.find("$$")==std::string::npos);
            if(prepared.document.nodes[1].type==u"diamond") {
                expect(svg->height>=196.0);
                expect(svg->svg.find("93,0 186,-93 93,-186 0,-93")!=std::string::npos);
            }
        }
    };

    "mermaid_block_math_labels_cover_nodes_edges_and_composites"_test=[] {
        auto prepared=input(u"block-beta\ncolumns 2\n"
            u"block:G[\"$$title$$\"]\nA[\"$$x$$\"]\nend\n"
            u"B[\"$$x$$\"]\nA -- \"$$edge$$\" --> B");
        std::size_t calls=0;
        const MathLabelMeasure math=[&](std::u16string_view text,std::u16string_view family,
            double size,unsigned weight,bool italic)
            ->std::expected<std::optional<MathLabel>,std::string> {
            ++calls;
            expect(family==prepared.theme.font_family);
            expect(size==prepared.config.font_size);
            expect(weight==400_u&&!italic);
            return box_label(text==u"$$title$$"?160:80);
        };
        auto svg=render_block_svg(prepared,reject_text(),math);
        expect(fatal(svg.has_value()))<<(svg?"":svg.error());
        expect(calls==4_u);
        expect(count(svg->svg,"class=\"block-math-label\"")==4_u);
        expect(svg->height>=186.0);
        expect(svg->svg.find("$$")==std::string::npos);
    };

    "mermaid_block_math_honors_html_labels_and_plain_fallback"_test=[] {
        auto prepared=input(u"---\nconfig:\n  htmlLabels: false\n---\nblock-beta\nA[\"$$x$$\"]");
        expect(!prepared.config.html_labels);
        std::size_t calls=0;
        const MathLabelMeasure math=[&](std::u16string_view,std::u16string_view,
            double,unsigned,bool)->std::expected<std::optional<MathLabel>,std::string> {
            ++calls;return box_label();
        };
        // Explicit host fixture metrics, independent of the source's length.
        const BlockMeasure plain=[](std::u16string_view,double)
            ->std::expected<BlockTextBox,std::string> {return BlockTextBox{40,40,16,12,16};};
        auto svg=render_block_svg(prepared,plain,math);
        expect(fatal(svg.has_value()));
        expect(calls==0_u);
        expect(svg->svg.find("$$x$$")!=std::string::npos);

        prepared.config.html_labels=true;
        const MathLabelMeasure declined=[](std::u16string_view,std::u16string_view,
            double,unsigned,bool)->std::expected<std::optional<MathLabel>,std::string> {
            return std::nullopt;
        };
        auto with_provider=render_block_svg(prepared,plain,declined);
        auto without_provider=render_block_svg(prepared,plain);
        expect(fatal(with_provider.has_value()&&without_provider.has_value()));
        expect(with_provider->svg==without_provider->svg);
    };

    "mermaid_block_math_errors_propagate_without_literal_measurement"_test=[] {
        auto prepared=input(u"block-beta\nA[\"$$x$$\"]");
        const MathLabelMeasure failed=[](std::u16string_view,std::u16string_view,
            double,unsigned,bool)->std::expected<std::optional<MathLabel>,std::string> {
            return std::unexpected("Math rendering failed");
        };
        auto svg=render_block_svg(prepared,reject_text(),failed);
        expect(!svg&&svg.error()=="Math rendering failed");
    };

    "mermaid_block_math_edges_honor_effective_html_without_changing_node_gate"_test=[] {
        struct ConfigCase {
            std::u16string_view yaml;
            bool nodes,edges;
        };
        constexpr ConfigCase cases[]={
            {u"{}",true,true},
            {u"{flowchart: {htmlLabels: false}}",true,false},
            {u"{flowchart: {htmlLabels: true}}",true,true},
            {u"{htmlLabels: true, flowchart: {htmlLabels: false}}",true,true},
            {u"{htmlLabels: false, flowchart: {htmlLabels: true}}",false,false},
            {u"{htmlLabels: null, flowchart: {htmlLabels: false}}",true,false},
        };
        for(const auto& config:cases) {
            auto prepared=input(u"---\nconfig: "+std::u16string(config.yaml)+
                u"\n---\nblock-beta\nA[\"$$node$$\"] B[\"$$node$$\"]\n"
                u"A -- \"$$edge$$\" --> B");
            expect(prepared.config.html_labels==config.nodes);
            expect(prepared.config.edge_html_labels==config.edges);
            std::size_t node_calls=0,edge_calls=0,plain_calls=0;
            const MathLabelMeasure math=[&](std::u16string_view source,std::u16string_view,
                double,unsigned,bool)->std::expected<std::optional<MathLabel>,std::string> {
                if(source==u"$$edge$$") ++edge_calls;
                else {expect(source==u"$$node$$");++node_calls;}
                return box_label();
            };
            const BlockMeasure plain=[&](std::u16string_view,double)
                ->std::expected<BlockTextBox,std::string> {
                ++plain_calls;
                return BlockTextBox{40,40,16,12,16};
            };
            auto svg=render_block_svg(prepared,plain,math);
            expect(fatal(svg.has_value()))<<(svg?"":svg.error());
            expect(node_calls==std::size_t(config.nodes?2:0));
            expect(edge_calls==std::size_t(config.edges?1:0));
            expect(plain_calls==3-node_calls-edge_calls);
            expect((svg->svg.find("$$edge$$")!=std::string::npos)==!config.edges);
            expect((svg->svg.find("$$node$$")!=std::string::npos)==!config.nodes);
        }
    };
};
}
