// SPDX-License-Identifier: EPL-2.0
// ELK 0.9.3 ordinary implicit port preparation; see ../ELK-LICENSE.
export module folia.mermaid.elk_ports;
import std;
export import folia.mermaid.elk_long_edges;

export namespace folia::mermaid {
struct ElkPointPortOptions {
    bool center_alignment = false;
    double surrounding_margin = 12, port_spacing = 10;
    double port_label_spacing_vertical = 1, label_label_spacing = 0;
};
// PortSideProcessor for ordinary non-compound ports. Run after reversal and
// before sorting/crossing; fixed-position long-edge dummy ports keep their side.
void elk_assign_flat_port_sides(ElkLayeredGraph& graph);
// Fixed measured nodes, no separate node/port labels or size constraints.
// Run after crossing order; preserve node.ports clockwise order. Ordinary
// implicit ports are zero-size points, so local position equals their anchor.
// The optional options span is indexed by stable node identity.
[[nodiscard]] std::expected<void,std::string> elk_place_flat_point_ports(
    ElkLayeredGraph& graph,std::span<const ElkPointPortOptions> options = {});
}
