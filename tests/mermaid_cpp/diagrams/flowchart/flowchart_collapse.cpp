#include "support/folia_test.hpp"
import folia.mermaid.flowchart_svg;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
suite flowchart_collapse=[] {
    "mermaid_flowchart_collapsed_groups_project_nested_boundaries_and_keep_real_loops"_test=[] {
        auto session=ConfigSession::create();expect(fatal(session.has_value()));
        const auto doc=prepare_flowchart(uR"(flowchart LR
External --> Hidden
subgraph Outer[Outer]
  subgraph Inner[Inner]
    Hidden --> Work
  end
  Tail
end
Inner@{ view: collapsed }
Outer@{ view: collapsed }
Work --> Sink
Outer --> Outer
Visible --> Visible
)",*session);
        expect(fatal(doc.has_value())) << (doc?"":doc.error());
        expect(doc->nodes.size()==4_u);expect(fatal(doc->edges.size()==4_u));
        expect(bool(doc->nodes.front().id==u"Outer"));
        expect(bool(doc->nodes.front().shape==u"collapsedGroup"));
        expect(!doc->nodes.front().group);
        expect(bool(doc->edges[0].start==u"External"&&doc->edges[0].end==u"Outer"));
        expect(bool(doc->edges[1].start==u"Outer"&&doc->edges[1].end==u"Sink"));
        expect(bool(doc->edges[2].start==u"Outer"&&doc->edges[2].end==u"Outer"));
        expect(bool(doc->edges[3].start==u"Visible"&&doc->edges[3].end==u"Visible"));
        const auto nested=prepare_flowchart(u"flowchart TB\nsubgraph Parent\nsubgraph Child\nA-->B\nend\nX-->A\nend\nChild@{view: collapsed}",*session);
        expect(fatal(nested.has_value()));
        const auto child=std::ranges::find(nested->nodes,u"Child",&FlowchartNode::id);
        expect(fatal(child!=nested->nodes.end()));expect(bool(child->parent==u"Parent"));
        expect(nested->nodes.size()==3_u);expect(nested->edges.size()==1_u);
    };
    "mermaid_flowchart_collapsed_group_paints_its_measured_title_and_indicator"_test=[] {
        FlowchartRenderInput input;
        input.document.nodes.push_back({.id=u"Group",.label=u"Group",.shape=u"collapsedGroup",.padding=8});
        input.theme.minimum_label_width=500;
        FlowchartMeasurements measured;
        measured.nodes.push_back({.width=70,.height=24,.baseline=18,.line_step=24,.lines={u"Group"}});
        const auto svg=render_flowchart_svg(input,measured);
        expect(fatal(svg.has_value())) << (svg?"":svg.error());
        expect(svg->width<200);
        expect(svg->svg.find("collapsed-separator")!=svg->svg.npos);
        std::size_t dots=0;
        for(std::size_t pos=0;(pos=svg->svg.find("class=\"collapsed-indicator\"",pos))!=svg->svg.npos;++pos) ++dots;
        expect(dots==3_u);
        expect(svg->svg.find("opacity=\"0.6\"")!=svg->svg.npos);
        expect(svg->svg.find("stroke-width=\"0.75\"")!=svg->svg.npos);
        input.theme.hand_drawn=true;input.theme.hand_drawn_seed=42;
        const auto rough=render_flowchart_svg(input,measured);
        expect(fatal(rough.has_value())) << (rough?"":rough.error());
        expect(rough->width>80&&rough->height>60);
        expect(rough->svg.find("<path d=")!=rough->svg.npos);
        expect(rough->svg.find("<rect class=\"label-container collapsed-group\"")==rough->svg.npos);
        expect(rough->svg.find("Group")!=rough->svg.npos);
    };
};
}
