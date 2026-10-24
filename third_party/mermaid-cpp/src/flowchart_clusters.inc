// Mermaid 12.2.0 measureDagreGraph/renderDagreSubgraph, clusters.rect and
// positionNode. Included inside flowchart_svg's private namespace. MIT.
// A scene owns painted geometry in one coordinate space. Extracted scenes are
// measured before their parent layout, then translated once into that parent.
struct FlowchartScene {
    FlowchartLayout layout;
    double min_x=0,min_y=0,max_x=0,max_y=0,diff=0;
};

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
    const FlowchartPlacedNode* from_cluster,const FlowchartPlacedNode* to_cluster) {
    const auto original=edge.points;
    if(intersect_shapes) {
        edge.points.front()=intersect(start,start_shape,edge.points[1]);
        edge.points.back()=intersect(end,end_shape,edge.points[edge.points.size()-2]);
    }
    // The original toCluster branch deliberately starts from the Dagre path.
    if(to_cluster) edge.points=cut_at_cluster(original,*to_cluster);
    if(from_cluster) {
        std::reverse(edge.points.begin(),edge.points.end());
        edge.points=cut_at_cluster(edge.points,*from_cluster);
        std::reverse(edge.points.begin(),edge.points.end());
    }
    if(edge.points.size()<2) throw std::runtime_error("Flowchart group edge has no visible path");
    auto shortened=edge.points;
    if(source.arrow_start==u"arrow_point") shorten(shortened.front(),shortened[1],4);
    if(source.arrow_end==u"arrow_point") shorten(shortened.back(),shortened[shortened.size()-2],4);
    const auto path=curve_path(shortened,source.curve==u"linear");
    if(edge.label && (from_cluster||to_cluster||!label_coordinate_in_path(edge.points[edge.points.size()/2],path))) {
        const auto before=label_midpoint(original),after=label_midpoint(edge.points);
        edge.label->x+=after.x-before.x;edge.label->y+=after.y-before.y;
    }
}

FlowchartScene layout_prepared(const FlowchartInput& source,FlowchartPreparedGraph graph,
    std::span<const FlowchartNodeBox> boxes,std::span<const FlowchartEdgeLabelSize> labels,
    const FlowchartMeasurements& measured,const FlowchartSvgTheme& theme,
    const FlowchartPreparedNode* parent=nullptr) {
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
            children[i]=layout_prepared(source,std::move(nested),boxes,labels,measured,theme,&n);
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
            const double title_width=measured.nodes[*prepared.source_index].width+prepared.data.padding;
            const double width=std::max(n.width,title_width);
            scene.diff=(n.width<=title_width?(width-n.width)/2:0)-prepared.data.padding;
            n.width=width;
        }
        if(prepared.source_index && !children[i]) result.nodes[*prepared.source_index]=n;
        if(children[i]) {
            const auto& child=*children[i];
            const double x=n.x+child.diff-n.width/2,y=n.y-n.height/2-8;
            for(std::size_t j=0;j<child.layout.nodes.size();++j) if(!child.layout.nodes[j].id.empty()) {
                auto node=child.layout.nodes[j];node.x+=x;node.y+=y;result.nodes[j]=std::move(node);
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
        const double dx=(first.x+second.x)/2-node.x,dy=(first.y+second.y)/2-node.y;
        enum class Side {top,bottom,left,right};
        Side side=graph.direction==u"BT"?Side::bottom:graph.direction==u"LR"?Side::right:
            graph.direction==u"RL"?Side::left:Side::top;
        if(std::abs(dx)>std::abs(dy)) side=dx>0?Side::right:Side::left;
        else if(std::abs(dy)>0) side=dy>0?Side::bottom:Side::top;
        const auto label=labels[e.source_index];
        const double max_span=std::max(36.0,std::min(100.0,node.width*0.8));
        const double span=std::clamp(std::max(label.width,node.width*0.35),36.0,max_span);
        const double depth=std::clamp(std::min(node.width,node.height)*0.45,24.0,48.0);
        auto& out=result.edges[e.source_index];
        if(side==Side::left||side==Side::right) {
            const double sign=side==Side::right?1.0:-1.0,x=node.x+sign*node.width/2;
            out.points={{x,node.y-span/2},{x+sign*depth,node.y-span/2},{x+sign*depth,node.y+span/2},{x,node.y+span/2}};
            if(!original.label.empty()) out.label=FlowchartPoint{x+sign*(depth+label.width/2+4),node.y};
        } else {
            const double sign=side==Side::bottom?1.0:-1.0,y=node.y+sign*node.height/2;
            out.points={{node.x-span/2,y},{node.x-span/2,y+sign*depth},{node.x+span/2,y+sign*depth},{node.x+span/2,y}};
            if(!original.label.empty()) out.label=FlowchartPoint{node.x,y+sign*(depth+label.height/2+4)};
        }
    }
    for(const auto& e:graph.edges) if(e.loop_segment==-1||e.loop_segment==1) {
        auto& edge=result.edges[e.source_index];
        const auto& original=source.edges[e.source_index];
        const auto a=ids.at(e.loop_segment==1?original.start:e.data.start);
        const auto b=ids.at(e.loop_segment==1?original.end:e.data.end);
        finish_edge(edge,original,placed->nodes[a],placed->nodes[b],graph.nodes[a].data.shape,graph.nodes[b].data.shape,
            !children[a]&&!children[b],e.from_cluster.empty()?nullptr:&placed->nodes[ids.at(e.from_cluster)],
            e.to_cluster.empty()?nullptr:&placed->nodes[ids.at(e.to_cluster)]);
    }
    scene.min_x=scene.min_y=std::numeric_limits<double>::infinity();
    scene.max_x=scene.max_y=-std::numeric_limits<double>::infinity();
    const auto include=[&](double x,double y,double width=0,double height=0) {
        scene.min_x=std::min(scene.min_x,x-width/2);scene.max_x=std::max(scene.max_x,x+width/2);
        scene.min_y=std::min(scene.min_y,y-height/2);scene.max_y=std::max(scene.max_y,y+height/2);
    };
    for(const auto& n:result.nodes) if(!n.id.empty()) include(n.x,n.y,n.width,n.height);
    for(std::size_t i=0;i<result.edges.size();++i) {
        const auto& e=result.edges[i];
        for(auto p:e.points) include(p.x,p.y);
        if(e.label) include(e.label->x,e.label->y,labels[i].width,labels[i].height);
    }
    if(!std::isfinite(scene.min_x)) throw std::runtime_error("Flowchart graph has no painted content");
    return scene;
}
