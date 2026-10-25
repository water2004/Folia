module folia.mermaid.class_svg;
import std;
import folia.mermaid.number;
import folia.mermaid.svg_text;
import folia.mermaid.flowchart_graph;
import folia.mermaid.flowchart_svg;

namespace folia::mermaid {
namespace {
std::string num(double v) {
    auto n = format_number(v); if (!n) throw std::runtime_error("Invalid Class coordinate"); return *n;
}
std::string xml(std::u16string_view value) {
    auto s = escape_svg_text(value,u"Class text"); if (!s) throw std::runtime_error("Invalid Class XML text");
    std::string out; for (auto c : *s) out += c == '"' ? "&quot;" : std::string(1,c); return out;
}
std::string point(FlowchartPoint p) { return num(p.x)+","+num(p.y); }
FlowchartPoint mix(FlowchartPoint a,FlowchartPoint b,double x,double y,double d) {
    return {(a.x*x+b.x*y)/d,(a.y*x+b.y*y)/d};
}
// D3 curveBasis, as used by the original classic Class edge painter.
std::string curve(std::span<const FlowchartPoint> p) {
    if (p.empty()) return {};
    std::string out = "M"+point(p.front());
    if (p.size()<3) {for(std::size_t i=1;i<p.size();++i) out+="L"+point(p[i]);return out;}
    out += "L"+point(mix(p[0],p[1],5,1,6));
    const auto segment = [&](FlowchartPoint a,FlowchartPoint b,FlowchartPoint c) {
        out += "C"+point(mix(a,b,2,1,3))+" "+point(mix(a,b,1,2,3))+" "+
            point({(a.x+4*b.x+c.x)/6,(a.y+4*b.y+c.y)/6});
    };
    for(std::size_t i=2;i<p.size();++i) segment(p[i-2],p[i-1],p[i]);
    segment(p[p.size()-2],p.back(),p.back());return out+"L"+point(p.back());
}
void shorten(FlowchartPoint& p,FlowchartPoint q,ClassMarker marker) {
    const double amount = marker==ClassMarker::none?0:marker==ClassMarker::dependency?6:17.25;
    const double length=std::hypot(q.x-p.x,q.y-p.y);
    if(length>0) {p.x+=(q.x-p.x)*amount/length;p.y+=(q.y-p.y)*amount/length;}
}
std::string marker_id(ClassMarker marker,bool start) {
    return "class-marker-"+std::to_string(static_cast<int>(marker))+(start?"-start":"-end");
}
std::string markers(const ClassSvgTheme& theme) {
    std::string out="<defs>";
    for(auto type:{ClassMarker::aggregation,ClassMarker::extension,ClassMarker::composition,ClassMarker::dependency})
        for(bool start:{false,true}) {
            const bool dep=type==ClassMarker::dependency,ext=type==ClassMarker::extension;
            const std::string path=ext?(start?"M 1,7 L18,13 V 1 Z":"M 1,1 V 13 L18,7 Z"):
                dep?(start?"M 5,7 L9,13 L1,7 L9,1 Z":"M 18,7 L9,13 L14,7 L9,1 Z"):
                "M 18,7 L9,13 L1,7 L9,1 Z";
            out+="<marker id=\""+marker_id(type,start)+"\" refX=\""+num(dep?(start?6:13):(start?18:1))+
                "\" refY=\"7\" markerWidth=\"20\" markerHeight=\"28\" orient=\"auto\" markerUnits=\"userSpaceOnUse\"><path d=\""+path+
                "\" fill=\""+(dep||type==ClassMarker::composition?theme.line:"transparent")+"\" stroke=\""+theme.line+"\" stroke-width=\"1\"/></marker>";
        }
    return out+"</defs>";
}
struct NodeBox { double width,height,header,member_height; bool extra; };
NodeBox node_box(const ClassNodeMeasurements& m,const ClassSvgConfig& c) {
    const double half=std::max(m.label.width,m.annotation.width)/2;
    double right=half,member_height=0,method_height=0;
    for(const auto& t:m.members) {right=std::max(right,t.width);member_height+=t.height;}
    for(const auto& t:m.methods) {right=std::max(right,t.width);method_height+=t.height;}
    const auto p=c.padding,head=m.annotation.height+m.label.height;
    const bool empty=m.members.empty()&&m.methods.empty(),extra=empty&&!c.hide_empty_members;
    double h=head;
    if(!m.members.empty()) h=std::max(h,head+2*p+member_height);
    if(!m.methods.empty()) h=std::max(h,head+std::max(member_height,p/2)+4*p+method_height);
    if(empty) h+=p; else if(m.methods.empty()) h+=2*p;
    h+=2*p+(extra?2*p:empty?-p:0);
    return {half+right+2*p,h,head,member_height,extra};
}
std::string text(const ClassTextBox& t,double x,double y,const ClassSvgTheme& theme,
    unsigned weight=400,std::u16string_view classifier={},double size=0) {
    if(t.text.empty()) return {};
    return "<g class=\"label\"><text x=\""+num(x)+"\" y=\""+num(y+t.baseline)+"\" font-size=\""+
        num(size?size:theme.font_size)+"\" font-weight=\""+std::to_string(weight)+"\""+
        (classifier==u"*"?" font-style=\"italic\"":classifier==u"$"?" text-decoration=\"underline\"":"")+
        " fill=\""+theme.text+"\">"+xml(t.text)+"</text></g>";
}
FlowchartPoint terminal(std::vector<FlowchartPoint> points,bool start) {
    if(!start) std::ranges::reverse(points);
    const auto first=points.front();auto center=points.back();double remaining=35;
    for(std::size_t i=1;i<points.size();++i) {
        const auto a=points[i-1],b=points[i];const double length=std::hypot(b.x-a.x,b.y-a.y);
        if(length>=remaining) {center={a.x+(b.x-a.x)*remaining/length,a.y+(b.y-a.y)*remaining/length};break;}
        remaining-=length;
    }
    const double angle=std::atan2(first.y-center.y,first.x-center.x);
    return {std::sin(angle)*15+(first.x+center.x)/2,-std::cos(angle)*15+(first.y+center.y)/2};
}
}
std::expected<ClassSvg,std::string> render_class_svg(const ClassRenderInput& input,const ClassMeasurements& measured) {
    try {
        const auto& doc=input.document;const auto& theme=input.theme;const auto& config=input.config;
        if(measured.nodes.size()!=doc.classes.size()||measured.edges.size()!=doc.relations.size()||measured.notes.size()!=doc.notes.size()||measured.namespaces.size()!=doc.namespaces.size())
            return std::unexpected("Class needs one measured box per class and relationship");
        // ClassDB inserts note attachments before ordinary relationships.
        std::vector<ClassRelation> relations;
        std::vector<ClassEdgeMeasurements> edge_measurements;
        for(const auto& note:doc.notes)
            if(std::ranges::any_of(doc.classes,[&](const auto& n){return n.id==note.class_id;})) {
                relations.push_back({.from=note.id,.to=note.class_id,.dotted=true});
                edge_measurements.emplace_back();
            }
        const auto attachment_count=relations.size();
        relations.insert(relations.end(),doc.relations.begin(),doc.relations.end());
        edge_measurements.insert(edge_measurements.end(),measured.edges.begin(),measured.edges.end());
        FlowchartInput graph;graph.direction=doc.direction;graph.node_spacing=config.node_spacing;graph.rank_spacing=config.rank_spacing;
        std::vector<NodeBox> boxes;std::vector<FlowchartNodeBox> sizes;std::vector<FlowchartEdgeLabelSize> labels;
        const auto group_count=doc.namespaces.size();
        FlowchartMeasurements group_measurements;
        for(std::size_t i=0;i<group_count;++i) {
            const auto& n=doc.namespaces[i];const auto& m=measured.namespaces[i];
            graph.nodes.push_back({.id=n.id,.label=n.label,.shape=u"rect",.parent=n.parent,.group=true,.padding=config.group_padding});
            sizes.push_back({m.width,m.height});
            group_measurements.nodes.push_back({.width=m.width,.height=m.height,.baseline=m.baseline});
        }
        for(std::size_t i=0;i<doc.classes.size();++i) {
            const auto& n=doc.classes[i];const auto& m=measured.nodes[i];
            if(m.members.size()!=n.members.size()||m.methods.size()!=n.methods.size()) return std::unexpected("Class member measurement count mismatch");
            const auto b=node_box(m,config);boxes.push_back(b);sizes.push_back({b.width,b.height});
            graph.nodes.push_back({.id=n.id,.parent=n.parent});
        }
        for(std::size_t i=0;i<doc.notes.size();++i) {
            const auto& m=measured.notes[i];
            graph.nodes.push_back({.id=doc.notes[i].id,.parent=doc.notes[i].parent});
            sizes.push_back({m.width+2*config.note_padding,m.height+2*config.note_padding});
        }
        for(std::size_t i=0;i<relations.size();++i) {
            const auto& e=relations[i];
            const auto id=std::to_string(i);
            graph.edges.push_back({.id=std::u16string{id.begin(),id.end()},.start=e.from,.end=e.to,.label=e.title,.arrow_end=u"none"});
            labels.push_back({edge_measurements[i].label.width,edge_measurements[i].label.height});
        }
        FlowchartLayout logical;
        std::vector<std::vector<FlowchartPoint>> label_paths(graph.edges.size());
        double graph_min_x=0,graph_min_y=0,graph_max_x=0,graph_max_y=0;
        if(group_count) {
            group_measurements.nodes.resize(graph.nodes.size());
            FlowchartSvgTheme group_theme;
            group_theme.group_title_top=config.group_title_top;group_theme.group_title_bottom=config.group_title_bottom;
            auto scene=layout_measured_flowchart(graph,sizes,labels,group_measurements,group_theme);
            if(!scene) return std::unexpected(scene.error());
            graph_min_x=scene->min_x;graph_min_y=scene->min_y;graph_max_x=scene->max_x;graph_max_y=scene->max_y;
            logical=std::move(scene->layout);
            for(std::size_t i=0;i<logical.edges.size();++i) label_paths[i]=logical.edges[i].points;
        } else {
            auto preparation=prepare_flowchart_graph(graph);if(!preparation) return std::unexpected(preparation.error());
            const auto& prepared=preparation->graph;
            FlowchartInput layout_input;layout_input.direction=graph.direction;
            layout_input.node_spacing=graph.node_spacing;layout_input.rank_spacing=graph.rank_spacing;
            std::vector<FlowchartNodeBox> layout_sizes;std::vector<FlowchartEdgeLabelSize> layout_labels;
            std::unordered_map<std::u16string,std::size_t> ids;
            for(const auto& n:prepared.nodes) {
                ids.emplace(n.data.id,layout_input.nodes.size());layout_input.nodes.push_back(n.data);
                layout_sizes.push_back(n.source_index?sizes[*n.source_index]:FlowchartNodeBox{0.1,0.1});
            }
            for(const auto& e:prepared.edges) {
                layout_input.edges.push_back(e.data);
                layout_labels.push_back(e.loop_segment==0||e.loop_segment==2?FlowchartEdgeLabelSize{}:labels[e.source_index]);
            }
            auto layout=layout_flowchart(layout_input,layout_sizes,layout_labels);if(!layout) return std::unexpected(layout.error());
            logical.width=layout->width;logical.height=layout->height;
            logical.nodes.resize(graph.nodes.size());logical.edges.resize(graph.edges.size());
            for(std::size_t i=0;i<prepared.nodes.size();++i)
                if(auto index=prepared.nodes[i].source_index) logical.nodes[*index]=layout->nodes[i];
            for(std::size_t i=0;i<prepared.edges.size();++i) {
                const auto& e=prepared.edges[i];
                if(e.loop_segment==-1) {
                    logical.edges[e.source_index]=layout->edges[i];label_paths[e.source_index]=layout->edges[i].points;continue;
                }
                if(e.loop_segment!=1) continue;
                const auto& original=graph.edges[e.source_index];
                const auto& node=layout->nodes[ids.at(original.start)];
                const auto& first=layout->nodes[ids.at(e.data.start)];
                const auto& second=layout->nodes[ids.at(e.data.end)];
                auto edge=route_dagre_self_loop(node,{(first.x+second.x)/2,(first.y+second.y)/2},
                    graph.direction,labels[e.source_index],!original.label.empty());
                label_paths[e.source_index]=edge.points;
                // ClassBox uses intersect.rect after the compact loop is merged.
                const auto intersect=[&](FlowchartPoint p) {
                    const auto dx=p.x-node.x,dy=p.y-node.y,w=node.width/2,h=node.height/2;
                    const auto scale=std::abs(dy)*w>std::abs(dx)*h?h/std::abs(dy):w/std::abs(dx);
                    return FlowchartPoint{node.x+dx*scale,node.y+dy*scale};
                };
                edge.points.front()=intersect(edge.points[1]);edge.points.back()=intersect(edge.points[edge.points.size()-2]);
                logical.edges[e.source_index]=std::move(edge);
            }
            graph_max_x=logical.width;graph_max_y=logical.height;
        }
        const auto* placed=&logical;
        // insertTitle uses the pre-title graph bounds; setupViewPortForSVG then
        // includes the SVG text bounds before adding its fixed eight-unit pad.
        const double title_x=(graph_min_x+graph_max_x)/2+8,title_y=-config.title_top_margin;
        double min_x=graph_min_x+8,min_y=graph_min_y+8,max_x=graph_max_x+8,max_y=graph_max_y+8;
        if(!doc.title.empty()) {
            const auto& t=measured.title;
            if(t.text!=doc.title||t.height<=0) return std::unexpected("Class title needs measured SVG text bounds");
            min_x=std::min(min_x,title_x-t.width/2);max_x=std::max(max_x,title_x+t.width/2);
            min_y=std::min(min_y,title_y-t.baseline);max_y=std::max(max_y,title_y+t.height-t.baseline);
        }
        ClassSvg result;result.width=max_x-min_x+16;result.height=max_y-min_y+16;
        std::string svg="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""+num(result.width)+"\" height=\""+num(result.height)+
            "\" viewBox=\""+num(min_x-8)+" "+num(min_y-8)+" "+num(result.width)+" "+num(result.height)+"\" font-family=\""+xml(theme.font_family)+"\">";
        for(auto [tag,value]:{std::pair{"title",doc.acc_title},{"desc",doc.acc_description}})
            if(!value.empty()) svg+="<"+std::string(tag)+">"+xml(value)+"</"+tag+">";
        svg+=markers(theme);
        auto group_theme=theme;group_theme.text=theme.cluster_text;
        // Containers paint before relationships and nodes, outermost first.
        std::vector<std::size_t> groups(group_count);std::iota(groups.begin(),groups.end(),0);
        const auto depth=[&](std::size_t i) {
            std::size_t value=0;auto parent=doc.namespaces[i].parent;
            while(!parent.empty()) {
                const auto found=std::ranges::find(doc.namespaces,parent,&ClassNamespace::id);
                parent=found->parent;++value;
            }
            return value;
        };
        std::stable_sort(groups.begin(),groups.end(),[&](auto a,auto b){return depth(a)<depth(b);});
        for(auto i:groups) {
            const auto& n=placed->nodes[i];const auto& m=measured.namespaces[i];
            const double x=n.x+8-n.width/2,y=n.y+8-n.height/2;
            svg+="<g class=\"cluster\" data-namespace=\""+xml(doc.namespaces[i].id)+"\"><rect x=\""+num(x)+"\" y=\""+num(y)+
                "\" width=\""+num(n.width)+"\" height=\""+num(n.height)+"\" fill=\""+theme.cluster_fill+"\" stroke=\""+theme.cluster_border+"\" stroke-width=\"1\"/>";
            svg+="<g class=\"cluster-label\">"+text(m,n.x+8-m.width/2,y+config.group_title_top,group_theme)+"</g></g>";
        }
        for(std::size_t i=0;i<relations.size();++i) {
            const auto& e=relations[i];auto points=placed->edges[i].points;
            for(auto& p:points) {p.x+=8;p.y+=8;}
            if(points.size()<2) return std::unexpected("Class relation needs a routed path");
            shorten(points.front(),points[1],e.start);shorten(points.back(),points[points.size()-2],e.end);
            const auto path=curve(points);
            // insertEdge returns the original compact route unless its literal
            // rounded-coordinate lookup selects the clipped path. Symmetric
            // rectangle clipping retains the self-loop main-label midpoint.
            if(!label_coordinate_in_path(points[points.size()/2],path)) label_paths[i]=placed->edges[i].points;
            svg+="<path class=\"relation\" d=\""+path+"\" fill=\"none\" stroke=\""+theme.line+"\" stroke-width=\""+num(theme.stroke_width)+"\"";
            svg+=i<attachment_count?" stroke-dasharray=\"2\"":e.dotted?" stroke-dasharray=\"3\"":" stroke-dasharray=\"0\"";
            if(e.start!=ClassMarker::none) svg+=" marker-start=\"url(#"+marker_id(e.start,true)+")\"";
            if(e.end!=ClassMarker::none) svg+=" marker-end=\"url(#"+marker_id(e.end,false)+")\"";
            svg+="/>";
        }
        svg+="<g class=\"edgeLabels\">";
        for(std::size_t i=0;i<relations.size();++i) {
            const auto& m=edge_measurements[i];const auto& route=placed->edges[i];
            if(route.label&&!m.label.text.empty()) {
                const auto x=route.label->x+8-m.label.width/2,y=route.label->y+8-m.label.height/2;
                svg+="<rect x=\""+num(x)+"\" y=\""+num(y)+"\" width=\""+num(m.label.width)+"\" height=\""+num(m.label.height)+"\" fill=\""+theme.label_background+"\"/>";
                svg+=text(m.label,x,y,theme);
            }
            for(bool start:{true,false}) {
                const auto& t=start?m.start:m.end;if(t.text.empty()) continue;
                const auto p=terminal(label_paths[i],start);
                svg+=text(t,p.x+8-t.width/2,p.y+8-t.height/2,theme,400,{},11);
            }
        }
        svg+="</g>";
        for(std::size_t i=0;i<doc.classes.size();++i) {
            const auto& n=doc.classes[i];const auto& m=measured.nodes[i];const auto& b=boxes[i];const auto& position=placed->nodes[group_count+i];
            const auto p=config.padding,x=position.x+8-b.width/2,y=position.y+8-b.height/2;
            svg+="<g class=\"node\" data-class-id=\""+xml(n.id)+"\"><rect class=\"outer-path\" x=\""+num(x)+"\" y=\""+num(y)+
                "\" width=\""+num(b.width)+"\" height=\""+num(b.height)+"\" fill=\""+theme.fill+"\" stroke=\""+theme.border+"\" stroke-width=\""+num(theme.stroke_width)+"\"/>";
            const auto group=[&](std::string_view kind,const std::string& content){svg+="<g class=\"text "+std::string(kind)+"-group\">"+content+"</g>";};
            group("annotation",text(m.annotation,position.x+8-m.annotation.width/2,y+p,theme));
            group("label",text(m.label,position.x+8-m.label.width/2,y+p+m.annotation.height,theme,700));
            std::string content;double row_y=y+p+b.header+2*p;
            for(std::size_t j=0;j<m.members.size();++j) {content+=text(m.members[j],x+p,row_y,theme,400,n.members[j].classifier);row_y+=m.members[j].height;}
            group("members",content);content.clear();row_y=y+p+b.header+std::max(b.member_height,p/2)+4*p;
            for(std::size_t j=0;j<m.methods.size();++j) {content+=text(m.methods[j],x+p,row_y,theme,400,n.methods[j].classifier);row_y+=m.methods[j].height;}
            group("methods",content);
            if(b.extra||!m.members.empty()||!m.methods.empty()) {
                // Empty group getBBox heights are reduced by PADDING/2 in the
                // original extra compartment branch, including the empty member group.
                const double first=y+2*p+b.header,second=first+b.member_height+2*p-(b.extra?p/2:0);
                for(double line_y:{first,second}) svg+="<path class=\"divider\" d=\"M"+num(x)+","+num(line_y)+"L"+num(x+b.width)+","+num(line_y+0.001)+
                    "\" stroke=\""+theme.border+"\" stroke-width=\"1\" fill=\"none\"/>";
            }
            svg+="</g>";
        }
        auto note_theme=theme;note_theme.text=theme.note_text;
        for(std::size_t i=0;i<doc.notes.size();++i) {
            const auto& m=measured.notes[i];const auto& position=placed->nodes[group_count+doc.classes.size()+i];
            const auto w=m.width+2*config.note_padding,h=m.height+2*config.note_padding;
            const auto x=position.x+8-w/2,y=position.y+8-h/2;
            svg+="<g class=\"node\" data-class-id=\""+xml(doc.notes[i].id)+"\"><rect class=\"outer-path\" x=\""+num(x)+"\" y=\""+num(y)+
                "\" width=\""+num(w)+"\" height=\""+num(h)+"\" fill=\""+theme.note_fill+"\" stroke=\""+theme.note_border+"\" stroke-width=\""+num(theme.stroke_width)+"\"/>";
            svg+="<g class=\"noteLabel\">";double row_y=y+config.note_padding;
            for(const auto& line:m.lines) {svg+=text(line,x+config.note_padding,row_y,note_theme);row_y+=line.height;}
            svg+="</g></g>";
        }
        if(!doc.title.empty()) svg+="<text class=\"classDiagramTitleText\" text-anchor=\"middle\" x=\""+num(title_x)+
            "\" y=\""+num(title_y)+"\" font-size=\""+num(theme.font_size)+"\" fill=\""+theme.title_color+"\">"+xml(doc.title)+"</text>";
        result.svg=svg+"</svg>";return result;
    } catch(const std::exception& e) {return std::unexpected(std::string{e.what()});}
}
}
