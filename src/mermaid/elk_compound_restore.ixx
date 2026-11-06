// SPDX-License-Identifier: EPL-2.0
export module folia.mermaid.elk_compound_restore;
import std;
export import folia.mermaid.elk_compound_graph;

export namespace folia::mermaid {
struct ElkCompoundSegmentGeometry {
    std::vector<ElkRoutingPoint> bends;
    std::vector<ElkLabelPosition> labels;
};
struct ElkCompoundGeometry {
    std::vector<ElkCompoundContent> graphs;
    std::vector<ElkRoutingPoint> node_positions,port_positions;
    std::vector<ElkCompoundSegmentGeometry> segments;
};
struct ElkCompoundRestoredEdge {
    std::size_t original_edge=0,reference_graph=0,source_port=0,target_port=0;
    ElkRoutingPoint start,end,target_offset;
    std::vector<ElkRoutingPoint> bends;
    std::vector<ElkLabelPosition> labels;
};
// Original changeCoordSystem and CompoundGraphPostprocessor. Geometry is in
// each graph's post-direction local coordinates; padding and offsets are not
// yet folded into child node positions. Output uses the original source graph
// (or its nested graph when the edge starts at an ancestor container).
[[nodiscard]] std::vector<ElkCompoundRestoredEdge> elk_restore_compound_edges(
    const ElkCompoundGraph& graph,const ElkCompoundGeometry& geometry,
    bool unnecessary_bendpoints=true);
}
