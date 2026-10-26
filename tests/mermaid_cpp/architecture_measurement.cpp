#include "support/folia_test.hpp"
import folia.mermaid.fcose_architecture;
import folia.mermaid.architecture_measurement;
import folia.mermaid.number;
namespace {
using namespace boost::ut;using namespace folia::mermaid;
#include "theme_fixture.hpp"
suite architecture_measured_layout=[] {
    "mermaid_architecture_browser_measured_layout_matches_original"_test=[] {
        const auto file=std::getenv("FOLIA_MERMAID_ARCHITECTURE_MEASUREMENTS");if(!file)return;
        std::ifstream input(file);expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;const auto tab=line.find('\t');expect(fatal(tab!=line.npos));const auto name=line.substr(0,tab);const auto data=json(decode(std::string_view(line).substr(tab+1)));++count;
            auto session=ConfigSession::create();expect(fatal(session.has_value()));ConfigValue site;site.kind=ConfigKind::mapping;site.mapping.emplace(u"theme",field(data,u"theme"));site.mapping.emplace(u"look",ConfigValue{.kind=ConfigKind::string,.string=u"classic"});expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
            const auto document=prepare_architecture(field(data,u"source").string,*session);expect(fatal(document.has_value()));ArchitectureCyMeasurements measured;const auto& labels=field(data,u"cyLabels");
            for(const auto& [key,output]:{std::pair{u"nodes",&measured.nodes},{u"edges",&measured.edges}})for(const auto& label:field(labels,key).sequence)output->push_back({field(label,u"width").number,field(label,u"height").number});
            const auto layout=layout_architecture(*document,&measured);expect(fatal(layout.has_value()))<<name<<(layout?"":layout.error());const auto& services=field(data,u"services").sequence;const auto& groups=field(data,u"groups").sequence;
            expect(fatal(services.size()==document->nodes.size()));expect(fatal(groups.size()==document->groups.size()));
            for(std::size_t i=0;i<services.size();++i){const auto& n=layout->nodes[document->groups.size()+i];const auto& p=field(services[i],u"position");expect(std::abs(n.x-field(p,u"x").number)<.0001)<<name<<i<<n.x;expect(std::abs(n.y-field(p,u"y").number)<.0001)<<name<<i<<n.y;}
            const auto icon=document->config.find(u"iconSize")->number;
            for(std::size_t i=0;i<groups.size();++i){const auto& n=layout->nodes[i];const auto& b=field(groups[i],u"box");const double w=n.width+2*n.padding+3,h=n.height+2*n.padding+3;
                for(const auto& [key,value]:{std::pair{u"x",n.x-w/2+icon/2},{u"y",n.y-h/2+icon/2},{u"width",w},{u"height",h}})expect(std::abs(value-field(b,key).number)<.0001)<<name<<i<<value;
            }
        }
        expect(count==12_u);
    };
};
}
