// Measured Class default ELK adapter, Mermaid 12.2.0. MIT, ../LICENSE.
export module folia.mermaid.class_elk;
import std;
export import folia.mermaid.class_svg;
export import folia.mermaid.flowchart_elk;
export namespace folia::mermaid {
struct ClassElkTerminals { std::optional<FlowchartPoint> start,end; };
struct ClassElkScene : FlowchartScene { std::vector<ClassElkTerminals> terminals; };
// Uses the existing Class node-box measurements and graph assembly. Relations
// and measurements include note attachments in their graph edge order.
std::expected<ClassElkScene,std::string> layout_measured_elk_class(
    const FlowchartInput&,std::span<const FlowchartNodeBox>,
    std::span<const ClassRelation>,std::span<const ClassEdgeMeasurements>,
    const FlowchartMeasurements&);
// After the shared endpoint clipper, before marker shortening. Raw endpoints
// preserve the original ELK port positions for terminal labels to follow.
void finish_class_elk_scene(ClassElkScene&,const FlowchartInput&,
    std::span<const ClassEdgeMeasurements>,
    std::span<const std::pair<FlowchartPoint,FlowchartPoint>> raw_endpoints);
}
