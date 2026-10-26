// SPDX-License-Identifier: EPL-2.0
module folia.mermaid.elk_end_labels;
import std;

namespace folia::mermaid {
namespace {
// Original RectangleStripOverlapRemover transposes east/west rectangles, then
// GreedyRectangleStripOverlapRemover packs their x extents out from the node.
void remove_overlaps(std::vector<ElkEndLabelCell>& cells,std::span<const std::size_t> selected,
    bool west,double start,double gap) {
    std::vector<std::size_t> order(selected.begin(),selected.end());
    std::stable_sort(order.begin(),order.end(),[&](auto a,auto b){return cells[a].y<cells[b].y;});
    std::vector<std::vector<std::size_t>> overlaps(cells.size());
    std::vector<std::size_t> active;
    for(const auto n:order) {
        const auto& r=cells[n];
        std::erase_if(active,[&](auto p){return cells[p].y+cells[p].height<r.y;});
        // Original TreeSet compares right borders only (equal keys coincide).
        for(const auto p:active){overlaps[p].push_back(n);overlaps[n].push_back(p);}
        const auto right=r.y+r.height;
        if(std::ranges::none_of(active,[&](auto p){return cells[p].y+cells[p].height==right;}))active.push_back(n);
        std::stable_sort(active.begin(),active.end(),[&](auto a,auto b){return cells[a].y+cells[a].height<cells[b].y+cells[b].height;});
    }
    std::vector<double> offset(cells.size());std::vector<bool> placed(cells.size());
    for(const auto n:order) {
        auto& adjacent=overlaps[n];
        std::stable_sort(adjacent.begin(),adjacent.end(),[&](auto a,auto b){return offset[a]<offset[b];});
        double pos=0;
        for(const auto p:adjacent)if(placed[p]&&pos<offset[p]+cells[p].width+gap&&pos+cells[n].width+gap>offset[p])
            pos=offset[p]+cells[p].width+gap;
        offset[n]=pos;placed[n]=true;
        cells[n].x=west?start-cells[n].width-pos:start+pos;
    }
}
}
std::expected<std::vector<ElkEndLabelCell>,std::string> elk_prepare_end_labels(
    ElkLayeredGraph& g,std::span<const ElkEndLabel> labels,std::span<ElkHorizontalMargin> margins,
    bool vertical,double edge_spacing,double label_spacing) {
    if(margins.size()!=g.nodes.size())return std::unexpected("ELK end labels require measured node margins");
    std::vector<std::vector<ElkEndLabel>> by_port(g.ports.size());
    for(const auto& label:labels) {
        std::optional<std::size_t> endpoint;
        for(const auto& edge:g.edges)if(edge.original_edge==label.original_edge) {
            const auto p=label.head!=edge.reversed?edge.target_port:edge.source_port;
            if(g.nodes[g.ports[p].node].kind==ElkLayerNodeKind::normal){endpoint=p;break;}
        }
        if(!endpoint)return std::unexpected("ELK end label has no original endpoint");
        by_port[*endpoint].push_back(label);
    }
    std::vector<ElkEndLabelCell> cells;
    for(std::size_t n=0;n<g.nodes.size();++n) {
        auto& node=g.nodes[n];if(node.kind!=ElkLayerNodeKind::normal)continue;
        const auto begin=cells.size();
        std::vector<bool> above(g.ports.size());
        // SMART_DOWN counts incident ports, including ones with no end labels.
        for(const auto side:{ElkPortSide::east,ElkPortSide::west}) {
            std::vector<std::size_t> group;
            for(const auto p:node.ports)if(g.ports[p].side==side&&(!g.ports[p].incoming.empty()||!g.ports[p].outgoing.empty()))group.push_back(p);
            if(group.size()==2)above[group[side==ElkPortSide::east?0:1]]=true;
        }
        for(const auto p:node.ports)if(!by_port[p].empty()) {
            const auto& port=g.ports[p];
            if(port.side!=ElkPortSide::east&&port.side!=ElkPortSide::west)
                return std::unexpected("ELK end label preparation requires east/west ports");
            ElkEndLabelCell cell{.node=n,.port=p,.above=above[p],.west=port.side==ElkPortSide::west,.labels=std::move(by_port[p])};
            for(const auto& label:cell.labels) {
                if(vertical){cell.width+=label.width+label_spacing;cell.height=std::max(cell.height,label.height);}
                else {cell.width=std::max(cell.width,label.width);cell.height+=label.height+label_spacing;}
            }
            if(vertical)cell.width-=label_spacing;else cell.height-=label_spacing;
            double thickness=-1;
            for(const auto* edges:{&port.incoming,&port.outgoing})for(const auto e:*edges)thickness=std::max(thickness,g.edges[e].thickness);
            cell.x=cell.west?-margins[n].left-edge_spacing-cell.width:node.width+margins[n].right+edge_spacing;
            cell.y=cell.above?port.y-thickness-edge_spacing-cell.height:port.y+thickness+edge_spacing;
            cells.push_back(std::move(cell));
        }
        if(vertical)for(const bool west:{false,true}) {
            std::vector<std::size_t> selected;
            for(std::size_t i=begin;i<cells.size();++i)if(cells[i].west==west)selected.push_back(i);
            remove_overlaps(cells,selected,west,west?-margins[n].left-edge_spacing:node.width+margins[n].right+edge_spacing,2*label_spacing);
        }
        for(std::size_t i=begin;i<cells.size();++i) {
            const auto& c=cells[i];margins[n].left=std::max(margins[n].left,-c.x);
            margins[n].right=std::max(margins[n].right,c.x+c.width-node.width);
            node.margin_top=std::max(node.margin_top,-c.y);node.margin_bottom=std::max(node.margin_bottom,c.y+c.height-node.height);
        }
    }
    return cells;
}
std::vector<ElkLabelPosition> elk_restore_end_labels(std::span<const ElkEndLabelCell> cells,
    std::span<const double> x,std::span<const double> y,bool vertical,double spacing) {
    std::vector<ElkLabelPosition> result;
    for(const auto& c:cells) {
        auto px=c.x+x[c.node],py=c.y+y[c.node];
        for(const auto& l:c.labels) {
            const auto lx=vertical?px:px+(c.west?c.width-l.width:0);
            const auto ly=vertical?py+(c.above?c.height-l.height:0):py;
            result.push_back({l.id,l.original_edge,lx,ly,l.width,l.height});
            if(vertical)px+=l.width+spacing;else py+=l.height+spacing;
        }
    }
    std::ranges::sort(result,{},&ElkLabelPosition::id);return result;
}
}
