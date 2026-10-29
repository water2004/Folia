// SPDX-License-Identifier: EPL-2.0
// Port of PortSideProcessor and the fixed-node, label-free east/west branch
// of NodeLabelAndSizeCalculator / PortPlacementCalculator in ELK 0.9.3.
module folia.mermaid.elk_ports;
import std;

namespace folia::mermaid {
void elk_assign_flat_port_sides(ElkLayeredGraph& graph) {
    for(const auto& node:graph.nodes)for(const auto p:node.ports) {
        auto& port=graph.ports[p];
        if(!node.fixed_port_positions||port.side==ElkPortSide::undefined)
            port.side=port.incoming.size()<port.outgoing.size()?ElkPortSide::east:ElkPortSide::west;
    }
}
std::expected<void,std::string> elk_place_flat_point_ports(
    ElkLayeredGraph& graph,std::span<const ElkPointPortOptions> options) {
    if(!options.empty()&&options.size()!=graph.nodes.size())
        return std::unexpected("ELK point port options must match graph nodes");
    for(const auto& node:graph.nodes) {
        if(node.kind!=ElkLayerNodeKind::normal||node.fixed_port_positions)continue;
        for(const auto p:node.ports)
            if(graph.ports[p].side!=ElkPortSide::east&&graph.ports[p].side!=ElkPortSide::west)
                return std::unexpected("ELK point port placement requires east/west side preprocessing");
    }
    for(std::size_t n=0;n<graph.nodes.size();++n) {
        const auto& node=graph.nodes[n];
        if(node.kind!=ElkLayerNodeKind::normal||node.fixed_port_positions)continue;
        const auto config=options.empty()?ElkPointPortOptions{}:options[n];
        // Empty north/south label cells still carry port-label border spacing.
        // updateVerticalInsidePortLabelCellPadding subtracts that and the gap.
        const auto padding=std::max(0.0,config.surrounding_margin-
            std::max(0.0,config.port_label_spacing_vertical)-2*config.label_label_spacing);
        const auto available=node.height-2*padding;
        for(const auto side:{ElkPortSide::east,ElkPortSide::west}) {
            std::vector<std::size_t> ports;
            for(const auto p:node.ports)if(graph.ports[p].side==side)ports.push_back(p);
            if(ports.empty())continue;
            // NodeContext reverses the clockwise west side into top-down order.
            if(side==ElkPortSide::west)std::ranges::reverse(ports);
            bool distributed=!config.center_alignment;
            auto calculated=(ports.size()-1)*config.port_spacing;
            if(distributed)calculated+=2*config.port_spacing;
            double current=padding,space=config.port_spacing;
            if(distributed&&ports.size()==1) {calculated-=2*config.port_spacing;distributed=false;}
            if(available<calculated) {
                if(distributed) {space+=(available-calculated)/(ports.size()+1);current+=space;}
                else space+=(available-calculated)/(ports.size()-1);
            } else if(distributed) {
                space+=std::max(0.0,(available-calculated)/(ports.size()+1));current+=space;
            } else current+=(available-calculated)/2;
            for(const auto p:ports) {
                graph.ports[p].x=side==ElkPortSide::east?node.width:0;
                graph.ports[p].y=current;current+=space;
            }
        }
    }
    return {};
}
}
