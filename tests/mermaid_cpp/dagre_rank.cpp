#include "support/folia_test.hpp"
import folia.mermaid.dagre_rank;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"
void same_rank(std::optional<int> actual,const ConfigValue& expected) {
    if(expected.kind==ConfigKind::null) expect(!actual.has_value());
    else {expect(fatal(actual.has_value()));expect(static_cast<double>(*actual)==expected.number);}
}
suite dagre_rank=[] {
    "mermaid_dagre_ranks_match_original_pipeline"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"dagre-rank.tsv"};
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
            std::vector<FlowchartEdgeLabelSize> labels;
            for(const auto& size:json(decode(fields[3])).sequence)
                labels.push_back({field(size,u"width").number,field(size,u"height").number});
            const auto expected=json(decode(fields[4]));
            const auto actual=rank_flowchart(*graph,labels);
            expect(actual.has_value())<<fields[0]<<(actual?"":actual.error());
            if(!actual) continue;
            expect(actual->rank_spacing==field(expected,u"rankSpacing").number)<<fields[0];
            const auto& nodes=field(expected,u"nodes").sequence;
            expect(fatal(nodes.size()==actual->nodes.size()))<<fields[0];
            for(std::size_t i=0;i<nodes.size();++i) {
                expect(bool(actual->nodes[i].id==field(nodes[i],u"id").string))<<fields[0];
                same_rank(actual->nodes[i].rank,field(nodes[i],u"rank"));
                same_rank(actual->nodes[i].min_rank,field(nodes[i],u"minRank"));
                same_rank(actual->nodes[i].max_rank,field(nodes[i],u"maxRank"));
            }
            const auto& edges=field(expected,u"edges").sequence;
            expect(fatal(edges.size()==actual->edges.size()))<<fields[0];
            for(std::size_t i=0;i<edges.size();++i) {
                expect(actual->edges[i].reversed==field(edges[i],u"reversed").boolean)<<fields[0];
                expect(actual->edges[i].self==field(edges[i],u"self").boolean)<<fields[0];
                same_rank(actual->edges[i].label_rank,field(edges[i],u"labelRank"));
            }
        }
        expect(count==25_u);
    };
    "mermaid_dagre_rank_validates_adapter_input_and_owns_output"_test=[] {
        FlowchartInput graph;
        graph.nodes={{.id=u"A"},{.id=u"B"}};
        graph.edges={{.id=u"edge",.start=u"A",.end=u"B"}};
        std::vector<FlowchartEdgeLabelSize> boxes={{40,16}};
        const auto good=rank_flowchart(graph,boxes);expect(fatal(good.has_value()));
        expect(bool(graph.nodes[0].id==u"A"));expect(graph.edges[0].min_length==1_u);
        expect(!rank_flowchart(graph,{}));
        boxes[0].width=-1;expect(!rank_flowchart(graph,boxes));boxes[0].width=40;
        graph.edges[0].end=u"missing";expect(!rank_flowchart(graph,boxes));graph.edges[0].end=u"B";
        graph.nodes[1].parent=u"A";expect(!rank_flowchart(graph,boxes));
        graph.nodes[0].parent=u"B";expect(!rank_flowchart(graph,boxes));
        graph.nodes[0].parent.clear();graph.nodes[1].parent.clear();
        const auto again=rank_flowchart(graph,boxes);expect(fatal(again.has_value()));
        expect(good->nodes[0].rank==again->nodes[0].rank);
        graph.nodes[0].id=u"changed";expect(bool(good->nodes[0].id==u"A"));
    };
};
}
