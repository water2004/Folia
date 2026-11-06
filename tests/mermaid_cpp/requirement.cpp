#include "support/folia_test.hpp"
import folia.mermaid.requirement;
import folia.mermaid.config_value;
import folia.mermaid.directives;
import folia.mermaid.number;
namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"
std::vector<std::string_view> columns(std::string_view rest) {
    std::vector<std::string_view> out;
    for (;;) {auto tab=rest.find('\t');out.push_back(rest.substr(0,tab));if(tab==rest.npos)return out;rest.remove_prefix(tab+1);}
}
suite requirement=[] {
    "mermaid_requirement_parser_and_db_match_official_12_2_0"_test=[] {
        const auto dir=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path(dir?dir:FOLIA_MERMAID_FIXTURE_DIR)/"requirement.tsv"};
        expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            if(line.empty()||line.front()=='#')continue;
            auto f=columns(line);expect(fatal(f.size()==4_u));++count;
            auto actual=parse_requirement(decode(f[1]));auto expected=json(decode(f[3]));
            if(expected.kind==ConfigKind::null){expect(!actual)<<f[0];continue;}
            expect(fatal(actual.has_value()))<<f[0]<<(actual?"":actual.error());
            const auto& d=*actual;
            expect(bool(d.direction==field(expected,u"direction").string))<<f[0]<<" direction";
            expect(bool(d.title==field(expected,u"title").string))<<f[0]<<" title";
            expect(bool(d.acc_title==field(expected,u"accTitle").string))<<f[0]<<" accTitle";
            expect(bool(d.acc_description==field(expected,u"accDescription").string))<<f[0]<<" accDescription";
            const auto& reqs=field(expected,u"requirements").sequence;expect(fatal(d.requirements.size()==reqs.size()));
            for(std::size_t i=0;i<reqs.size();++i){const auto& a=d.requirements[i];const auto& e=reqs[i];
                expect(bool(a.name==field(e,u"name").string))<<f[0]<<" requirement name";expect(bool(a.type==field(e,u"type").string))<<f[0]<<" requirement type";
                expect(bool(a.requirement_id==field(e,u"requirementId").string))<<f[0]<<" requirement id";expect(bool(a.text==field(e,u"text").string))<<f[0]<<" requirement text";
                expect(bool(a.risk==field(e,u"risk").string))<<f[0]<<" risk";expect(bool(a.verify_method==field(e,u"verifyMethod").string))<<f[0]<<" verify method";
                const auto& classes=field(e,u"classes").sequence;expect(fatal(a.classes.size()==classes.size()))<<f[0]<<" requirement classes";for(std::size_t j=0;j<classes.size();++j)expect(bool(a.classes[j]==classes[j].string))<<f[0]<<" requirement class";
                const auto& styles=field(e,u"cssStyles").sequence;expect(fatal(a.css_styles.size()==styles.size()))<<f[0]<<" requirement styles";for(std::size_t j=0;j<styles.size();++j)expect(bool(a.css_styles[j]==styles[j].string))<<f[0]<<" requirement style";
            }
            const auto& elements=field(expected,u"elements").sequence;expect(fatal(d.elements.size()==elements.size()));
            for(std::size_t i=0;i<elements.size();++i){const auto& a=d.elements[i];const auto& e=elements[i];
                expect(bool(a.name==field(e,u"name").string))<<f[0]<<" element name";expect(bool(a.type==field(e,u"type").string))<<f[0]<<" element type";expect(bool(a.doc_ref==field(e,u"docRef").string))<<f[0]<<" element docRef";
                const auto& classes=field(e,u"classes").sequence;expect(fatal(a.classes.size()==classes.size()))<<f[0]<<" element classes";for(std::size_t j=0;j<classes.size();++j)expect(bool(a.classes[j]==classes[j].string))<<f[0]<<" element class";
                const auto& styles=field(e,u"cssStyles").sequence;expect(fatal(a.css_styles.size()==styles.size()))<<f[0]<<" element styles";for(std::size_t j=0;j<styles.size();++j)expect(bool(a.css_styles[j]==styles[j].string))<<f[0]<<" element style";
            }
            const auto& rels=field(expected,u"relationships").sequence;expect(fatal(d.relationships.size()==rels.size()));
            for(std::size_t i=0;i<rels.size();++i){const auto& a=d.relationships[i];const auto& e=rels[i];expect(bool(a.type==field(e,u"type").string))<<f[0]<<" relation type";expect(bool(a.source==field(e,u"src").string))<<f[0]<<" relation source";expect(bool(a.target==field(e,u"dst").string))<<f[0]<<" relation target";}
            const auto& classes=field(expected,u"classes").sequence;expect(fatal(d.classes.size()==classes.size()))<<f[0]<<" defined classes";
            for(std::size_t i=0;i<classes.size();++i){const auto& a=d.classes[i];const auto& e=classes[i];expect(bool(a.id==field(e,u"id").string))<<f[0]<<" class id";
                const auto& styles=field(e,u"styles").sequence;expect(fatal(a.styles.size()==styles.size()))<<f[0]<<" class styles";for(std::size_t j=0;j<styles.size();++j)expect(bool(a.styles[j]==styles[j].string))<<f[0]<<" class style";
                const auto& text_styles=field(e,u"textStyles").sequence;expect(fatal(a.text_styles.size()==text_styles.size()))<<f[0]<<" class text styles";for(std::size_t j=0;j<text_styles.size();++j)expect(bool(a.text_styles[j]==text_styles[j].string))<<f[0]<<" class text style";
            }
        }
        expect(count==10_u);
    };
    "mermaid_requirement_rejects_unclosed_and_unknown_constructs"_test=[] {
        expect(!parse_requirement(u"requirementDiagram\nrequirement R {\nid: R1\n"));
        expect(!parse_requirement(u"requirementDiagram\ndirection sideways\n"));
        expect(!parse_requirement(u"requirementDiagram\nA - unknown -> B\n"));
    };
};
}
