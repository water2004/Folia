#include "support/folia_test.hpp"
import folia.mermaid.architecture;
import folia.mermaid.number;
namespace {
using namespace boost::ut;using namespace folia::mermaid;
#include "theme_fixture.hpp"
ConfigValue text(std::u16string value){ConfigValue v;v.kind=ConfigKind::string;v.string=std::move(value);return v;}
ConfigValue number(double value){ConfigValue v;v.kind=ConfigKind::number;v.number=value;return v;}
ConfigValue boolean(bool value){ConfigValue v;v.kind=ConfigKind::boolean;v.boolean=value;return v;}
ConfigValue array(std::vector<ConfigValue> values={}){ConfigValue v;v.kind=ConfigKind::sequence;v.sequence=std::move(values);return v;}
ConfigValue object(std::map<std::u16string,ConfigValue> values){ConfigValue v;v.kind=ConfigKind::mapping;v.mapping=std::move(values);return v;}
ConfigValue project(const ArchitectureDocument& d) {
    auto nodes=array(),groups=array(),edges=array(),hints=array(),adjacency=array(),spatial=array(),alignments=array();
    for(const auto& n:d.nodes){auto list=array();for(auto e:n.edges)list.sequence.push_back(number(e));nodes.sequence.push_back(object({{u"id",text(n.id)},{u"type",text(n.type)},{u"icon",text(n.icon)},{u"iconText",text(n.icon_text)},{u"title",text(n.title)},{u"parent",text(n.parent)},{u"edges",std::move(list)}}));}
    for(const auto& g:d.groups)groups.sequence.push_back(object({{u"id",text(g.id)},{u"icon",text(g.icon)},{u"title",text(g.title)},{u"parent",text(g.parent)}}));
    for(const auto& e:d.edges)edges.sequence.push_back(object({{u"lhsId",text(e.lhs_id)},{u"rhsId",text(e.rhs_id)},{u"lhsDir",text(std::u16string(1,e.lhs_dir))},{u"rhsDir",text(std::u16string(1,e.rhs_dir))},{u"lhsInto",boolean(e.lhs_into)},{u"rhsInto",boolean(e.rhs_into)},{u"lhsGroup",boolean(e.lhs_group)},{u"rhsGroup",boolean(e.rhs_group)},{u"title",text(e.title)}}));
    for(const auto& h:d.hints){auto members=array();for(const auto& member:h.members)members.sequence.push_back(text(member));hints.sequence.push_back(object({{u"direction",text(h.direction)},{u"members",std::move(members)}}));}
    for(const auto& [id,list]:d.adjacency){auto entries=array();for(const auto& [pair,node]:list)entries.sequence.push_back(array({text(pair),text(node)}));adjacency.sequence.push_back(array({text(id),std::move(entries)}));}
    for(const auto& map:d.spatial_maps){auto entries=array();for(const auto& n:map)entries.sequence.push_back(array({text(n.id),array({number(n.x),number(n.y)})}));spatial.sequence.push_back(std::move(entries));}
    for(const auto& [pair,direction]:d.group_alignments)alignments.sequence.push_back(array({text(pair),text(direction)}));
    return object({{u"title",text(d.title)},{u"accTitle",text(d.acc_title)},{u"accDescr",text(d.acc_description)},{u"nodes",std::move(nodes)},{u"groups",std::move(groups)},{u"edges",std::move(edges)},{u"hints",std::move(hints)},{u"adjacency",std::move(adjacency)},{u"spatialMaps",std::move(spatial)},{u"groupAlignments",std::move(alignments)},{u"config",d.config}});
}
suite architecture=[] {
    "mermaid_architecture_parser_db_and_constraint_inputs_match_original"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");std::ifstream input{std::filesystem::path(override?override:FOLIA_MERMAID_FIXTURE_DIR)/"architecture.tsv"};expect(fatal(input.is_open()));
        std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            std::vector<std::string_view> fields;std::string_view rest=line;for(;;){const auto tab=rest.find('\t');fields.push_back(rest.substr(0,tab));if(tab==rest.npos)break;rest.remove_prefix(tab+1);}
            expect(fatal(fields.size()==4_u));++count;auto session=ConfigSession::create();expect(fatal(session.has_value()));const auto site=json(decode(fields[2]));
            expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
            const auto actual=prepare_architecture(decode(fields[1]),*session);const auto expected=json(decode(fields[3]));
            if(expected.kind==ConfigKind::null){expect(!actual)<<fields[0];continue;}
            expect(fatal(actual.has_value()))<<fields[0]<<(actual?"":actual.error());const auto data=project(*actual);
            for(const auto& [key,value]:expected.mapping)expect(bool(snapshot(field(data,key))==snapshot(value)))<<fields[0]<<hex(key);
        }
        expect(count==8_u);
    };
};
}
