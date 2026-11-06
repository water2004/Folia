#include "support/folia_test.hpp"
import folia.mermaid.flowchart_label;
import folia.mermaid.render_entry;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"
suite flowchart_labels=[] {
    "mermaid_flowchart_markdown_runs_match_original"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"flowchart-label.tsv"};
        expect(fatal(input.is_open()));std::string row;std::size_t count=0;
        while(std::getline(input,row)) {
            if(!row.empty()&&row.back()=='\r') row.pop_back();
            if(row.empty()||row.front()=='#') continue;
            const auto first=row.find('\t'),last=row.rfind('\t');
            const auto source=decode(std::string_view(row).substr(first+1,last-first-1));
            const auto expected=decode(std::string_view(row).substr(last+1));
            auto lines=flowchart_label_runs(source,u"markdown");
            expect(lines.has_value())<<row.substr(0,first)<<(lines?"":lines.error());
            if(!lines) continue;
            std::u16string observed;
            for(std::size_t i=0;i<lines->size();++i) {
                if(i) observed+=u'\n';
                for(const auto& run:(*lines)[i]) for(auto c:run.text) {
                    if(c==u' '||c==u'\t'||c==u'\r'||c==u'\n') continue;
                    observed+=static_cast<char16_t>(u'0'+run.strong+2*run.emphasis);observed+=c;
                }
            }
            expect(bool(observed==expected))<<row.substr(0,first)<<hex(observed)<<hex(expected);++count;
        }
        expect(count==8_u);
    };
    "mermaid_flowchart_display_lines_preserve_source_labels"_test=[] {
        auto session=ConfigSession::create();expect(fatal(session.has_value()));
        const ConfigValue site{.kind=ConfigKind::mapping,.mapping={
            {u"theme",ConfigValue{.kind=ConfigKind::string,.string=u"default"}},
            {u"look",ConfigValue{.kind=ConfigKind::string,.string=u"classic"}},
            {u"layout",ConfigValue{.kind=ConfigKind::string,.string=u"dagre"}},
        }};
        expect(fatal(session->save_initialize_config(site).has_value()));
        expect(fatal(session->set_site_config(site).has_value()));
        const std::u16string source=u"---\nconfig: {theme: default, look: classic, layout: dagre}\n---\nflowchart TD\nA[\"Say #quot;hello#quot;<br/>x &lt; 5\"] -->|\"next\\nstep\"| B[End]";
        auto native=prepare_native_diagram(source,*session);
        expect(native.has_value())<<(native?"":native.error());
        expect(fatal(native.has_value()));
        const auto& graph=std::get<FlowchartRenderInput>(native->content).document;
        const auto label=graph.nodes[0].label;
        expect(bool(label==u"Say #quot;hello#quot;<br/>x &lt; 5"));
        auto lines=flowchart_label_lines(label,graph.nodes[0].label_type);
        expect(fatal(lines.has_value()));expect(fatal(lines->size()==2_u));
        expect(bool((*lines)[0]==u"Say \"hello\""));expect(bool((*lines)[1]==u"x < 5"));
        expect(bool(graph.nodes[0].label==label));
        auto edge=flowchart_label_lines(graph.edges[0].label,graph.edges[0].label_type);
        expect(fatal(edge.has_value()));expect(fatal(edge->size()==2_u));
        expect(bool((*edge)[0]==u"next"));expect(bool((*edge)[1]==u"step"));
        auto styled=flowchart_label_runs(u"normal <b>bold <i>both</i></b><br/>x &lt; y",u"string");
        expect(fatal(styled.has_value()));expect(fatal(styled->size()==2_u));
        expect(fatal((*styled)[0].size()==3_u));
        expect(bool((*styled)[0][0].text==u"normal "));
        expect((*styled)[0][1].strong && !(*styled)[0][1].emphasis);
        expect((*styled)[0][2].strong && (*styled)[0][2].emphasis);
        expect(bool((*styled)[1][0].text==u"x < y"));
        expect(!flowchart_label_runs(u"<img src='image'>",u"string"));
    };
};
}
