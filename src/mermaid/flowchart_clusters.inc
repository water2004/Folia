// Mermaid 12.2.0 measureDagreGraph/renderDagreSubgraph, clusters.rect and
// positionNode. Included inside flowchart_svg's private namespace. MIT.
// A scene owns painted geometry in one coordinate space. Extracted scenes are
// measured before their parent layout, then translated once into that parent.
FlowchartPoint cluster_intersection(const FlowchartPlacedNode& n,FlowchartPoint outside,FlowchartPoint inside) {
    const double x=n.x,y=n.y,w=n.width/2,h=n.height/2;
    const double q_distance=std::abs(outside.y-inside.y),r_distance=std::abs(outside.x-inside.x);
    double r,q;
    FlowchartPoint result;
    if(std::abs(y-outside.y)*w>std::abs(x-outside.x)*h) {
        q=inside.y<outside.y?outside.y-h-y:y-h-outside.y;
        r=r_distance*q/q_distance;
        result={inside.x<outside.x?inside.x+r:inside.x-r_distance+r,
            inside.y<outside.y?inside.y+q_distance-q:inside.y-q_distance+q};
    } else {
        r=inside.x<outside.x?outside.x-w-x:x-w-outside.x;
        q=q_distance*r/r_distance;
        result={inside.x<outside.x?inside.x+r_distance-r:inside.x-r_distance+r,
            inside.y<outside.y?inside.y+q:inside.y-q};
    }
    if(r==0) result=outside;
    if(r_distance==0) result.x=outside.x;
    if(q_distance==0) result.y=outside.y;
    return result;
}

std::vector<FlowchartPoint> cut_at_cluster(std::span<const FlowchartPoint> points,const FlowchartPlacedNode& n) {
    std::vector<FlowchartPoint> result;
    auto last=points.front();bool inside=false;
    for(auto p:points) {
        if(std::abs(p.x-n.x)<n.width/2 && std::abs(p.y-n.y)<n.height/2 && !inside) {
            const auto cross=cluster_intersection(n,last,p);
            if(!std::ranges::any_of(result,[&](auto a){return a.x==cross.x&&a.y==cross.y;})) result.push_back(cross);
            inside=true;
        } else {last=p;if(!inside) result.push_back(p);}
    }
    return result;
}

void finish_edge(FlowchartPlacedEdge& edge,const FlowchartEdge& source,
    const FlowchartPlacedNode& start,const FlowchartPlacedNode& end,
    std::u16string_view start_shape,std::u16string_view end_shape,bool intersect_shapes,
    const FlowchartPlacedNode* from_cluster,const FlowchartPlacedNode* to_cluster,bool neo,
    const FlowchartLocalShape* start_shape_geometry,const FlowchartLocalShape* end_shape_geometry) {
    const auto original=edge.points;
    if(intersect_shapes) {
        edge.points.front()=intersect(start,start_shape,edge.points[1],neo,start_shape_geometry);
        edge.points.back()=intersect(end,end_shape,edge.points[edge.points.size()-2],neo,end_shape_geometry);
    }
    // The original toCluster branch deliberately starts from the Dagre path.
    if(to_cluster) edge.points=cut_at_cluster(original,*to_cluster);
    if(from_cluster) {
        std::reverse(edge.points.begin(),edge.points.end());
        edge.points=cut_at_cluster(edge.points,*from_cluster);
        std::reverse(edge.points.begin(),edge.points.end());
    }
    if(edge.points.size()<2) throw std::runtime_error("Flowchart group edge has no visible path");
    const auto path=flowchart_edge_path(edge.points,source.curve,source);
    if(edge.label && (from_cluster||to_cluster||!label_coordinate_in_path(edge.points[edge.points.size()/2],path))) {
        const auto before=label_midpoint(original),after=label_midpoint(edge.points);
        edge.label->x+=after.x-before.x;edge.label->y+=after.y-before.y;
    }
}

FlowchartScene layout_prepared(const FlowchartInput& source,FlowchartPreparedGraph graph,
    std::span<const FlowchartNodeBox> boxes,std::span<const FlowchartEdgeLabelSize> labels,
    FlowchartMeasurements& measured,const FlowchartSvgTheme& theme,
    const FlowchartPreparedNode* parent=nullptr,std::vector<RoughFlowchartNode>* rough_nodes=nullptr,
    FlowchartLocalShapes local_shapes={}) {
    if(parent) {
        for(auto& n:graph.nodes) if(n.data.parent.empty()) n.data.parent=parent->data.id;
        auto container=*parent;container.subgraph.reset();container.data.parent.clear();
        graph.nodes.push_back(std::move(container));
    }
    FlowchartInput input;input.direction=graph.direction;
    input.node_spacing=graph.node_spacing;input.rank_spacing=graph.rank_spacing;
    std::vector<FlowchartNodeBox> layout_boxes;
    std::vector<FlowchartEdgeLabelSize> layout_labels;
    std::unordered_map<std::u16string,std::size_t> ids;
    std::vector<std::optional<FlowchartScene>> children(graph.nodes.size());
    for(std::size_t i=0;i<graph.nodes.size();++i) {
        const auto& n=graph.nodes[i];ids.emplace(n.data.id,i);input.nodes.push_back(n.data);
        if(n.subgraph) {
            auto nested=graph.subgraphs[*n.subgraph];
            nested.node_spacing=graph.node_spacing;nested.rank_spacing=graph.rank_spacing+25;
            children[i]=layout_prepared(source,std::move(nested),boxes,labels,measured,theme,&n,rough_nodes,local_shapes);
            const auto& child=*children[i];
            layout_boxes.push_back({child.max_x-child.min_x,child.max_y-child.min_y});
        } else layout_boxes.push_back(n.source_index?boxes[*n.source_index]:FlowchartNodeBox{0.1,0.1});
    }
    for(const auto& e:graph.edges) {
        input.edges.push_back(e.data);
        layout_labels.push_back(e.loop_segment==0||e.loop_segment==2?FlowchartEdgeLabelSize{}:labels[e.source_index]);
    }
    auto placed=layout_flowchart(input,layout_boxes,layout_labels);
    if(!placed) throw std::runtime_error(placed.error());
    // The root's margin is supplied by the SVG wrapper. Recursive graphs retain
    // Dagre's 8px margins because positionNode subtracts that local padding.
    const double margin_x=parent?graph.margin_x:0,margin_y=parent?graph.margin_y:0;
    const double title_margin=theme.group_title_top+theme.group_title_bottom;
    FlowchartScene scene;auto& result=scene.layout;
    result.nodes.resize(source.nodes.size());result.edges.resize(source.edges.size());
    std::vector<bool> compound(graph.nodes.size());
    for(const auto& n:graph.nodes) if(!n.data.parent.empty()) compound[ids.at(n.data.parent)]=true;
    for(std::size_t i=0;i<graph.nodes.size();++i) {
        auto& n=placed->nodes[i];const auto& prepared=graph.nodes[i];
        n.x+=margin_x;n.y+=margin_y;
        if(children[i]) n.y+=title_margin;
        else if(compound[i]) n.height+=title_margin;
        else n.y+=title_margin/2;
        if(compound[i]) {
            if(prepared.data.label_type==u"markdown" && measured.measure_group) {
                auto label=measured.measure_group(*prepared.source_index,n.width);
                if(!label) throw std::runtime_error(label.error());
                measured.nodes[*prepared.source_index]=std::move(*label);
            }
            const double title_width=measured.nodes[*prepared.source_index].width+prepared.data.padding;
            const double width=std::max(n.width,title_width);
            scene.diff=(n.width<=title_width?(width-n.width)/2:0)-prepared.data.padding;
            n.width=width;
            if(rough_nodes) {
                // Insert the frame in the local graph before measuring the
                // enclosing scene. A translated hatch would round a different
                // set of scanlines, and parent Dagre needs the painted size.
                auto& rough=(*rough_nodes)[*prepared.source_index];
                rough.drawing=rough_cluster_frame(n,theme);
                if(const auto& b=rough.drawing.bounds) {
                    rough.label_offset.y=(b->height()-n.height)/2;
                    n.width=b->width();
                    n.height=b->height();
                }
            }
        }
        if(prepared.source_index && !children[i]) result.nodes[*prepared.source_index]=n;
        if(children[i]) {
            const auto& child=*children[i];
            const double x=n.x+child.diff-n.width/2,y=n.y-n.height/2-8;
            for(std::size_t j=0;j<child.layout.nodes.size();++j) if(!child.layout.nodes[j].id.empty()) {
                auto node=child.layout.nodes[j];node.x+=x;node.y+=y;result.nodes[j]=std::move(node);
                if(rough_nodes&&source.nodes[j].group) {
                    auto& offset=(*rough_nodes)[j].offset;
                    offset.x+=x;offset.y+=y;
                }
            }
            for(std::size_t j=0;j<child.layout.edges.size();++j) if(!child.layout.edges[j].points.empty()) {
                auto edge=child.layout.edges[j];
                for(auto& p:edge.points) {p.x+=x;p.y+=y;}
                if(edge.label) {edge.label->x+=x;edge.label->y+=y;}
                result.edges[j]=std::move(edge);
            }
        }
    }
    // Both positionEdgeLabel (recursive paint) and positionRenderedEdgeLabel
    // (root paint) add half the title margin to the label's Dagre center.
    for(auto& edge:placed->edges) {
        for(auto& p:edge.points) {p.x+=margin_x;p.y+=margin_y+title_margin/2;}
        if(edge.label) {edge.label->x+=margin_x;edge.label->y+=margin_y+title_margin/2;}
    }
    for(std::size_t i=0;i<graph.edges.size();++i) {
        const auto& e=graph.edges[i];
        if(e.loop_segment==-1) {result.edges[e.source_index]=placed->edges[i];continue;}
        if(e.loop_segment!=1) continue;
        // Compact self-loop construction follows the same measured dummy-node
        // hints as the ungrouped adapter, in this graph's own direction.
        const auto& original=source.edges[e.source_index];
        const auto& node=placed->nodes[ids.at(original.start)];
        const auto& first=placed->nodes[ids.at(e.data.start)];
        const auto& second=placed->nodes[ids.at(e.data.end)];
        result.edges[e.source_index]=route_dagre_self_loop(node,
            {(first.x+second.x)/2,(first.y+second.y)/2},graph.direction,
            labels[e.source_index],!original.label.empty());
    }
    for(const auto& e:graph.edges) if(e.loop_segment==-1||e.loop_segment==1) {
        auto& edge=result.edges[e.source_index];
        const auto& original=source.edges[e.source_index];
        const auto a=ids.at(e.loop_segment==1?original.start:e.data.start);
        const auto b=ids.at(e.loop_segment==1?original.end:e.data.end);
        const auto local_shape=[&](std::size_t i) -> const FlowchartLocalShape* {
            const auto index=graph.nodes[i].source_index;
            return index?find_local_shape(local_shapes,*index):nullptr;
        };
        finish_edge(edge,original,placed->nodes[a],placed->nodes[b],graph.nodes[a].data.shape,graph.nodes[b].data.shape,
            !children[a]&&!children[b],e.from_cluster.empty()?nullptr:&placed->nodes[ids.at(e.from_cluster)],
            e.to_cluster.empty()?nullptr:&placed->nodes[ids.at(e.to_cluster)],theme.neo,local_shape(a),local_shape(b));
    }
    scene.min_x=scene.min_y=std::numeric_limits<double>::infinity();
    scene.max_x=scene.max_y=-std::numeric_limits<double>::infinity();
    const auto include=[&](double x,double y,double width=0,double height=0) {
        scene.min_x=std::min(scene.min_x,x-width/2);scene.max_x=std::max(scene.max_x,x+width/2);
        scene.min_y=std::min(scene.min_y,y-height/2);scene.max_y=std::max(scene.max_y,y+height/2);
    };
    for(std::size_t i=0;i<result.nodes.size();++i) {
        const auto& n=result.nodes[i];
        if(n.id.empty()) continue;
        if(rough_nodes&&(*rough_nodes)[i].drawing.bounds) {
            const auto& rough=(*rough_nodes)[i];
            const auto& b=*rough.drawing.bounds;
            const auto x=rough.offset.x+(source.nodes[i].group?0:n.x);
            const auto y=rough.offset.y+(source.nodes[i].group?0:n.y);
            include(x+b.left,y+b.top);
            include(x+b.right,y+b.bottom);
        } else if(const auto* shape=find_local_shape(local_shapes,i)) {
            const auto& bounds=shape->paint_bounds;
            include(n.x+bounds.left,n.y+bounds.top);
            include(n.x+bounds.right,n.y+bounds.bottom);
        } else include(n.x,n.y,n.width,n.height);
    }
    for(std::size_t i=0;i<result.edges.size();++i) {
        const auto& e=result.edges[i];
        for(auto p:e.points) include(p.x,p.y);
        if(e.label) include(e.label->x,e.label->y,labels[i].width,labels[i].height);
    }
    if(!std::isfinite(scene.min_x)) throw std::runtime_error("Flowchart graph has no painted content");
    return scene;
}
