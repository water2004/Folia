module folia.mermaid.architecture_svg;
import folia.mermaid.architecture_icons;
import folia.mermaid.svg_text;
import folia.mermaid.number;
import std;
namespace folia::mermaid {
namespace {
std::string num(double x){auto value=format_number(x);if(!value)throw std::runtime_error("Invalid Architecture coordinate");return *value;}
std::string xml(std::u16string_view text){const auto value=escape_svg_text(text,u"Architecture SVG");if(!value)throw std::runtime_error("Invalid Architecture SVG text");std::string result;for(const auto c:*value)if(c=='"')result+="&quot;";else result+=c;return result;}
const ConfigValue& field(const ConfigValue& v,std::u16string_view key){const auto* value=v.find(key);if(!value)throw std::runtime_error("Missing Architecture configuration");return *value;}
double numeric(const ConfigValue& v){if(v.kind==ConfigKind::number)return v.number;const std::string s(v.string.begin(),v.string.end());double value=0;const auto result=std::from_chars(s.data(),s.data()+s.size(),value);if(result.ec!=std::errc{})throw std::runtime_error("Invalid Architecture numeric style");return value;}
struct Point {double x=0,y=0;};
struct Bounds {
    double l=std::numeric_limits<double>::infinity(),t=l,r=-l,b=-l;
    void add(double x,double y,double w,double h){l=std::min(l,x);t=std::min(t,y);r=std::max(r,x+w);b=std::max(b,y+h);}
    void point(Point p){add(p.x,p.y,0,0);}
};
std::string point(Point p){return num(p.x)+","+num(p.y);}
bool horizontal(char16_t direction){return direction==u'L'||direction==u'R';}
Point endpoint(const FCoseNode& n,char16_t direction,double size){switch(direction){case u'L':return {n.x,n.y+size/2};case u'R':return {n.x+size,n.y+size/2};case u'T':return {n.x+size/2,n.y};default:return {n.x+size/2,n.y+size};}}
std::string icon(std::u16string_view name,double size){const auto body=architecture_icon(name);if(!body)throw std::runtime_error("Architecture external icon packs are not connected");return "<svg width=\""+num(size)+"\" height=\""+num(size)+"\" viewBox=\"0 0 80 80\">"+std::string(*body)+"</svg>";}
std::string text(const FlowchartTextBox& box,const ArchitectureRenderInput& input,Point p,bool start,bool middle,double rotation=0) {
    const auto font=box.font.value_or(FlowchartLabelFont{input.theme.font_family,input.theme.font_size});
    std::string out="<g transform=\"translate("+point(p)+") rotate("+num(rotation)+")\"><text class=\"architecture-label\" xml:space=\"preserve\" font-family=\""+xml(font.family)+"\" font-size=\""+num(font.size)+"\" text-anchor=\""+(start?"start":"middle")+"\" dominant-baseline=\""+(middle?"middle":"auto")+"\" fill=\""+xml(input.text_color)+"\">";
    for(std::size_t i=0;i<box.lines.size();++i){out+="<tspan x=\"0\" y=\""+num(box.baseline+i*box.line_step)+"\">";if(box.runs.empty())out+=xml(box.lines[i]);else for(const auto& run:box.runs[i])out+="<tspan font-weight=\""+std::string(run.strong?"700":"400")+"\" font-style=\""+(run.emphasis?"italic":"normal")+"\">"+xml(run.text)+"</tspan>";out+="</tspan>";}
    return out+"</text></g>";
}
void text_bounds(Bounds& bounds,const FlowchartTextBox& box,Point p,bool start,bool middle,const ArchitectureMeasurements& measured,double rotation=0) {
    if(box.lines.empty())return;const double x=start?0:-box.width/2,y=box.baseline-measured.svg_ascent+(middle?measured.svg_middle:0);
    const double radians=rotation*std::numbers::pi/180,c=std::cos(radians),s=std::sin(radians);
    for(const auto point:std::array<Point,4>{{{x,y},{x+box.width,y},{x,y+box.height},{x+box.width,y+box.height}}})bounds.point({p.x+c*point.x-s*point.y,p.y+s*point.x+c*point.y});
}
std::string arrow(Point p,char16_t direction,double size,const ArchitectureRenderInput& input,Bounds& bounds) {
    const double half=size/2;std::array<Point,3> vertices;
    switch(direction){case u'L':vertices={Point{size,half},{0,size},{0,0}};p.x-=size-2;p.y-=half;break;case u'R':vertices={Point{0,half},{size,0},{size,size}};p.x-=2;p.y-=half;break;case u'T':vertices={Point{0,0},{size,0},{half,size}};p.x-=half;p.y-=size-2;break;default:vertices={Point{half,0},{size,size},{0,size}};p.x-=half;p.y-=2;}
    std::string value;for(const auto v:vertices){if(!value.empty())value+=' ';value+=point(v);bounds.point({p.x+v.x,p.y+v.y});}
    return "<polygon class=\"arrow\" fill=\""+xml(input.arrow_color)+"\" points=\""+value+"\" transform=\"translate("+point(p)+")\"/>";
}
}
std::expected<ArchitectureRenderInput,std::string> prepare_architecture_render(std::u16string_view source,const ConfigSession& session,FrontendLimits limits) {
    auto doc=prepare_architecture(source,session,limits);if(!doc)return std::unexpected(doc.error());
    try{ArchitectureRenderInput out;const auto& theme=field(doc->effective_config,u"themeVariables");out.theme.font_family=field(theme,u"fontFamily").string;out.theme.font_size=numeric(field(theme,u"fontSize"));out.theme.html_label_spacing=false;out.edge_color=field(theme,u"archEdgeColor").string;out.arrow_color=field(theme,u"archEdgeArrowColor").string;out.group_color=field(theme,u"archGroupBorderColor").string;out.text_color=field(theme,u"textColor").string;out.edge_width=numeric(field(theme,u"archEdgeWidth"));out.group_width=numeric(field(theme,u"archGroupBorderWidth"));out.document=std::move(*doc);return out;}catch(const std::exception& e){return std::unexpected(e.what());}
}
std::expected<ArchitectureSvg,std::string> render_architecture_svg(const ArchitectureRenderInput& input,const ArchitectureMeasurements& measured) {
    try {
        const auto& doc=input.document;if(measured.services.size()!=doc.nodes.size()||measured.icon_text.size()!=doc.nodes.size()||!measured.measure_label)return std::unexpected("Architecture needs measured labels");
        const auto layout=layout_architecture(doc,&measured.cy_labels);if(!layout)return std::unexpected(layout.error());
        const double size=numeric(field(doc.config,u"iconSize")),padding=numeric(field(doc.config,u"padding")),font_size=numeric(field(doc.config,u"fontSize"));
        std::map<std::u16string,std::size_t> ids;for(std::size_t i=0;i<layout->nodes.size();++i)ids.emplace(layout->nodes[i].id,i);
        std::map<std::u16string,const ArchitectureNode*> services;for(const auto& node:doc.nodes)services.emplace(node.id,&node);
        Bounds bounds;std::string svg="<g class=\"architecture-edges\">";
        for(const auto& e:doc.edges) {
            const auto& a=layout->nodes[ids.at(e.lhs_id)];const auto& b=layout->nodes[ids.at(e.rhs_id)];Point start=endpoint(a,e.lhs_dir,size),end=endpoint(b,e.rhs_dir,size);
            const bool xy=horizontal(e.lhs_dir)!=horizontal(e.rhs_dir);Point mid=xy?(horizontal(e.lhs_dir)?Point{end.x,start.y}:Point{start.x,end.y}):Point{(start.x+end.x)/2,(start.y+end.y)/2};
            const auto adjust=[&](Point& p,char16_t direction,bool group,bool junction){double delta=0;if(group)delta=padding+4+(direction==u'B'?18:0);else if(junction)delta=-size/2;if(direction==u'L')p.x-=delta;else if(direction==u'R')p.x+=delta;else if(direction==u'T')p.y-=delta;else p.y+=delta;};
            adjust(start,e.lhs_dir,e.lhs_group,services.at(e.lhs_id)->type==u"junction");adjust(end,e.rhs_dir,e.rhs_group,services.at(e.rhs_id)->type==u"junction");
            svg+="<g><path class=\"edge\" fill=\"none\" stroke=\""+xml(input.edge_color)+"\" stroke-width=\""+num(input.edge_width)+"\" d=\"M "+point(start)+" L "+point(mid)+" L "+point(end)+"\"/>";bounds.point(start);bounds.point(mid);bounds.point(end);
            if(e.lhs_into)svg+=arrow(start,e.lhs_dir,size/6,input,bounds);if(e.rhs_into)svg+=arrow(end,e.rhs_dir,size/6,input,bounds);
            if(!e.title.empty()){const double width=xy?std::abs(start.x-end.x)/2:horizontal(e.lhs_dir)?std::abs(start.x-end.x):std::abs(start.y-end.y)/1.5;auto label=measured.measure_label(e.title,width);if(!label)return std::unexpected(label.error());double rotation=horizontal(e.lhs_dir)?0:-90;Point position=mid;
                if(xy) {
                    const double x=e.lhs_dir==u'L'||e.rhs_dir==u'L'?1:-1,y=e.lhs_dir==u'T'||e.rhs_dir==u'T'?1:-1;
                    rotation=-x*y*45;const double diagonal=(label->width+label->height)/std::sqrt(2.0),angle=rotation*std::numbers::pi/180;
                    position.x+=x*diagonal/2+std::sin(angle)*label->height/2;
                    position.y+=y*diagonal/2-std::cos(angle)*label->height/2;
                }
                svg+=text(*label,input,position,false,!xy,rotation);text_bounds(bounds,*label,position,false,!xy,measured,rotation);}
            svg+="</g>";
        }
        svg+="</g><g class=\"architecture-services\">";
        for(std::size_t i=0;i<doc.nodes.size();++i) {
            const auto& source=doc.nodes[i];const auto& n=layout->nodes[ids.at(source.id)];const bool junction=source.type==u"junction";
            svg+="<g id=\"service-"+xml(source.id)+"\" class=\""+(junction?"architecture-junction":"architecture-service")+"\" transform=\"translate("+point({n.x,n.y})+")\">";bounds.add(n.x,n.y,size,size);
            if(junction)svg+="<rect fill-opacity=\"0\" width=\""+num(size)+"\" height=\""+num(size)+"\"/>";
            else {
                if(!source.title.empty()){const Point p{size/2,size};svg+=text(measured.services[i],input,p,false,true);text_bounds(bounds,measured.services[i],{n.x+p.x,n.y+p.y},false,true,measured);}
                if(!source.icon.empty())svg+=icon(source.icon,size);
                else if(!source.icon_text.empty()){svg+=icon(u"blank",size);auto paint=input;paint.text_color=u"#fff";const auto& label=measured.icon_text[i];const Point p{1,(size-label.height)/2+measured.svg_ascent-label.baseline};auto icon_label=text(label,paint,p,true,false);const auto marker=icon_label.find("architecture-label");icon_label.replace(marker,18,"architecture-icon-text");svg+=icon_label;}
                else svg+="<path class=\"node-bkg\" fill=\"none\" stroke=\""+xml(input.group_color)+"\" stroke-width=\""+num(input.group_width)+"\" stroke-dasharray=\"8\" d=\"M0,"+num(size)+" V5 Q0,0 5,0 H"+num(size-5)+" Q"+num(size)+",0 "+num(size)+",5 V"+num(size)+" Z\"/>";
            }
            svg+="</g>";
        }
        svg+="</g><g class=\"architecture-groups\">";
        for(const auto& group:doc.groups) {
            const auto& n=layout->nodes[ids.at(group.id)];const double w=n.width+2*padding+3,h=n.height+2*padding+3,x=n.x-w/2+size/2,y=n.y-h/2+size/2;
            bounds.add(x,y,w,h);svg+="<rect class=\"node-bkg\" x=\""+num(x)+"\" y=\""+num(y)+"\" width=\""+num(w)+"\" height=\""+num(h)+"\" fill=\"none\" stroke=\""+xml(input.group_color)+"\" stroke-width=\""+num(input.group_width)+"\" stroke-dasharray=\"8\"/>";
            double label_x=x+4,label_y=y+2;
            if(!group.icon.empty()){const auto icon_size=.75*padding;svg+="<g transform=\"translate("+point({x+1,y+1})+")\">"+icon(group.icon,icon_size)+"</g>";bounds.add(x+1,y+1,icon_size,icon_size);label_x+=icon_size;label_y+=font_size/2-3;}
            if(!group.title.empty()){auto label=measured.measure_label(group.title,w);if(!label)return std::unexpected(label.error());svg+=text(*label,input,{label_x,label_y},true,false);text_bounds(bounds,*label,{label_x,label_y},true,false,measured);}
        }
        svg+="</g>";const double width=bounds.r-bounds.l+2*padding,height=bounds.b-bounds.t+2*padding;
        return ArchitectureSvg{"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""+num(width)+"\" height=\""+num(height)+"\" viewBox=\""+num(bounds.l-padding)+" "+num(bounds.t-padding)+" "+num(width)+" "+num(height)+"\">"+svg+"</svg>",width,height};
    }catch(const std::exception& e){return std::unexpected(e.what());}
}
}
