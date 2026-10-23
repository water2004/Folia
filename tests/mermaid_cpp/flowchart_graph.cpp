#include "support/folia_test.hpp"
import folia.mermaid.flowchart_graph;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"

void compare_graph(const FlowchartPreparedGraph& actual,const ConfigValue& expected,
    const FlowchartInput& source,std::string_view name) {
    expect(bool(actual.direction==field(expected,u"direction").string))<<name;
    expect(actual.node_spacing==field(expected,u"nodeSpacing").number)<<name;
    expect(actual.rank_spacing==field(expected,u"rankSpacing").number)<<name;
    expect(actual.margin_x==8.0);expect(actual.margin_y==8.0);
    const auto& nodes=field(expected,u"nodes").sequence;
    expect(fatal(actual.nodes.size()==nodes.size()))<<name;
    std::size_t extracted=0;
    for(std::size_t i=0;i<nodes.size();++i) {
        const auto& a=actual.nodes[i];const auto& e=nodes[i];
        expect(bool(a.data.id==field(e,u"id").string))<<name<<i<<" id";
        expect(bool(a.data.parent==field(e,u"parent").string))<<name<<i<<" parent";
        expect(bool(a.data.label==field(e,u"label").string))<<name<<i<<" label";
        expect(bool(a.data.shape==field(e,u"shape").string))<<name<<i<<" shape";
        if(a.source_index) {
            expect(fatal(*a.source_index<source.nodes.size()));
            expect(bool(a.data.id==source.nodes[*a.source_index].id));
            expect(bool(a.data.styles==source.nodes[*a.source_index].styles));
        } else expect(bool(a.data.shape==u"labelRect"));
        const auto& child=field(e,u"graph");
        if(child.kind==ConfigKind::null) expect(!a.subgraph.has_value())<<name;
        else {
            expect(fatal(a.subgraph.has_value()));expect(fatal(*a.subgraph<actual.subgraphs.size()));
            ++extracted;compare_graph(actual.subgraphs[*a.subgraph],child,source,name);
        }
    }
    expect(extracted==actual.subgraphs.size());
    const auto& edges=field(expected,u"edges").sequence;
    expect(fatal(actual.edges.size()==edges.size()))<<name;
    for(std::size_t i=0;i<edges.size();++i) {
        const auto& a=actual.edges[i];const auto& e=edges[i];
        for(auto [value,key]:{std::pair{a.name,u"name"},std::pair{a.data.start,u"start"},
            std::pair{a.data.end,u"end"},std::pair{a.data.id,u"id"},std::pair{a.data.label,u"label"},
            std::pair{a.from_cluster,u"fromCluster"},std::pair{a.to_cluster,u"toCluster"}})
            expect(bool(value==field(e,key).string))<<name<<i<<" edge field";
        expect(fatal(a.source_index<source.edges.size()));
        const auto& original=source.edges[a.source_index];
        expect(bool(a.data.styles==original.styles));expect(a.data.min_length==original.min_length);
        if(field(e,u"loopOrder").kind==ConfigKind::null) expect(a.loop_segment==-1);
        else {
            expect(a.loop_segment==static_cast<int>(field(e,u"loopOrder").number));
            expect(bool(original.id==field(e,u"loopId").string));
            if(a.loop_segment!=1) expect(a.data.label.empty());
        }
    }
}
suite graph_preparation=[] {
    "mermaid_flowchart_graph_matches_original_clusters_and_loops"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"flowchart-graph.tsv"};
        expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r') line.pop_back();
            if(line.empty()||line.front()=='#') continue;
            std::vector<std::string_view> fields;std::string_view rest{line};
            for(;;) {auto tab=rest.find('\t');fields.push_back(rest.substr(0,tab));if(tab==rest.npos) break;rest.remove_prefix(tab+1);}
            expect(fatal(fields.size()==4_u));++count;
            auto session=ConfigSession::create();expect(fatal(session.has_value()));
            const auto site=json(decode(fields[2]));
            expect(fatal(session->save_initialize_config(site).has_value()));
            expect(fatal(session->set_site_config(site).has_value()));
            auto source=prepare_flowchart(decode(fields[1]),*session);
            expect(fatal(source.has_value()))<<fields[0]<<(source?"":source.error());
            auto actual=prepare_flowchart_graph(*source);
            expect(fatal(actual.has_value()))<<fields[0]<<(actual?"":actual.error());
            compare_graph(actual->graph,json(decode(fields[3])),*source,fields[0]);
            expect(actual->source.nodes.size()==source->nodes.size());
            expect(actual->source.edges.size()==source->edges.size());
            // Repeated request uses no process-wide cluster database.
            auto repeated=prepare_flowchart_graph(*source);expect(fatal(repeated.has_value()));
            compare_graph(repeated->graph,json(decode(fields[3])),*source,fields[0]);
        }
        expect(count==15_u);
    };
    "mermaid_flowchart_graph_rejects_invalid_links_without_mutation"_test=[] {
        FlowchartInput source;source.nodes={{.id=u"A"},{.id=u"B"}};
        source.edges={{.id=u"edge",.start=u"A",.end=u"missing"}};
        expect(!prepare_flowchart_graph(source));
        source.edges[0].end=u"B";
        auto good=prepare_flowchart_graph(source);expect(fatal(good.has_value()));
        source.nodes[0].label=u"changed";expect(good->source.nodes[0].label.empty());
        source.nodes[0].parent=u"B";source.nodes[1].parent=u"A";
        expect(!prepare_flowchart_graph(source));
        source.nodes[0].parent.clear();source.nodes[1].parent.clear();
        expect(prepare_flowchart_graph(source).has_value());
    };
};
}
