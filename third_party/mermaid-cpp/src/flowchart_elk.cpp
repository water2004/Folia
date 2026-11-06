module folia.mermaid.flowchart_elk;
import std;
import folia.mermaid.elk_compound_layout;

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
    o.edge_label_spacing=2.4;o.ports.port_spacing=12;o.ports.port_label_spacing_vertical=1.2;return o;
}
}
std::expected<FlowchartScene,std::string> layout_measured_elk_flowchart(
    const FlowchartInput& source,std::span<const FlowchartNodeBox> boxes,
    std::span<const FlowchartEdgeLabelSize> labels,const FlowchartMeasurements& measured,const FlowchartSvgTheme&) {
    if(boxes.size()!=source.nodes.size()||labels.size()!=source.edges.size()||measured.nodes.size()!=boxes.size())
        return std::unexpected("ELK Flowchart needs measured nodes and labels in input order");
    try {
        ElkCompoundLayoutInput input;const auto options=root_options(source.direction);input.hierarchy.direction=options.direction;
        std::unordered_map<std::u16string,std::size_t> ids;
        for(std::size_t i=0;i<source.nodes.size();++i)ids.emplace(source.nodes[i].id,i);
        input.group_boxes.resize(boxes.size());input.group_options.resize(boxes.size());
        std::vector<std::vector<std::size_t>> children(boxes.size());
        for(std::size_t i=0;i<boxes.size();++i) {
            const auto& node=source.nodes[i];ElkCompoundInputNode n;n.group=node.group;n.direction=options.direction;
            if(!node.parent.empty()){n.parent=ids.at(node.parent);children[*n.parent].push_back(i);}
            input.hierarchy.nodes.push_back(n);
            if(node.group) {
                input.nodes.push_back({measured.nodes[i].width+node.padding,0});
                input.group_boxes[i].padding={24,24,24,24};
                input.group_boxes[i].title=ElkGroupTitle{measured.nodes[i].width,std::max(0.0,measured.nodes[i].height-2)};
                input.group_options[i]=group_options();
            } else input.nodes.push_back({boxes[i].width,boxes[i].height});
        }
        for(std::size_t i=0;i<source.edges.size();++i) {
            const auto& e=source.edges[i];input.hierarchy.edges.push_back({ids.at(e.start),ids.at(e.end)});
            if(!e.label.empty())input.center_labels.push_back({i,i,labels[i].width,labels[i].height});
        }
        auto placed=elk_layout_compound(input,options);if(!placed)return std::unexpected(placed.error());
        FlowchartScene scene;auto& layout=scene.layout;layout.width=placed->width;layout.height=placed->height;
        layout.nodes.resize(boxes.size());layout.edges.resize(labels.size());
        for(std::size_t i=0;i<boxes.size();++i) {
            const auto& b=placed->nodes[i];layout.nodes[i]={source.nodes[i].id,b.x+b.width/2,b.y+b.height/2,b.width,b.height};
        }
        for(const auto& route:placed->routes) {
            auto& points=layout.edges[route.original_edge].points;points.push_back({route.start.x,route.start.y});
            for(auto p:route.bends)points.push_back({p.x,p.y});points.push_back({route.end.x,route.end.y});
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
            for(auto child:children[group]){const auto& b=layout.nodes[child];include(b.x-b.width/2,b.y-b.height/2);include(b.x+b.width/2,b.y+b.height/2);}
            for(std::size_t i=0;i<input.hierarchy.edges.size();++i) {
                const auto& edge=input.hierarchy.edges[i];const auto& points=layout.edges[i].points;
                if(descendant(edge.source,group)&&descendant(edge.target,group))for(auto p:points)include(p.x,p.y);
                else {if(edge.source==group)include(points.front().x,points.front().y);if(edge.target==group)include(points.back().x,points.back().y);}
            }
            auto& frame=layout.nodes[group];const double ox=frame.x-frame.width/2,oy=frame.y-frame.height/2;
            left=std::max(ox,left-24);right=std::min(ox+frame.width,right+24);bottom=std::min(oy+frame.height,bottom+24);
            double width=right-left;const double floor=measured.nodes[group].width+source.nodes[group].padding;
            if(width<floor){left-=(floor-width)/2;width=floor;left=std::max(ox,std::min(left,ox+frame.width-width));width=std::min(width,frame.width);}
            if(width>0&&bottom>oy){frame.x=left+width/2;frame.y=(oy+bottom)/2;frame.width=width;frame.height=bottom-oy;}
        };
        for(std::size_t i=0;i<boxes.size();++i)if(source.nodes[i].group&&!input.hierarchy.nodes[i].parent)tighten(tighten,i);
        scene.min_x=scene.min_y=std::numeric_limits<double>::infinity();scene.max_x=scene.max_y=-scene.min_x;
        const auto include=[&](double x,double y,double w=0,double h=0){scene.min_x=std::min(scene.min_x,x-w/2);scene.max_x=std::max(scene.max_x,x+w/2);scene.min_y=std::min(scene.min_y,y-h/2);scene.max_y=std::max(scene.max_y,y+h/2);};
        for(const auto& node:layout.nodes)include(node.x,node.y,node.width,node.height);
        for(std::size_t i=0;i<layout.edges.size();++i){for(auto p:layout.edges[i].points)include(p.x,p.y);if(auto l=layout.edges[i].label)include(l->x,l->y,labels[i].width,labels[i].height);}
        if(!std::isfinite(scene.min_x))return std::unexpected("Flowchart graph has no painted content");
        return scene;
    }catch(const std::exception& e){return std::unexpected(e.what());}
}
}

