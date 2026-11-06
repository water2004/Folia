#include "support/folia_test.hpp"
import folia.mermaid.sankey_svg;
import folia.mermaid.number;
namespace {
using namespace boost::ut;using namespace folia::mermaid;
#include "theme_fixture.hpp"
void near(double actual,double expected){expect(std::abs(actual-expected)<1e-8)<<actual<<expected;}
suite sankey=[] {
    "mermaid_sankey_matches_csv_db_and_original_d3_layout"_test=[] {
        const auto override_path=std::getenv("FOLIA_MERMAID_FIXTURES");std::ifstream input{std::filesystem::path{override_path?override_path:FOLIA_MERMAID_FIXTURE_DIR}/"sankey.tsv"};expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;std::vector<std::string_view> fields;std::string_view rest{line};for(;;){auto tab=rest.find('\t');fields.push_back(rest.substr(0,tab));if(tab==rest.npos)break;rest.remove_prefix(tab+1);}expect(fatal(fields.size()==4_u));++count;auto session=ConfigSession::create();expect(fatal(session.has_value()));auto site=json(decode(fields[2]));expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));auto e=json(decode(fields[3]));auto prepared=prepare_diagram(decode(fields[1]),*session);expect(fatal(prepared.has_value()));auto parsed=parse_sankey(prepared->code.cleaned);
            if(e.kind==ConfigKind::null){expect(!parsed)<<fields[0];continue;}expect(fatal(parsed.has_value()))<<fields[0];auto& expected_graph=field(e,u"graph");auto& expected_nodes=field(expected_graph,u"nodes").sequence;auto& expected_links=field(expected_graph,u"links").sequence;expect(fatal(parsed->nodes.size()==expected_nodes.size()));expect(fatal(parsed->links.size()==expected_links.size()));for(std::size_t i=0;i<parsed->nodes.size();++i)expect(bool(parsed->nodes[i].id==field(expected_nodes[i],u"id").string));for(std::size_t i=0;i<parsed->links.size();++i){auto& l=parsed->links[i];expect(bool(parsed->nodes[l.source].id==field(expected_links[i],u"source").string));expect(bool(parsed->nodes[l.target].id==field(expected_links[i],u"target").string));near(l.value,field(expected_links[i],u"value").number);}
            auto actual=prepare_sankey(decode(fields[1]),*session);auto& geometry=field(e,u"geometry");if(geometry.kind==ConfigKind::null){expect(!actual);continue;}expect(fatal(actual.has_value()))<<fields[0]<<(actual?"":actual.error());auto& nodes=field(geometry,u"nodes").sequence;auto& links=field(geometry,u"links").sequence;const auto& g=actual->graph;for(std::size_t i=0;i<nodes.size();++i){auto& n=g.nodes[i];for(auto [key,value]:{std::pair{u"value",n.value},{u"depth",double(n.depth)},{u"height",double(n.height)},{u"layer",double(n.layer)},{u"x0",n.x0},{u"x1",n.x1},{u"y0",n.y0},{u"y1",n.y1}})near(value,field(nodes[i],key).number);}for(std::size_t i=0;i<links.size();++i){auto& l=g.links[i];for(auto [key,value]:{std::pair{u"source",double(l.source)},{u"target",double(l.target)},{u"value",l.value},{u"width",l.width},{u"y0",l.y0},{u"y1",l.y1}})near(value,field(links[i],key).number);}expect(!render_sankey_svg(*actual,{}));
        }expect(count==12_u);
    };
};
}
