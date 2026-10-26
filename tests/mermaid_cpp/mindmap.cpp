#include "support/folia_test.hpp"
import folia.mermaid.mindmap;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"
suite mindmap=[] {
    "mermaid_mindmap_parser_db_and_user_layout_match_original"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"mindmap.tsv"};
        expect(fatal(input.is_open()));
        std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            if(line.empty()||line.front()=='#')continue;
            std::vector<std::string_view> fields;std::string_view rest=line;
            for(;;){const auto tab=rest.find('\t');fields.push_back(rest.substr(0,tab));if(tab==rest.npos)break;rest.remove_prefix(tab+1);}
            expect(fatal(fields.size()==4_u));++count;
            auto session=ConfigSession::create();expect(fatal(session.has_value()));
            const auto site=json(decode(fields[2]));
            expect(fatal(session->save_initialize_config(site).has_value()));
            expect(fatal(session->set_site_config(site).has_value()));
            const auto actual=prepare_mindmap(decode(fields[1]),*session);
            const auto expected=json(decode(fields[3]));
            if(expected.kind==ConfigKind::null){expect(!actual)<<fields[0];continue;}
            expect(fatal(actual.has_value()))<<fields[0]<<(actual?"":actual.error());
            expect(bool(actual->layout==field(expected,u"layout").string));
            expect(bool(actual->color_by==field(expected,u"colorBy").string));
            expect(actual->padding==field(expected,u"padding").number);
            expect(actual->max_node_width==field(expected,u"maxNodeWidth").number);
            const auto& nodes=field(expected,u"nodes").sequence;
            expect(fatal(actual->nodes.size()==nodes.size()));
            for(std::size_t i=0;i<nodes.size();++i) {
                const auto& n=actual->nodes[i];const auto& e=nodes[i];
                for(const auto& [key,value]:{std::pair{u"id",n.id},{u"nodeId",n.node_id},{u"label",n.label},
                    {u"labelType",n.label_type},{u"shape",n.shape},{u"classes",n.classes},{u"icon",n.icon}})
                    expect(bool(value==field(e,key).string))<<fields[0];
                for(const auto& [key,value]:{std::pair{u"level",double(n.level)},{u"type",double(n.type)},
                    {u"width",n.width},{u"height",n.height},{u"padding",n.padding}})
                    expect(value==field(e,key).number)<<fields[0];
                const auto& section=field(e,u"section");
                if(section.kind==ConfigKind::null)expect(!n.section);
                else {expect(fatal(n.section.has_value()));expect(*n.section==section.number);}
            }
            const auto& edges=field(expected,u"edges").sequence;
            expect(fatal(actual->edges.size()==edges.size()));
            for(std::size_t i=0;i<edges.size();++i) {
                const auto& edge=actual->edges[i];const auto& e=edges[i];
                for(const auto& [key,value]:{std::pair{u"id",edge.id},{u"start",edge.start},{u"end",edge.end},{u"classes",edge.classes}})
                    expect(bool(value==field(e,key).string))<<fields[0];
                expect(edge.depth==field(e,u"depth").number);
                expect(fatal(edge.section.has_value()));expect(*edge.section==field(e,u"section").number);
            }
        }
        expect(count==8_u);
    };
};
}
