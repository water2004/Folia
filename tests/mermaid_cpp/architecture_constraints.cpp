#include "support/folia_test.hpp"
import folia.mermaid.architecture_constraints;
import folia.mermaid.number;
namespace {
using namespace boost::ut;using namespace folia::mermaid;
#include "theme_fixture.hpp"
suite architecture_constraints_tests=[] {
    "mermaid_architecture_alignment_and_relative_constraints_match_original"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");std::ifstream input{std::filesystem::path(override?override:FOLIA_MERMAID_FIXTURE_DIR)/"architecture-constraints.tsv"};expect(fatal(input.is_open()));
        std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            std::vector<std::string_view> fields;std::string_view rest=line;for(;;){auto tab=rest.find('\t');fields.push_back(rest.substr(0,tab));if(tab==rest.npos)break;rest.remove_prefix(tab+1);}
            expect(fatal(fields.size()==4_u));++count;auto session=ConfigSession::create();expect(fatal(session.has_value()));const auto site=json(decode(fields[2]));
            expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
            const auto doc=prepare_architecture(decode(fields[1]),*session);expect(fatal(doc.has_value()));
            const auto actual=architecture_constraints(*doc);expect(fatal(actual.has_value()));const auto expected=json(decode(fields[3]));const auto& alignment=field(expected,u"alignment");
            for(const auto& [key,groups]:{std::pair{u"horizontal",actual->horizontal},{u"vertical",actual->vertical}}) {
                const auto& want=field(alignment,key).sequence;expect(fatal(groups.size()==want.size()))<<fields[0];
                for(std::size_t i=0;i<groups.size();++i){expect(fatal(groups[i].size()==want[i].sequence.size()));for(std::size_t j=0;j<groups[i].size();++j)expect(bool(groups[i][j]==want[i].sequence[j].string))<<fields[0];}
            }
            const auto& relative=field(expected,u"relative").sequence;expect(fatal(actual->relative.size()==relative.size()))<<fields[0];
            for(std::size_t i=0;i<relative.size();++i) {
                const auto& r=actual->relative[i];expect(r.gap==field(relative[i],u"gap").number)<<fields[0];
                for(const auto& [key,value]:{std::pair{u"left",r.left},{u"right",r.right},{u"top",r.top},{u"bottom",r.bottom}}) {
                    if(const auto* e=relative[i].find(key))expect(bool(value==e->string))<<fields[0];else expect(value.empty())<<fields[0];
                }
            }
        }
        expect(count==6_u);
    };
};
}
