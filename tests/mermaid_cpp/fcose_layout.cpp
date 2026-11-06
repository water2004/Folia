#include "support/folia_test.hpp"
import folia.mermaid.fcose_layout;
import folia.mermaid.fcose_architecture;
import folia.mermaid.number;
namespace {
using namespace boost::ut;using namespace folia::mermaid;
#include "theme_fixture.hpp"
suite fcose_layout_tests=[] {
    "mermaid_architecture_two_pass_fcose_matches_original"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");std::ifstream input{std::filesystem::path(override?override:FOLIA_MERMAID_FIXTURE_DIR)/"architecture-layout.tsv"};expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            std::vector<std::string_view> fields;std::string_view rest=line;for(;;){auto tab=rest.find('\t');fields.push_back(rest.substr(0,tab));if(tab==rest.npos)break;rest.remove_prefix(tab+1);}
            expect(fatal(fields.size()==4_u));++count;auto session=ConfigSession::create();expect(fatal(session.has_value()));const auto site=json(decode(fields[2]));expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
            const auto document=prepare_architecture(decode(fields[1]),*session);expect(fatal(document.has_value()));const auto actual=layout_architecture(*document);expect(fatal(actual.has_value()))<<fields[0]<<(actual?"":actual.error());const auto expected=json(decode(fields[3]));const auto& nodes=field(expected,u"nodes").sequence;
            expect(fatal(actual->nodes.size()==nodes.size()));
            for(std::size_t i=0;i<nodes.size();++i){const auto& n=actual->nodes[i];expect(bool(n.id==field(nodes[i],u"id").string));for(const auto& [key,value]:{std::pair{u"x",n.x},{u"y",n.y},{u"width",n.width},{u"height",n.height}})expect(std::abs(value-field(nodes[i],key).number)<1e-6)<<fields[0]<<i<<value<<field(nodes[i],key).number;}
        }
        expect(count==6_u);
    };
    "mermaid_fcose_incremental_compound_positions_match_original"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");std::ifstream input{std::filesystem::path(override?override:FOLIA_MERMAID_FIXTURE_DIR)/"fcose-layout.tsv"};expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            const auto tab=line.find('\t');expect(fatal(tab!=line.npos));const auto name=line.substr(0,tab);const auto data=json(decode(std::string_view(line).substr(tab+1)));++count;expect(fatal(data.sequence.size()==2_u));
            for(const auto& pass:data.sequence) {
                FCoseLayoutInput request;const auto& original=field(pass,u"input");
                for(const auto& node:field(original,u"nodes").sequence)request.nodes.push_back({field(node,u"id").string,field(node,u"parent").string,field(node,u"x").number,field(node,u"y").number,field(node,u"width").number,field(node,u"height").number,field(node,u"padding").number});
                for(const auto& edge:field(original,u"edges").sequence)request.edges.push_back({field(edge,u"source").string,field(edge,u"target").string,field(edge,u"ideal").number,field(edge,u"elasticity").number});
                const auto& constraints=field(original,u"constraints");const auto& alignment=field(constraints,u"alignmentConstraint");
                for(const auto& [key,groups]:{std::pair{u"horizontal",&request.constraints.horizontal},{u"vertical",&request.constraints.vertical}})if(const auto* array=alignment.find(key))for(const auto& g:array->sequence){std::vector<std::u16string> ids;for(const auto& id:g.sequence)ids.push_back(id.string);groups->push_back(std::move(ids));}
                if(const auto* relative=constraints.find(u"relativePlacementConstraint"))for(const auto& edge:relative->sequence) {
                    ArchitectureRelativePlacement r;r.gap=field(edge,u"gap").number;
                    for(const auto& [key,value]:{std::pair{u"left",&r.left},{u"right",&r.right},{u"top",&r.top},{u"bottom",&r.bottom}})if(const auto* item=edge.find(key))*value=item->string;
                    request.constraints.relative.push_back(std::move(r));
                }
                const auto actual=layout_fcose_incremental(request);expect(fatal(actual.has_value()))<<name<<(actual?"":actual.error());const auto& expected=field(pass,u"output");
                expect(actual->iterations==field(expected,u"iterations").number)<<name;
                const auto& nodes=field(expected,u"nodes").sequence;expect(fatal(actual->nodes.size()==nodes.size()));
                for(std::size_t i=0;i<nodes.size();++i){const auto& n=actual->nodes[i];for(const auto& [key,value]:{std::pair{u"x",n.x},{u"y",n.y},{u"width",n.width},{u"height",n.height}})expect(std::abs(value-field(nodes[i],key).number)<1e-6)<<name<<i<<value<<field(nodes[i],key).number;}
            }
        }
        expect(count==6_u);
    };
};
}
