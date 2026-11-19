module folia.mermaid.flowchart_elk;
import std;
import folia.mermaid.elk_compound_layout;
import folia.mermaid.elk_components;

namespace folia::mermaid {
namespace {
ElkFlatLayoutOptions root_options(std::u16string_view direction) {
    ElkFlatLayoutOptions o;o.consider_model_order=true;
    o.direction=direction==u"LR"?ElkLayoutDirection::right:direction==u"RL"?ElkLayoutDirection::left:
        direction==u"BT"?ElkLayoutDirection::up:ElkLayoutDirection::down;
    o.placement={40,20,20,20,2,10};o.routing={40,20,20};o.edge_label_spacing=4;
    o.ports.port_spacing=20;o.ports.port_label_spacing_vertical=2;return o;
}
ElkFlatLayoutOptions group_options() {
    ElkFlatLayoutOptions o;o.placement={50,12,20,12,1.2,6};o.routing={24,30,12};o.padding=24;
    // Root-only unnecessaryBendpoints is not inherited by an ELK child graph.
    o.unnecessary_bendpoints=false;
    o.edge_label_spacing=2.4;o.ports.port_spacing=12;o.ports.port_label_spacing_vertical=1.2;return o;
}
double group_title_floor(const FlowchartNode& node,const FlowchartTextBox& label) {
    // The note and divider frames do not paint a title. ELK may still
    // reserve the measured note label's vertical strip.
    return node.shape==u"noteGroup"||node.shape==u"divider"?0:label.width+node.padding;
}

// Mermaid 12.2.0 subgraphFeedbackEdges.ts. Find feedback edges between
// direct children of each common subgraph in edge declaration order.
std::vector<bool> subgraph_feedback_edges(const ElkCompoundInput& input) {
    struct ProjectedEdge {
        std::size_t original, source, target;
        bool collapsed;
    };
    const auto root = input.nodes.size();
    const auto parent = [&](std::size_t node) { return input.nodes[node].parent.value_or(root); };
    std::vector<std::size_t> depths(root + 1);
    for (std::size_t n = 0; n < root; ++n)
        for (auto p = n; p != root; p = parent(p)) ++depths[n];

    std::map<std::size_t, std::vector<ProjectedEdge>> levels;
    for (std::size_t i = 0; i < input.edges.size(); ++i) {
        const auto [source, target] = input.edges[i];
        auto from = source, to = target;
        while (depths[from] > depths[to]) from = parent(from);
        while (depths[to] > depths[from]) to = parent(to);
        if (from == to) continue;
        while (parent(from) != parent(to)) {
            from = parent(from);
            to = parent(to);
        }
        levels[parent(from)].push_back({i, from, to, from != source || to != target});
    }

    std::vector<bool> flags(input.edges.size());
    for (const auto& [level, edges] : levels) {
        ElkCycleGraph graph;
        std::unordered_map<std::size_t, std::size_t> order;
        const auto port = [&](std::size_t node) {
            const auto index = order.try_emplace(node, order.size()).first->second;
            graph.ports.push_back({index});
            return graph.ports.size() - 1;
        };
        for (const auto& edge : edges) {
            const auto source = port(edge.source);
            const auto target = port(edge.target);
            graph.edges.push_back({source, target});
        }
        graph.node_count = order.size();
        const auto reversed = elk_depth_first_feedback_edges(graph);
        if (!reversed) throw std::logic_error(reversed.error());
        for (const auto i : *reversed) flags[edges[i].original] = edges[i].collapsed;
    }
    return flags;
}

#include "flowchart_elk_scopes.inc"

std::expected<ElkMeasuredScene,std::string> layout_scope(
    const FlowchartInput& source,std::span<const FlowchartNodeBox> boxes,
    std::span<const FlowchartEdgeLabelSize> labels,const FlowchartMeasurements& measured,std::span<const ElkEndLabel> end_labels, const ElkFlatLayoutOptions& options) {
    if(boxes.size()!=source.nodes.size()||labels.size()!=source.edges.size()||measured.nodes.size()!=boxes.size())
        return std::unexpected("ELK Flowchart needs measured nodes and labels in input order");
    try {
        ElkCompoundLayoutInput input;input.end_labels.assign(end_labels.begin(),end_labels.end());input.hierarchy.direction=options.direction;
        std::unordered_map<std::u16string,std::size_t> ids;
        for(std::size_t i=0;i<source.nodes.size();++i)ids.emplace(source.nodes[i].id,i);
        input.group_boxes.resize(boxes.size());input.group_options.resize(boxes.size());
        std::vector<std::vector<std::size_t>> children(boxes.size());
        for(std::size_t i=0;i<source.nodes.size();++i)
            if(!source.nodes[i].parent.empty())children[ids.at(source.nodes[i].parent)].push_back(i);
        for(std::size_t i=0;i<boxes.size();++i) {
            const auto& node=source.nodes[i];ElkCompoundInputNode n;
            // configureSubgraphNodes only installs compound options for groups
            // that actually have children. An empty group stays a zero-size
            // ELK leaf; its measured title only widens the painted frame later.
            n.group=node.group&&!children[i].empty();n.direction=options.direction;
            if(n.group && !node.direction.empty()) n.direction=root_options(node.direction).direction;
            if(!node.parent.empty())n.parent=ids.at(node.parent);
            input.hierarchy.nodes.push_back(n);
            if(n.group) {
                input.nodes.push_back({group_title_floor(node,measured.nodes[i]),0});
                input.group_boxes[i].padding={24,24,24,24};
                if(node.shape!=u"divider"||measured.nodes[i].height>0) {
                    const auto width=node.shape==u"noteGroup"?0:measured.nodes[i].width;
                    input.group_boxes[i].title=ElkGroupTitle{width,std::max(0.0,measured.nodes[i].height-2)};
                }
                input.group_options[i]=group_options();
            } else input.nodes.push_back(node.group?ElkMeasuredNode{}:ElkMeasuredNode{boxes[i].width,boxes[i].height});
        }
        for(std::size_t i=0;i<source.edges.size();++i) {
            const auto& e=source.edges[i];input.hierarchy.edges.push_back({ids.at(e.start),ids.at(e.end)});
            if(!e.label.empty())input.center_labels.push_back({i,i,labels[i].width,labels[i].height});
        }
        const auto* config = source.effective_config.find(u"elk");
        const auto* orient = config ? config->find(u"orientFeedbackEdges") : nullptr;
        const auto reversed = orient && orient->kind == ConfigKind::boolean && !orient->boolean
            ? std::vector<bool>(input.hierarchy.edges.size()) : subgraph_feedback_edges(input.hierarchy);
        for (std::size_t i = 0; i < reversed.size(); ++i)
            if (reversed[i]) std::swap(input.hierarchy.edges[i].source, input.hierarchy.edges[i].target);
        for (auto& label : input.end_labels) {
            if (label.original_edge >= reversed.size())
                return std::unexpected("ELK end label has no edge");
            if (reversed[label.original_edge]) label.head = !label.head;
        }
        const auto separate=separate_group_scopes(source,input.hierarchy);
        auto placed=std::ranges::any_of(separate,[](bool value){return value;})
            ? layout_directed_scopes(input,options,separate) : elk_layout_compound(input,options);
        if(!placed)return std::unexpected(placed.error());
        ElkMeasuredScene scene;scene.end_labels=placed->end_labels;auto& layout=scene.layout;layout.width=placed->width;layout.height=placed->height;
        layout.nodes.resize(boxes.size());layout.edges.resize(labels.size());
        for(std::size_t i=0;i<boxes.size();++i) {
            const auto& b=placed->nodes[i];
            layout.nodes[i]={source.nodes[i].id,b.x+b.width/2,b.y+b.height/2,b.width,b.height};
        }
        for(const auto& route:placed->routes) {
            auto& points=layout.edges[route.original_edge].points;points.push_back({route.start.x,route.start.y});
            for(auto p:route.bends)points.push_back({p.x,p.y});points.push_back({route.end.x,route.end.y});
            if (reversed[route.original_edge]) std::ranges::reverse(points);
        }
        for(const auto& l:placed->center_labels)layout.edges[l.original_edge].label=FlowchartPoint{l.x+l.width/2,l.y+l.height/2};
        // Original evenGroupFrames: deepest first; retain the unmodified absolute
        // route coordinates while pulling only painted container frames inward.
        const auto descendant=[&](std::size_t node,std::size_t group) {
            for(auto parent=input.hierarchy.nodes[node].parent;parent;parent=input.hierarchy.nodes[*parent].parent)
                if(*parent==group)return true;
            return false;
        };
        const auto tighten=[&](auto&& self,std::size_t group)->void {
            for(auto child:children[group])if(source.nodes[child].group)self(self,child);
            if(children[group].empty())return;
            double left=std::numeric_limits<double>::infinity(),right=-left,bottom=-left;
            const auto include=[&](double x,double y){left=std::min(left,x);right=std::max(right,x);bottom=std::max(bottom,y);};
            for(auto child:children[group]) {
                const auto& b=layout.nodes[child];
                if(b.width>0&&b.height>0){include(b.x-b.width/2,b.y-b.height/2);include(b.x+b.width/2,b.y+b.height/2);}
            }
            if(!std::isfinite(left))return;
            for(std::size_t i=0;i<input.hierarchy.edges.size();++i) {
                auto edge=input.hierarchy.edges[i];const auto& points=layout.edges[i].points;
                if (reversed[i]) std::swap(edge.source, edge.target);
                if(descendant(edge.source,group)&&descendant(edge.target,group))for(auto p:points)include(p.x,p.y);
                else {if(edge.source==group)include(points.front().x,points.front().y);if(edge.target==group)include(points.back().x,points.back().y);}
            }
            auto& frame=layout.nodes[group];const double ox=frame.x-frame.width/2,oy=frame.y-frame.height/2;
            left=std::max(ox,left-24);right=std::min(ox+frame.width,right+24);bottom=std::min(oy+frame.height,bottom+24);
            double width=right-left;const double floor=group_title_floor(source.nodes[group],measured.nodes[group]);
            if(width<floor){left-=(floor-width)/2;width=floor;left=std::max(ox,std::min(left,ox+frame.width-width));width=std::min(width,frame.width);}
            if(width>0&&bottom>oy){frame.x=left+width/2;frame.y=(oy+bottom)/2;frame.width=width;frame.height=bottom-oy;}
        };
        for(std::size_t i=0;i<boxes.size();++i)if(source.nodes[i].group&&!input.hierarchy.nodes[i].parent)tighten(tighten,i);
        // alignDegenerateNodeToAnchor: on a side shorter than ELK's 12px
        // surrounding margins, the clamped port can sit off-center. Keep the
        // routed line and move the node onto its first anchor, as upstream does.
        std::vector<bool> aligned(boxes.size());
        const auto align=[&](std::size_t i,ElkRoutingPoint anchor) {
            if(aligned[i]||source.nodes[i].shape==u"rect33")return;
            auto& node=layout.nodes[i];
            const bool along_width=std::abs(anchor.y-(node.y-node.height/2))<=0.5||
                std::abs(anchor.y-(node.y+node.height/2))<=0.5;
            if((along_width?node.width:node.height)>=24)return;
            aligned[i]=true;
            auto& center=along_width?node.x:node.y;
            const auto desired=along_width?anchor.x:anchor.y;
            if(std::abs(desired-center)>=0.01)center=desired;
        };
        for(const auto& route:placed->routes) {
            align(route.source_node,route.start);
            align(route.target_node,route.end);
        }
        // applyElkNodePositions widens an empty group's painted title without
        // changing the ELK node box used by its parent's frame tightening.
        for(std::size_t i=0;i<boxes.size();++i)
            if(source.nodes[i].group)layout.nodes[i].width=std::max(layout.nodes[i].width,measured.nodes[i].width);
        scene.min_x=scene.min_y=std::numeric_limits<double>::infinity();scene.max_x=scene.max_y=-scene.min_x;
        const auto include=[&](double x,double y,double w=0,double h=0){scene.min_x=std::min(scene.min_x,x-w/2);scene.max_x=std::max(scene.max_x,x+w/2);scene.min_y=std::min(scene.min_y,y-h/2);scene.max_y=std::max(scene.max_y,y+h/2);};
        for(const auto& node:layout.nodes)include(node.x,node.y,node.width,node.height);
        for(std::size_t i=0;i<layout.edges.size();++i){for(auto p:layout.edges[i].points)include(p.x,p.y);if(auto l=layout.edges[i].label)include(l->x,l->y,labels[i].width,labels[i].height);}
        if(!std::isfinite(scene.min_x))return std::unexpected("Flowchart graph has no painted content");
        return scene;
    }catch(const std::exception& e){return std::unexpected(e.what());}
}
} // namespace

std::expected<ElkMeasuredScene,std::string> layout_measured_elk_scene(
    const FlowchartInput& source,std::span<const FlowchartNodeBox> boxes,
    std::span<const FlowchartEdgeLabelSize> labels,const FlowchartMeasurements& measured,
    std::span<const ElkEndLabel> end_labels) {
    return layout_scope(source,boxes,labels,measured,end_labels,root_options(source.direction));
}

std::expected<ElkMeasuredScene,std::string> layout_measured_elk_subgraph(
    const FlowchartInput& source,std::span<const FlowchartNodeBox> boxes,
    std::span<const FlowchartEdgeLabelSize> labels,const FlowchartMeasurements& measured) {
    auto options=group_options();
    options.direction=root_options(source.direction).direction;
    return layout_scope(source,boxes,labels,measured,{},options);
}
std::expected<FlowchartScene,std::string> layout_measured_elk_flowchart(
    const FlowchartInput& source,std::span<const FlowchartNodeBox> boxes,
    std::span<const FlowchartEdgeLabelSize> labels,const FlowchartMeasurements& measured,const FlowchartSvgTheme&) {
    auto scene=layout_measured_elk_scene(source,boxes,labels,measured,{});
    if(!scene)return std::unexpected(scene.error());
    return std::move(static_cast<FlowchartScene&>(*scene));
}
}
