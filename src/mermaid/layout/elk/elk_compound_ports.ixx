// SPDX-License-Identifier: EPL-2.0
export module folia.mermaid.elk_compound_ports;
import std;
export import folia.mermaid.elk_layout;

export namespace folia::mermaid {
enum class ElkBoundaryConstraint { first_separate, last_separate, top, bottom };
struct ElkCompoundPortDummy {
    ElkPortSide side=ElkPortSide::undefined,inner_port_side=ElkPortSide::undefined;
    ElkBoundaryConstraint constraint=ElkBoundaryConstraint::first_separate;
    double border_offset=0;
    ElkRoutingPoint position;
};
struct ElkCompoundContent {
    double width=0,height=0;
    double padding_left=0,padding_right=0,padding_top=0,padding_bottom=0;
    ElkRoutingPoint offset;
};
// Original createExternalPortDummy for implicit zero-size group ports with FREE
// constraints. outgoing describes flow out of the child graph. Parent-side
// boundary port and its internal dummy port face opposite directions.
[[nodiscard]] ElkCompoundPortDummy elk_make_compound_port_dummy(
    ElkLayoutDirection direction,bool outgoing,double border_offset);
// HierarchicalNodeResizingProcessor's minimum-size expansion. The adapter uses
// H_CENTER/V_TOP for containers; ordinary ELK H_LEFT is available via false.
void elk_resize_compound_content(ElkCompoundContent& content,
    std::span<ElkCompoundPortDummy> dummies,double minimum_width,double minimum_height,
    bool center_horizontal=true);
// Original getExternalPortPosition exports parent-local coordinates and moves
// the internal dummy onto the final border, including padding and graph offset.
[[nodiscard]] ElkRoutingPoint elk_export_compound_port(
    const ElkCompoundContent& content,ElkCompoundPortDummy& dummy);
}
