#include "support/folia_test.hpp"
import folia.mermaid.block_svg;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
suite block_regressions=[] {
    "mermaid_block_shape_delimiters_match_all_classic_handler_types"_test=[] {
        const auto doc=parse_block(uR"(block-beta
columns 4
a["Square"] b("Round") c(("Circle")) d{{"Hexagon"}}
e(["Stadium"]) f[["Subroutine"]] g[("Cylinder")] h((("Double")))
i[/"Right"/] j[\"Left"\] k[/"Trapezoid"\] l[\"Inverse"/]
m>"Note"] n{"Decision"} o<["Route"]>(x):2
classDef selected fill:#ffeecc,stroke:#883300
class g, m selected
)");
        expect(fatal(doc.has_value())) << (doc?"":doc.error());
        const std::vector<std::u16string> types{u"square",u"round",u"circle",u"hexagon",u"stadium",
            u"subroutine",u"cylinder",u"doublecircle",u"lean_right",u"lean_left",u"trapezoid",
            u"inv_trapezoid",u"rect_left_inv_arrow",u"diamond",u"block_arrow"};
        expect(fatal(doc->nodes.size()==types.size()+1));
        for(std::size_t i=0;i<types.size();++i) expect(bool(doc->nodes[i+1].type==types[i]));
        expect(doc->nodes[7].classes.size()==1_u);expect(doc->nodes[13].classes.size()==1_u);
        BlockRenderInput input;input.document=*doc;
        input.theme={u"Arial",u"#fff",u"#000",u"#000",u"#000",u"#eee",u"#999"};
        // Controlled boxes exercise geometry/paint dispatch, not font metrics.
        const auto svg=render_block_svg(input,[](std::u16string_view,double)->std::expected<BlockTextBox,std::string> {
            return BlockTextBox{80,80,16,12,16};
        });
        expect(fatal(svg.has_value())) << (svg?"":svg.error());
        for(const auto type:{"circle","hexagon","stadium","subroutine","cylinder","doublecircle","rect_left_inv_arrow"})
            expect(svg->svg.find(std::string{"data-block-shape=\""}+type+'"')!=svg->svg.npos);
        expect(svg->svg.find("<circle r=")!=svg->svg.npos);
        expect(svg->width>0 && svg->height>0);
    };
    "mermaid_block_edges_keep_start_end_markers_and_line_patterns"_test=[] {
        const auto doc=parse_block(uR"(block-beta
columns 3
A["A"] B["B"] C["C"]
A<-->B
B -.-> C
C -- "cross ends" x--x A
A ==> C
B -- "circle ends" o--o A
)");
        expect(fatal(doc.has_value())) << (doc?"":doc.error());
        expect(fatal(doc->edges.size()==5_u));
        expect(bool(doc->edges[0].arrow_start==u"arrow_point"));
        expect(bool(doc->edges[1].pattern==u"dotted"));
        expect(bool(doc->edges[2].arrow_start==u"arrow_cross"));
        expect(bool(doc->edges[2].arrow_end==u"arrow_cross"));
        expect(bool(doc->edges[3].thickness==u"thick"));
        expect(bool(doc->edges[4].arrow_start==u"arrow_circle"));
        expect(bool(doc->edges[4].arrow_end==u"arrow_circle"));
        BlockRenderInput input;input.document=*doc;
        input.theme={u"Arial",u"#fff",u"#000",u"#000",u"#000",u"#eee",u"#999"};
        const auto svg=render_block_svg(input,[](std::u16string_view,double)->std::expected<BlockTextBox,std::string> {
            return BlockTextBox{80,80,16,12,16};
        });
        expect(fatal(svg.has_value())) << (svg?"":svg.error());
        for(const auto name:{"point","circle","cross"}) {
            expect(svg->svg.find(std::string{"marker-start=\"url(#block-"}+name+"-start)\"")!=svg->svg.npos);
            expect(svg->svg.find(std::string{"marker-end=\"url(#block-"}+name+"-end)\"")!=svg->svg.npos);
        }
        expect(svg->svg.find("stroke-width=\"3.5\"")!=svg->svg.npos);
        expect(svg->svg.find("stroke-dasharray=\"2\"")!=svg->svg.npos);
    };
};
}
