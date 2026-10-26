#include "support/folia_test.hpp"
import folia.mermaid.mindmap_svg;
import folia.mermaid.number;
namespace {
using namespace boost::ut;using namespace folia::mermaid;
#include "theme_fixture.hpp"
suite mindmap_measured_svg=[] {
    "mermaid_mindmap_original_measured_boxes_to_layout_and_svg"_test=[] {
        const auto file=std::getenv("FOLIA_MERMAID_MINDMAP_MEASUREMENTS");if(!file)return;
        std::ifstream input(file);expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;
            const auto tab=line.find('\t');expect(fatal(tab!=line.npos));const auto name=line.substr(0,tab);const auto data=json(decode(std::string_view(line).substr(tab+1)));++count;
            auto session=ConfigSession::create();expect(fatal(session.has_value()));ConfigValue site;site.kind=ConfigKind::mapping;
            site.mapping.emplace(u"theme",field(data,u"theme"));site.mapping.emplace(u"look",ConfigValue{.kind=ConfigKind::string,.string=u"classic"});
            ConfigValue variables;variables.kind=ConfigKind::mapping;variables.mapping.emplace(u"fontFamily",ConfigValue{.kind=ConfigKind::string,.string=u"Arial"});site.mapping.emplace(u"themeVariables",std::move(variables));
            expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
            const auto prepared=prepare_mindmap_render(field(data,u"source").string,*session);expect(fatal(prepared.has_value()));
            MindmapMeasurements measured;std::vector<CoseTreeNode> nodes;std::vector<CoseTreeEdge> edges;
            const auto& observed=field(data,u"nodes").sequence;
            for(std::size_t i=0;i<observed.size();++i) {
                const auto& n=prepared->document.nodes[i];const auto& label=field(observed[i],u"labelBox");const auto& b=field(observed[i],u"nodeBox");
                FlowchartTextBox box;box.width=field(label,u"width").number;box.height=field(label,u"height").number;box.baseline=16;box.line_step=24;
                const auto runs=flowchart_label_runs(n.label,n.label_type);expect(fatal(runs.has_value()));box.runs=*runs;
                for(const auto& line:*runs){std::u16string text;for(const auto& run:line)text+=run.text;box.lines.push_back(std::move(text));}
                measured.nodes.push_back(std::move(box));measured.node_bounds.push_back({field(b,u"x").number,field(b,u"y").number,field(b,u"width").number,field(b,u"height").number});
                nodes.push_back({n.id,field(b,u"width").number,field(b,u"height").number});
            }
            for(const auto& e:prepared->document.edges)edges.push_back({e.start,e.end});const auto layout=layout_cose_tree(nodes,edges);expect(fatal(layout.has_value()));
            expect(layout->iterations==field(data,u"iterations").number)<<name;
            const auto& positions=field(data,u"layoutPositions").sequence;
            for(std::size_t i=0;i<observed.size();++i){const auto& position=positions[i];expect(std::abs(layout->nodes[i].x-field(position,u"x").number)<1e-6)<<name;expect(std::abs(layout->nodes[i].y-field(position,u"y").number)<1e-6)<<name;}
            const auto svg=render_mindmap_svg(*prepared,measured);expect(fatal(svg.has_value()));
            if(const auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")){std::filesystem::create_directories(directory);std::ofstream output(std::filesystem::path(directory)/("mindmap-measured-"+name+".svg"),std::ios::binary);output<<svg->svg;expect(output.good());}
        }
        expect(count==4_u);
    };
};
}
