// SPDX-License-Identifier: EPL-2.0
module folia.mermaid.elk_compound_layout;
import std;
import folia.mermaid.elk_order;

namespace folia::mermaid {
std::expected<ElkCompoundLayoutResult,std::string> elk_layout_compound(
    const ElkCompoundLayoutInput& original,const ElkFlatLayoutOptions& original_options) {
    const auto direction=original_options.direction;
    const bool vertical=direction==ElkLayoutDirection::down||direction==ElkLayoutDirection::up;
    // ElkGraphImporter.importHierarchicalGraph forces parentGraphDirection
    // (the top-level graph direction) on every included child graph.
    auto input=original;auto options=original_options;
    const auto label_direction=!vertical?ElkLabelDirection::horizontal:direction==ElkLayoutDirection::up?ElkLabelDirection::up:ElkLabelDirection::down;
    input.hierarchy.direction=ElkLayoutDirection::right;options.direction=ElkLayoutDirection::right;
    for(std::size_t n=0;n<input.nodes.size();++n) {
        input.hierarchy.nodes[n].direction=ElkLayoutDirection::right;
        if(n<input.group_boxes.size()&&input.hierarchy.nodes[n].group&&input.group_boxes[n].title) {
            const auto& title=*input.group_boxes[n].title;
            input.nodes[n].width=std::max(input.nodes[n].width,title.width+2*title.padding);
        }
        // Original hierarchy resizing consumes the graph minimum after its
        // parent's GraphTransformer has transposed the node property.
        // The child graph minimum therefore retains these original values.
        if(vertical&&!input.hierarchy.nodes[n].group)std::swap(input.nodes[n].width,input.nodes[n].height);
    }
    if(vertical){for(auto& l:input.center_labels)std::swap(l.width,l.height);for(auto& l:input.end_labels)std::swap(l.width,l.height);}
    if(input.nodes.size()!=input.hierarchy.nodes.size())return std::unexpected("ELK compound layout requires measured original nodes");
    auto prepared=elk_prepare_compound_graph(input.hierarchy);if(!prepared)return std::unexpected(prepared.error());auto& topology=*prepared;
    const auto graph_options=[&](std::size_t graph) {
        auto selected=options;
        if(const auto parent=topology.graphs[graph].parent_node) {
            const auto n=*topology.nodes[*parent].original;
            if(n<input.group_options.size()&&input.group_options[n])selected=*input.group_options[n];
        }
        selected.direction=ElkLayoutDirection::right;return selected;
    };
    for(auto& node:topology.nodes)if(node.boundary&&node.boundary->border_offset!=0)
        node.boundary->border_offset=graph_options(node.graph).placement.edge_edge/2;
    const auto absent=std::numeric_limits<std::size_t>::max();
    struct Local {
        ElkLabelPreparation labels;
        std::vector<ElkEndLabel> ends;
        std::vector<std::size_t> ports,segments;
        ElkRandom random;
        ElkFlatLayoutOptions options;
        explicit Local(const ElkFlatLayoutOptions& selected):random(static_cast<std::uint64_t>(selected.seed)),options(selected){}
    };
    std::vector<Local> locals;locals.reserve(topology.graphs.size());
    std::vector<std::size_t> local_node(topology.nodes.size()),local_port(topology.ports.size(),absent),local_segment(topology.segments.size());
    for(std::size_t gi=0;gi<topology.graphs.size();++gi) {
        const auto& sub=topology.graphs[gi];auto& local=locals.emplace_back(graph_options(gi));
        const auto& options=local.options;
        ElkCycleGraph source{.node_count=sub.nodes.size()};std::vector<ElkExternalLayerRole> roles(sub.nodes.size());
        for(std::size_t n=0;n<sub.nodes.size();++n)local_node[sub.nodes[n]]=n;
        for(const auto n:sub.nodes)for(const auto p:topology.nodes[n].ports) {
            local_port[p]=source.ports.size();local.ports.push_back(p);source.ports.push_back({local_node[n]});
        }
        for(const auto s:sub.segments) {
            const auto& segment=topology.segments[s];local_segment[s]=source.edges.size();local.segments.push_back(s);
            source.edges.push_back({local_port[segment.source_port],local_port[segment.target_port]});
        }
        for(const auto n:sub.nodes)if(const auto& boundary=topology.nodes[n].boundary)
            roles[local_node[n]]=boundary->side==ElkPortSide::west?ElkExternalLayerRole::first_separate:ElkExternalLayerRole::last_separate;
        std::vector<ElkInlineCenterLabel> centers;
        for(auto label:input.center_labels) {
            const auto s=topology.edges[label.original_edge].center_segment;
            if(topology.segments[s].graph==gi){label.original_edge=local_segment[s];centers.push_back(label);}
        }
        for(auto label:input.end_labels) {
            const auto& chain=topology.edges[label.original_edge];
            const auto s=label.head?chain.head_segment:chain.tail_segment;
            if(topology.segments[s].graph==gi){label.original_edge=local_segment[s];local.ends.push_back(label);}
        }
        auto labels=elk_prepare_inline_labels(source,centers,label_direction,
            options.edge_label_spacing,options.label_label_spacing,roles);
        if(!labels)return std::unexpected(labels.error());local.labels=std::move(*labels);auto& g=local.labels.graph;
        for(const auto n:sub.nodes) {
            auto& node=g.nodes[local_node[n]];
            if(topology.nodes[n].boundary){node.kind=ElkLayerNodeKind::external_port;node.fixed_port_positions=true;}
            else {const auto& measured=input.nodes[*topology.nodes[n].original];node.width=measured.width;node.height=measured.height;}
        }
        elk_split_long_edges(g);elk_assign_flat_port_sides(g);
        if(const auto sorted=elk_sort_flat_ports(g);!sorted)return std::unexpected(sorted.error());
    }
    std::vector<ElkHierarchyOrderGraph> order(topology.graphs.size());
    for(std::size_t gi=0;gi<order.size();++gi) {
        auto& entry=order[gi];const auto& sub=topology.graphs[gi];entry.graph=&locals[gi].labels.graph;entry.random=&locals[gi].random;
        if(sub.parent_node){entry.parent_graph=topology.nodes[*sub.parent_node].graph;entry.parent_node=local_node[*sub.parent_node];}
        for(const auto n:sub.nodes)if(topology.nodes[n].boundary_port)
            entry.ports.push_back({local_node[n],local_port[*topology.nodes[n].boundary_port]});
    }
    if(const auto ordered=elk_order_hierarchy(order);!ordered)return std::unexpected(ordered.error());
    ElkCompoundGeometry geometry{.graphs=std::vector<ElkCompoundContent>(order.size()),
        .node_positions=std::vector<ElkRoutingPoint>(topology.nodes.size()),.port_positions=std::vector<ElkRoutingPoint>(topology.ports.size()),
        .segments=std::vector<ElkCompoundSegmentGeometry>(topology.segments.size())};
    std::vector<ElkNodeBox> boxes(input.nodes.size());
    std::vector<std::pair<std::size_t,ElkLabelPosition>> center_positions,end_positions;
    std::vector<std::size_t> breadth{0};
    for(std::size_t i=0;i<breadth.size();++i)for(const auto n:topology.graphs[breadth[i]].nodes)
        if(topology.nodes[n].nested_graph)breadth.push_back(*topology.nodes[n].nested_graph);
    for(const auto gi:breadth|std::views::reverse) {
        auto& local=locals[gi];auto& g=local.labels.graph;const auto& sub=topology.graphs[gi];
        const auto& options=local.options;
        std::vector<ElkPointPortOptions> port_options(g.nodes.size(),options.ports);
        for(const auto n:sub.nodes)if(topology.nodes[n].original) {
            auto& node=g.nodes[local_node[n]];port_options[local_node[n]].center_alignment=input.nodes[*topology.nodes[n].original].center_ports;
            if(const auto nested=topology.nodes[n].nested_graph) {
                const auto& content=geometry.graphs[*nested];node.width=content.width+content.padding_left+content.padding_right;
                node.height=content.height+content.padding_top+content.padding_bottom;node.fixed_port_positions=true;
                const auto original_node=*topology.nodes[n].original;
                if(vertical&&original_node<input.group_boxes.size()&&input.group_boxes[original_node].title) {
                    const auto overhang=std::max(0.0,(input.group_boxes[original_node].title->width-node.height)/2);
                    node.margin_top=std::max(node.margin_top,overhang);node.margin_bottom=std::max(node.margin_bottom,overhang);
                }
                for(const auto p:topology.nodes[n].ports){auto& port=g.ports[local_port[p]];port.x=geometry.port_positions[p].x;port.y=geometry.port_positions[p].y;}
            }
        }
        if(const auto placed=elk_place_flat_point_ports(g,port_options);!placed)return std::unexpected(placed.error());
        elk_switch_label_dummies(g);elk_place_inline_label_ports(g,local.labels.dummies,options.edge_label_spacing);
        std::vector<ElkHorizontalMargin> margins(g.nodes.size());
        for(std::size_t n=0;n<g.nodes.size();++n)for(const auto p:g.nodes[n].ports) {
            margins[n].left=std::max(margins[n].left,-g.ports[p].x);
            margins[n].right=std::max(margins[n].right,g.ports[p].x-g.nodes[n].width);
        }
        const auto end_cells=elk_prepare_end_labels(g,local.ends,margins,vertical,options.edge_label_spacing,options.label_label_spacing,vertical,false);
        if(!end_cells)return std::unexpected(end_cells.error());
        const auto y=elk_place_flat_nodes(g,options.placement,false);if(!y)return std::unexpected(y.error());
        double minimum=std::numeric_limits<double>::infinity(),maximum=-minimum;
        for(const auto& layer:g.layers)if(!layer.empty()) {
            const auto first=layer.front(),last=layer.back();
            auto top=(*y)[first]-g.nodes[first].margin_top,bottom=(*y)[last]+g.nodes[last].height+g.nodes[last].margin_bottom;
            if(g.nodes[first].kind==ElkLayerNodeKind::external_port)top-=options.ports.surrounding_margin;
            if(g.nodes[last].kind==ElkLayerNodeKind::external_port)bottom+=options.ports.surrounding_margin;
            minimum=std::min(minimum,top);maximum=std::max(maximum,bottom);
        }
        if(g.nodes.empty())minimum=maximum=0;
        auto routed=elk_route_flat_edges(g,*y,margins,local.random,options.routing);if(!routed)return std::unexpected(routed.error());
        ElkGraphPadding padding{options.padding,options.padding,options.padding,options.padding};
        if(sub.parent_node) {
            const auto n=*topology.nodes[*sub.parent_node].original;
            if(n<input.group_boxes.size())padding=elk_group_content_padding(input.group_boxes[n]);
        }
        padding=elk_normalize_padding(padding,direction);
        auto& content=geometry.graphs[gi];content={.width=routed->width,.height=maximum-minimum,
            .padding_left=padding.left,.padding_right=padding.right,.padding_top=padding.top,.padding_bottom=padding.bottom,.offset={0,-minimum}};
        std::vector<ElkCompoundPortDummy> dummies;std::vector<std::size_t> dummy_nodes;
        for(const auto n:sub.nodes)if(topology.nodes[n].boundary) {
            auto dummy=*topology.nodes[n].boundary;dummy.position={routed->node_x[local_node[n]],(*y)[local_node[n]]};
            dummies.push_back(dummy);dummy_nodes.push_back(n);
        }
        if(sub.parent_node) {
            const auto& measured=input.nodes[*topology.nodes[*sub.parent_node].original];
            elk_resize_compound_content(content,dummies,measured.width,measured.height);
        }
        for(std::size_t i=0;i<dummies.size();++i) {
            const auto n=dummy_nodes[i];geometry.port_positions[*topology.nodes[n].boundary_port]=elk_export_compound_port(content,dummies[i]);
            routed->node_x[local_node[n]]=dummies[i].position.x;
        }
        auto routes=elk_restore_flat_routes(g,*routed,*y,options.unnecessary_bendpoints);if(!routes)return std::unexpected(routes.error());
        for(auto& route:*routes)geometry.segments[local.segments[route.original_edge]].bends=std::move(route.bends);
        auto center_labels=elk_restore_inline_labels(g,local.labels.dummies,routed->node_x,*y,label_direction,options.label_label_spacing);
        auto end_labels=elk_restore_end_labels(*end_cells,routed->node_x,*y,vertical,options.label_label_spacing);
        for(auto label:center_labels) {
            const auto s=local.segments[label.original_edge];label.original_edge=topology.segments[s].original_edge;
            center_positions.emplace_back(gi,label);
        }
        for(auto label:end_labels) {
            const auto s=local.segments[label.original_edge];label.original_edge=topology.segments[s].original_edge;
            end_positions.emplace_back(gi,label);
        }
        for(const auto n:sub.nodes) {
            const auto ln=local_node[n];geometry.node_positions[n]={routed->node_x[ln],(*y)[ln]};
            if(topology.nodes[n].original)boxes[*topology.nodes[n].original]={routed->node_x[ln],(*y)[ln],g.nodes[ln].width,g.nodes[ln].height};
        }
        for(std::size_t p=0;p<local.ports.size();++p)geometry.port_positions[local.ports[p]]={g.ports[p].x,g.ports[p].y};
    }
    const auto origin=[&](std::size_t gi) {
        ElkRoutingPoint point{options.padding,options.padding};
        for(;;){const auto& content=geometry.graphs[gi];point.x+=content.offset.x;point.y+=content.offset.y;
            const auto parent=topology.graphs[gi].parent_node;if(!parent)break;
            point.x+=content.padding_left+geometry.node_positions[*parent].x;point.y+=content.padding_top+geometry.node_positions[*parent].y;
            gi=topology.nodes[*parent].graph;}
        return point;
    };
    ElkCompoundLayoutResult result;result.nodes=std::move(boxes);
    result.width=geometry.graphs[0].width+2*options.padding;result.height=geometry.graphs[0].height+2*options.padding;
    for(std::size_t n=0;n<input.nodes.size();++n){const auto delta=origin(topology.nodes[n].graph);result.nodes[n].x+=delta.x;result.nodes[n].y+=delta.y;}
    for(auto [gi,label]:center_positions){const auto delta=origin(gi);label.x+=delta.x;label.y+=delta.y;result.center_labels.push_back(label);}
    for(auto [gi,label]:end_positions){const auto delta=origin(gi);label.x+=delta.x;label.y+=delta.y;result.end_labels.push_back(label);}
    for(auto& edge:elk_restore_compound_edges(topology,geometry,options.unnecessary_bendpoints)) {
        const auto delta=origin(edge.reference_graph);const auto shift=[&](ElkRoutingPoint p){return ElkRoutingPoint{p.x+delta.x,p.y+delta.y};};
        const auto& endpoints=input.hierarchy.edges[edge.original_edge];
        ElkRestoredRoute route{.original_edge=edge.original_edge,.source_node=endpoints.source,.target_node=endpoints.target,
            .source_port=edge.source_port,.target_port=edge.target_port,.start=shift(edge.start),.end=shift(edge.end),.bends=std::move(edge.bends)};
        for(auto& p:route.bends)p=shift(p);result.routes.push_back(std::move(route));
    }
    const auto width=result.width,height=result.height;
    const auto transform=[&](ElkNodeBox box){return elk_export_direction_box(box,width,height,direction);};
    const auto transform_point=[&](ElkRoutingPoint p){const auto b=transform({p.x,p.y,0,0});return ElkRoutingPoint{b.x,b.y};};
    for(auto& box:result.nodes)box=transform(box);
    for(auto& route:result.routes){route.start=transform_point(route.start);route.end=transform_point(route.end);for(auto& p:route.bends)p=transform_point(p);}
    for(auto* labels:{&result.center_labels,&result.end_labels})for(auto& label:*labels){const auto b=transform({label.x,label.y,label.width,label.height});label.x=b.x;label.y=b.y;label.width=b.width;label.height=b.height;}
    if(vertical)std::swap(result.width,result.height);
    result.group_titles.resize(input.nodes.size());
    for(std::size_t n=0;n<input.group_boxes.size();++n)if(input.hierarchy.nodes[n].group&&input.group_boxes[n].title)
        result.group_titles[n]=elk_group_title_box(result.nodes[n],*input.group_boxes[n].title);
    return result;
}
}
