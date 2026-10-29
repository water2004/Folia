// Folia's native source-to-render boundary. Mermaid 12.2.0, MIT.
export module folia.mermaid.render_entry;
import std;
export import folia.mermaid.frontend;
export import folia.mermaid.packet_svg;
export import folia.mermaid.pie_svg;
export import folia.mermaid.flowchart_svg;
export import folia.mermaid.sequence_svg;
export import folia.mermaid.class_svg;
export import folia.mermaid.quadrant_svg;
export import folia.mermaid.radar_svg;
export import folia.mermaid.xy_svg;
export import folia.mermaid.sankey_svg;
export import folia.mermaid.treemap_svg;
export import folia.mermaid.journey_svg;
export import folia.mermaid.timeline_svg;
export import folia.mermaid.gantt_svg;
export import folia.mermaid.git_graph_svg;
export import folia.mermaid.block_svg;
export import folia.mermaid.kanban_svg;
export import folia.mermaid.c4_svg;
export import folia.mermaid.state_svg;
export import folia.mermaid.mindmap_svg;
export import folia.mermaid.architecture_svg;

export namespace folia::mermaid {
struct PacketRenderInput { PacketLayout layout; PacketSvgTheme theme; };
struct PieRenderInput { PieDocument document; PieSvgTheme theme; PieSvgConfig config; };
struct NativeDiagram {
    PreparedDiagram prepared;
    std::variant<PacketRenderInput, PieRenderInput, FlowchartRenderInput, SequenceRenderInput, ClassRenderInput, QuadrantRenderInput, RadarRenderInput, XYRenderInput, SankeyRenderInput, TreemapRenderInput, JourneyRenderInput, TimelineRenderInput, GanttRenderInput, GitGraphRenderInput, BlockRenderInput, KanbanRenderInput, C4RenderInput, StateRenderInput, MindmapRenderInput, ArchitectureRenderInput> content;
};
struct DiagramSvg { std::string svg; double width{}, height{}; };

// Resolves supported family configuration and parses the prepared source.
// Family font lists remain unresolved until the platform selects an installed
// font. No browser HTML/DOM sanitizer is emulated: metadata must be plain text.
std::expected<NativeDiagram, std::string> prepare_native_diagram(
    std::u16string_view source, const ConfigSession&, FrontendLimits limits = {});

// Decode parser-protected decimal/common named entity escapes for measurement.
// Parsing/deduplication retains the original spelling, as the official DB does.
std::expected<std::u16string, std::string> diagram_display_text(std::u16string_view);

// Measurement uses the selected Pie font and decoded final legend/title text.
// Flowchart consumes node/edge label metrics and requires Dagre/classic config.
// Packet does not need measured widths. Output includes plain accessibility
// metadata; no DOM id or event bindings are required by Folia's SVG consumer.
std::expected<DiagramSvg, std::string> render_native_diagram(
    const NativeDiagram&, const PieMeasurements& measurements = {},
    const FlowchartMeasurements& flowchart_measurements = {},
    const SequenceMeasurements& sequence_measurements = {},
    const ClassMeasurements& class_measurements = {},
    const XYMeasurements& xy_measurements = {},
    const SankeyMeasurements& sankey_measurements = {},
    const TreemapMeasure& treemap_measure = {},
    const JourneyMeasure& journey_measure = {},
    const TimelineMeasure& timeline_measure = {},
    const GanttMeasure& gantt_measure = {},
    const GitGraphMeasure& git_graph_measure = {},
    const MindmapMeasurements& mindmap_measurements = {},
    const BlockMeasure& block_measure = {},
    const KanbanMeasure& kanban_measure = {},
    const C4Measurements& c4_measurements = {},
    const ArchitectureMeasurements& architecture_measurements = {},
    const StateMeasure& state_measure = {});
}
