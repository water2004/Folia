// Measured default layered ELK adapter, Mermaid 12.2.0. MIT, ../LICENSE.
export module folia.mermaid.flowchart_elk;
import std;
export import folia.mermaid.flowchart_svg;
export import folia.mermaid.elk_layout;

export namespace folia::mermaid {
struct ElkMeasuredScene : FlowchartScene {
    std::vector<ElkLabelPosition> end_labels;
};
// Shared measured default adapter for Flowchart and Class. End label sizes
// already include the original marker clearance reserved by the consumer.
std::expected<ElkMeasuredScene,std::string> layout_measured_elk_scene(
    const FlowchartInput&,std::span<const FlowchartNodeBox>,
    std::span<const FlowchartEdgeLabelSize>,const FlowchartMeasurements&,
    std::span<const ElkEndLabel>);

// Routes retain ELK port anchors. The shared shape painter performs the
// original endpoint clipping, marker shortening and rounded path emission.
std::expected<FlowchartScene,std::string> layout_measured_elk_flowchart(
    const FlowchartInput&,std::span<const FlowchartNodeBox>,
    std::span<const FlowchartEdgeLabelSize>,const FlowchartMeasurements&,const FlowchartSvgTheme&);
}
