#include "support/folia_test.hpp"
import folia.mermaid.flowchart_label;
import folia.mermaid.render_entry;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
suite flowchart_labels=[] {
    "mermaid_flowchart_display_lines_preserve_source_labels"_test=[] {
        auto session=ConfigSession::create();expect(fatal(session.has_value()));
        const std::u16string source=u"---\nconfig: {theme: default, look: classic, layout: dagre}\n---\nflowchart TD\nA[\"Say #quot;hello#quot;<br/>x &lt; 5\"] -->|\"next\\nstep\"| B[End]";
        auto native=prepare_native_diagram(source,*session);expect(fatal(native.has_value()));
        const auto& graph=std::get<FlowchartRenderInput>(native->content).document;
        const auto& label=graph.nodes[0].label;
        expect(bool(label==u"Say #quot;hello#quot;<br/>x &lt; 5"));
        auto lines=flowchart_label_lines(label,graph.nodes[0].label_type);
        expect(fatal(lines.has_value()));expect(fatal(lines->size()==2_u));
        expect(bool((*lines)[0]==u"Say \"hello\""));expect(bool((*lines)[1]==u"x < 5"));
        expect(bool(graph.nodes[0].label==label));
        auto edge=flowchart_label_lines(graph.edges[0].label,graph.edges[0].label_type);
        expect(fatal(edge.has_value()));expect(fatal(edge->size()==2_u));
        expect(bool((*edge)[0]==u"next"));expect(bool((*edge)[1]==u"step"));
        expect(!flowchart_label_lines(u"<b>bold</b>",u"string"));
    };
};
}
