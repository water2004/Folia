#include "support/folia_test.hpp"
import folia.mermaid.dagre_rank;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"
void near(double actual,double expected,std::string_view name) { expect(std::abs(actual-expected)<1e-7)<<name<<actual<<expected; }
suite dagre_layout=[] {
    "mermaid_dagre_layout_matches_original_pipeline"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"dagre-layout.tsv"};
        expect(fatal(input.is_open()));
        std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r') line.pop_back();
            if(line.empty()||line.front()=='#') continue;
            std::vector<std::string_view> fields;std::string_view rest{line};
            for(;;) {auto tab=rest.find('\t');fields.push_back(rest.substr(0,tab));if(tab==rest.npos) break;rest.remove_prefix(tab+1);}
            expect(fatal(fields.size()==5_u));++count;
            auto session=ConfigSession::create();expect(fatal(session.has_value()));
            const auto site=json(decode(fields[2]));
            expect(fatal(session->save_initialize_config(site).has_value()));
            expect(fatal(session->set_site_config(site).has_value()));
            const auto graph=prepare_flowchart(decode(fields[1]),*session);
            expect(fatal(graph.has_value()))<<fields[0];

            const auto boxes=json(decode(fields[3]));
            std::vector<FlowchartNodeBox> node_boxes;
            for(const auto& box:field(boxes,u"nodes").sequence) node_boxes.push_back({field(box,u"width").number,field(box,u"height").number});
            std::vector<FlowchartEdgeLabelSize> labels;
            for(const auto& box:field(boxes,u"edges").sequence) labels.push_back({field(box,u"width").number,field(box,u"height").number});
            const auto expected=json(decode(fields[4]));
            const auto actual=layout_flowchart(*graph,node_boxes,labels);
            expect(actual.has_value())<<fields[0]<<(actual?"":actual.error());
            if(!actual) continue;
            near(actual->width,field(expected,u"width").number,fields[0]);
            near(actual->height,field(expected,u"height").number,fields[0]);
            const auto& nodes=field(expected,u"nodes").sequence;
            expect(fatal(nodes.size()==actual->nodes.size()));
            for(std::size_t i=0;i<nodes.size();++i) {
                const auto& a=actual->nodes[i];const auto& e=nodes[i];
                expect(bool(a.id==field(e,u"id").string));
                near(a.x,field(e,u"x").number,fields[0]);near(a.y,field(e,u"y").number,fields[0]);
                near(a.width,field(e,u"width").number,fields[0]);near(a.height,field(e,u"height").number,fields[0]);
                for(auto [value,key]:{std::pair{a.rank,u"rank"},std::pair{a.order,u"order"}}) {
                    const auto& want=field(e,key);
                    if(want.kind==ConfigKind::null) expect(!value.has_value())<<fields[0];
                    else {expect(fatal(value.has_value()));near(*value,want.number,fields[0]);}
                }
            }
            const auto& edges=field(expected,u"edges").sequence;
            expect(fatal(edges.size()==actual->edges.size()));
            for(std::size_t i=0;i<edges.size();++i) {
                const auto& a=actual->edges[i];const auto& e=edges[i];
                const auto& points=field(e,u"points").sequence;
                expect(a.points.size()==points.size())<<fields[0];
                for(std::size_t j=0;j<std::min(points.size(),a.points.size());++j) {
                    near(a.points[j].x,field(points[j],u"x").number,fields[0]);
                    near(a.points[j].y,field(points[j],u"y").number,fields[0]);
                }
                if(field(e,u"x").kind==ConfigKind::null) expect(!a.label.has_value())<<fields[0];
                else {expect(fatal(a.label.has_value()));near(a.label->x,field(e,u"x").number,fields[0]);near(a.label->y,field(e,u"y").number,fields[0]);}
            }
        }
        expect(count==25_u);
    };
    "mermaid_dagre_layout_common_invalid_boxes_and_isolation"_test=[] {
        FlowchartInput graph;graph.nodes={{.id=u"A"},{.id=u"B"}};
        graph.edges={{.id=u"e",.start=u"A",.end=u"B"}};
        std::vector<FlowchartNodeBox> nodes={{60,30},{80,40}};
        std::vector<FlowchartEdgeLabelSize> labels={{0,0}};
        auto a=layout_flowchart(graph,nodes,labels);expect(fatal(a.has_value()));
        expect(!layout_flowchart(graph,{},labels));
        nodes[0].width=-1;expect(!layout_flowchart(graph,nodes,labels));nodes[0].width=60;
        auto b=layout_flowchart(graph,nodes,labels);expect(fatal(b.has_value()));
        expect(a->width==b->width);expect(a->nodes[0].x==b->nodes[0].x);
        expect(bool(graph.nodes[0].id==u"A"));expect(graph.edges[0].min_length==1_u);
    };
};
}
