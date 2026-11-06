module folia.mermaid.state_svg;
import std;
import folia.mermaid.number;
import folia.mermaid.svg_text;
import folia.mermaid.rough_rectangle;
namespace folia::mermaid {
namespace {
const ConfigValue& member(const ConfigValue& v,std::u16string_view key){auto p=v.find(key);if(!p)throw std::runtime_error("Missing State configuration");return *p;}
std::u16string text(const ConfigValue& v){if(v.kind!=ConfigKind::string)throw std::runtime_error("Expected State style");return v.string;}
double numeric(const ConfigValue& v){if(v.kind==ConfigKind::number)return v.number;auto s=text(v);std::string raw(s.begin(),s.end());double n=0;auto p=std::from_chars(raw.data(),raw.data()+raw.size(),n);if(p.ec!=std::errc{})throw std::runtime_error("Invalid State dimension");return n;}
std::string num(double value){auto s=format_number(value);if(!s)throw std::runtime_error("Invalid State geometry");return *s;}
std::string xml(std::u16string_view value){auto s=escape_svg_text(value,u"State text");if(!s)throw std::runtime_error("Invalid State text");std::string out;for(auto c:*s)out+=c=='"'?"&quot;":std::string(1,c);return out;}
std::string point(FlowchartPoint p){return num(p.x)+","+num(p.y);}
FlowchartPoint mix(FlowchartPoint a,FlowchartPoint b,double x,double y,double divisor){return{(a.x*x+b.x*y)/divisor,(a.y*x+b.y*y)/divisor};}
// d3-shape curveBasis, shared unified-renderer path semantics.
std::string curve(std::span<const FlowchartPoint> p){if(p.empty())return{};std::string s="M"+point(p[0]);if(p.size()<3){for(std::size_t i=1;i<p.size();++i)s+="L"+point(p[i]);return s;}s+="L"+point(mix(p[0],p[1],5,1,6));auto segment=[&](auto a,auto b,auto c){s+="C"+point(mix(a,b,2,1,3))+" "+point(mix(a,b,1,2,3))+" "+point({(a.x+4*b.x+c.x)/6,(a.y+4*b.y+c.y)/6});};for(std::size_t i=2;i<p.size();++i)segment(p[i-2],p[i-1],p[i]);segment(p[p.size()-2],p.back(),p.back());return s+"L"+point(p.back());}
// The original roughjs zero-roughness circle uses a 36-step Catmull-Rom
// spline and a closing duplicate sample, including its small closing overshoot.
std::string end_circle(double radius){std::vector<FlowchartPoint> p;for(int i=-1;i<=36;++i){auto a=i*std::numbers::pi/18;p.push_back({radius*std::cos(a),radius*std::sin(a)});}p.push_back({radius,0});p.push_back({radius*std::cos(std::numbers::pi/18),radius*std::sin(std::numbers::pi/18)});std::string out="M"+point(p[1]);for(std::size_t i=1;i+2<p.size();++i)out+="C"+point({p[i].x+(p[i+1].x-p[i-1].x)/6,p[i].y+(p[i+1].y-p[i-1].y)/6})+" "+point({p[i+1].x-(p[i+2].x-p[i].x)/6,p[i+1].y-(p[i+2].y-p[i].y)/6})+" "+point(p[i+1]);return out;}
std::u16string paint(const FlowchartNode& node,std::u16string_view key,std::u16string fallback){for(const auto& style:node.styles){auto colon=style.find(u':');if(colon!=style.npos&&std::u16string_view(style).substr(0,colon)==key)return style.substr(colon+1);}return fallback;}
void add_annotations(FlowchartInput& graph,std::span<const StateAnnotation> annotations,double note_padding){
 for(std::size_t i=0;i<annotations.size();++i){
  const auto& annotation=annotations[i];
  if(std::ranges::find(graph.nodes,annotation.state,&FlowchartNode::id)==graph.nodes.end())
   throw std::runtime_error("State note references an unknown state");
  const auto text_id=std::to_string(i+1);
  const std::u16string suffix(text_id.begin(),text_id.end());
  FlowchartNode group;group.id=annotation.state+u"----parent-"+suffix;
  group.shape=u"noteGroup";group.group=true;group.padding=16;
  FlowchartNode note;note.id=annotation.state+u"----note-"+suffix;
  note.label=annotation.text;note.shape=u"note";note.padding=note_padding;
  note.parent=group.id;note.label_type=u"markdown";
  const auto note_id=note.id;
  graph.nodes.push_back(std::move(group));graph.nodes.push_back(std::move(note));
  FlowchartEdge edge;edge.id=u"state-note-edge-"+suffix;
  edge.start=annotation.position==u"left"?note_id:annotation.state;
  edge.end=annotation.position==u"left"?annotation.state:note_id;
  edge.stroke=u"dotted";edge.curve=u"linear";graph.edges.push_back(std::move(edge));
 }
}
}
std::expected<StateRenderInput,std::string> prepare_state(
    std::u16string_view source, const ConfigSession& session) {
    auto prepared = prepare_diagram(source, session);
    if (!prepared) return std::unexpected(prepared.error());

    auto parsed = parse_state_document(prepared->code.cleaned);
    if (!parsed) return std::unexpected(parsed.error());

    try {
        if (text(member(prepared->effective_config, u"look")) != u"classic" ||
            text(member(prepared->effective_config, u"layout")) != u"dagre") {
            return std::unexpected("State native renderer currently requires classic Dagre");
        }

        StateRenderInput result;
        result.document = std::move(parsed->document);
        result.annotations = std::move(parsed->annotations);
        result.document.effective_config = prepared->effective_config;
        if (prepared->title) result.document.title = *prepared->title;

        const auto& config = member(prepared->effective_config, u"state");
        const auto dimension = [&](std::u16string_view key, double fallback) {
            const auto value = config.find(key);
            return value ? numeric(*value) : fallback;
        };
        result.document.node_spacing = dimension(u"nodeSpacing", 50);
        result.document.rank_spacing = dimension(u"rankSpacing", 50);
        result.minimum_width = dimension(u"minNodeWidth", 120);
        result.wrapping_width = dimension(u"wrappingWidth", 120);
        result.title_margin = dimension(u"titleTopMargin", 25);
        result.note_margin = dimension(u"noteMargin", 10);

        if (const auto flowchart = prepared->effective_config.find(u"flowchart")) {
            if (const auto padding = flowchart->find(u"padding")) {
                result.note_node_padding = numeric(*padding);
            }
        }

        auto& theme = result.theme;
        const auto& variables = member(prepared->effective_config, u"themeVariables");
        theme.font_family = text(member(prepared->effective_config, u"fontFamily"));
        theme.font_size = numeric(member(variables, u"fontSize"));
        const auto color = [&](std::u16string_view name, std::u16string_view fallback) {
            const auto value = variables.find(name);
            return value && value->kind == ConfigKind::string && !value->string.empty()
                ? value->string
                : text(member(variables, fallback));
        };
        theme.fill = color(u"stateBkg", u"mainBkg");
        theme.border = color(u"stateBorder", u"nodeBorder");
        theme.text = text(member(variables, u"stateLabelColor"));
        theme.edge_text = text(member(variables, u"textColor"));
        theme.edge_background = text(member(variables, u"edgeLabelBackground"));
        theme.line = text(member(variables, u"transitionColor"));
        theme.special = text(member(variables, u"specialStateColor"));
        theme.composite_title = text(member(variables, u"compositeTitleBackground"));
        theme.composite = color(u"compositeBackground", u"background");
        theme.alternate = color(u"altBackground", u"background");
        theme.note_fill = color(u"noteBkgColor", u"noteBkgColor");
        theme.note_border = color(u"noteBorderColor", u"noteBorderColor");
        theme.note_text = color(u"noteTextColor", u"noteTextColor");
        return result;
    } catch (const std::runtime_error& error) {
        return std::unexpected(error.what());
    }
}
std::expected<StateSvg,std::string> render_state_svg(const StateRenderInput& in,const StateMeasure& measure){try{if(!measure)throw std::runtime_error("State requires text measurements");const auto& d=in.document;const auto& t=in.theme;auto metric=[&](std::u16string_view value,double size){auto b=measure(value,size);if(!b)throw std::runtime_error(b.error());return *b;};
 auto label=[&](std::u16string_view value,bool group=false,bool note=false){
  FlowchartTextBox box;if(value.empty())return box;
  std::vector<std::u16string_view> paragraphs;
  if(note){std::size_t begin=0;for(;;){const auto end=value.find(u'\n',begin);paragraphs.push_back(value.substr(begin,end==value.npos?value.size()-begin:end-begin));if(end==value.npos)break;begin=end+1;}}
  else paragraphs.push_back(value);
  for(const auto paragraph:paragraphs){
   std::u16string line;std::size_t begin=0;
   while(begin<paragraph.size()){
    auto end=paragraph.find(u' ',begin);if(end==paragraph.npos)end=paragraph.size();
    const auto word=paragraph.substr(begin,end-begin);
    auto candidate=line.empty()?std::u16string(word):line+u" "+std::u16string(word);
    const auto measured=metric(candidate,t.font_size);
    if(!group&&!line.empty()&&(note?measured.width:measured.advance)>in.wrapping_width){box.lines.push_back(std::move(line));line=word;}
    else line=std::move(candidate);
    begin=end==paragraph.size()?end:end+1;
   }
   if(!line.empty())box.lines.push_back(std::move(line));
  }
  const auto m=metric(value,t.font_size);box.line_step=t.font_size*1.5;
  box.height=box.line_step*box.lines.size();box.baseline=(box.line_step-m.height)/2+m.baseline;
  for(const auto& item:box.lines)box.width=std::max(box.width,metric(item,t.font_size).advance);
  return box;
 };
 auto graph=d;
 add_annotations(graph,in.annotations,in.note_node_padding);
 FlowchartMeasurements labels;
 std::vector<FlowchartNodeBox> boxes;
 std::vector<FlowchartEdgeLabelSize> edge_boxes;
 for(auto& node:graph.nodes){
  const bool terminal=node.shape==u"stateStart"||node.shape==u"stateEnd";
  auto box=terminal?FlowchartTextBox{}:label(node.label,node.group,node.shape==u"note");
  labels.nodes.push_back(box);
  if(terminal){
   const auto width=node.shape==u"stateEnd"?14+7*(1-std::cos(std::numbers::pi/18))/8:14;
   boxes.push_back({width,14});node.shape=u"circle";
  }else if(node.shape==u"note"){
   boxes.push_back({std::max(in.minimum_width,box.width)+2*node.padding,box.height+2*node.padding});
  }else if(node.shape==u"divider"){
   boxes.push_back({2,2});
  }else{
   boxes.push_back({node.group?box.width:std::max(in.minimum_width,box.width)+16,
                    node.group?box.height:box.height+16});
   if(!node.group)node.shape=u"squareRect";
  }
 }
 for(const auto& edge:graph.edges){auto b=label(edge.label);labels.edges.push_back(b);edge_boxes.push_back({b.width,b.height});}
 FlowchartSvgTheme layout_theme;layout_theme.font_family=t.font_family;layout_theme.font_size=t.font_size;layout_theme.padding=8;layout_theme.group_title_top=0;layout_theme.group_title_bottom=0;auto scene=layout_measured_flowchart(graph,boxes,edge_boxes,labels,layout_theme);if(!scene)return std::unexpected(scene.error());
 std::string body;auto rect=[&](double x,double y,double w,double h,double radius,std::u16string_view fill,std::u16string_view stroke){return "<rect x=\""+num(x)+"\" y=\""+num(y)+"\" width=\""+num(w)+"\" height=\""+num(h)+"\" rx=\""+num(radius)+"\" ry=\""+num(radius)+"\" fill=\""+xml(fill)+"\" stroke=\""+xml(stroke)+"\" stroke-width=\"1\"/>";};
 auto draw_label=[&](const FlowchartTextBox& box,double x,double y,std::u16string_view color,std::string_view css){if(box.lines.empty())return std::string{};std::string out="<text class=\""+std::string(css)+"\" data-html-label=\"true\" fill=\""+xml(color)+"\">";for(std::size_t i=0;i<box.lines.size();++i)out+="<tspan x=\""+num(x)+"\" y=\""+num(y+box.baseline+i*box.line_step)+"\" text-anchor=\"middle\">"+xml(box.lines[i])+"</tspan>";return out+"</text>";};
 for(std::size_t i=0;i<graph.nodes.size();++i){
  const auto& source=i<d.nodes.size()?d.nodes[i]:graph.nodes[i];
  if(!source.group)continue;
  const auto& node=scene->layout.nodes[i];
  if(source.shape==u"noteGroup"){
   body+="<g class=\"note-cluster\" id=\""+xml(source.id)+"\"><rect x=\""+
       num(node.x-node.width/2)+"\" y=\""+num(node.y-node.height/2)+"\" width=\""+
       num(node.width)+"\" height=\""+num(node.height)+"\" fill=\"none\" stroke=\"none\"/></g>";
   continue;
  }
  const bool divider=source.shape==u"divider";
  body+="<g class=\"state-group"+std::string(divider?" state-divider":"")+
      "\" data-id=\""+xml(source.id)+"\">";
  if(divider){
   body+="<rect class=\"divider\" x=\""+num(node.x-node.width/2)+"\" y=\""+
       num(node.y-node.height/2)+"\" width=\""+num(node.width)+"\" height=\""+
       num(node.height)+"\" fill=\""+xml(t.alternate)+"\" stroke=\""+xml(t.border)+
       "\" stroke-width=\"1\" stroke-dasharray=\"10,10\"/>";
  }else{
   body+=rect(node.x-node.width/2,node.y-node.height/2,node.width,node.height,5,t.composite_title,t.border);
   body+=rect(node.x-node.width/2,node.y-node.height/2+labels.nodes[i].height+2,
       node.width,node.height-labels.nodes[i].height-6,0,
       source.parent.empty()?t.composite:t.alternate,t.border);
   body+=draw_label(labels.nodes[i],node.x,node.y-node.height/2+1,t.text,"cluster-label");
  }
  body+="</g>";
 }
 for(std::size_t i=0;i<graph.edges.size();++i){
  const auto& edge=scene->layout.edges[i];
  const bool note_edge=i>=d.edges.size();
  body+="<path class=\"state-edge"+std::string(note_edge?" note-edge":"")+
      "\" d=\""+curve(edge.points)+"\" fill=\"none\" stroke=\""+xml(t.line)+
      "\" stroke-width=\"1\" stroke-dasharray=\""+
      std::string(note_edge?"5":"0")+"\""+
      (note_edge?"":" marker-end=\"url(#state-barb)\"")+"/>";
  if(edge.label&&!graph.edges[i].label.empty()){
   const auto& box=labels.edges[i];
   body+="<rect class=\"state-label-background\" x=\""+
       num(edge.label->x-box.width/2)+"\" y=\""+num(edge.label->y-box.height/2)+
       "\" width=\""+num(box.width)+"\" height=\""+num(box.height)+
       "\" fill=\""+xml(t.edge_background)+"\"/>";
   body+=draw_label(box,edge.label->x,edge.label->y-box.height/2,t.edge_text,"edge-label");
  }
 }
 for(std::size_t i=0;i<graph.nodes.size();++i){
  const auto& source=i<d.nodes.size()?d.nodes[i]:graph.nodes[i];
  if(source.group)continue;
  const auto& node=scene->layout.nodes[i];
  const auto note=source.shape==u"note";
  body+="<g class=\"state-node"+std::string(note?" state-note statediagram-note":"")+
      "\" data-id=\""+xml(source.id)+"\">";
  if(source.shape==u"stateStart"){
   body+="<circle cx=\""+num(node.x)+"\" cy=\""+num(node.y)+"\" r=\"7\" fill=\""+
       xml(t.special)+"\" stroke=\""+xml(t.special)+"\"/>";
  }else if(source.shape==u"stateEnd"){
   body+="<g transform=\"translate("+num(node.x)+","+num(node.y)+")\">";
   for(auto radius:{7.0,2.5}){
    const auto fill=radius==7?t.fill:t.border;
    body+="<path d=\""+end_circle(radius)+"\" fill=\""+xml(fill)+
        "\" stroke=\"none\" stroke-width=\"0\"/><path d=\""+end_circle(radius)+
        "\" fill=\"none\" stroke=\""+xml(radius==7?t.line:t.border)+
        "\" stroke-width=\"2\" stroke-dasharray=\"0 0\"/>";
   }
   body+="</g>";
  }else if(note){
   const auto paths=rough_rectangle_paths(node.x-node.width/2,node.y-node.height/2,node.width,node.height);
   body+="<path class=\"outer-path\" d=\""+paths.fill_path+"\" fill=\""+xml(t.note_fill)+
       "\" stroke=\"none\" stroke-width=\"0\"/><path d=\""+paths.stroke_path+
       "\" fill=\"none\" stroke=\""+xml(t.note_border)+"\" stroke-width=\"1.3\" stroke-dasharray=\"0 0\"/>";
   body+=draw_label(labels.nodes[i],node.x,node.y-labels.nodes[i].height/2,t.note_text,"noteText");
  }else if(source.shape==u"divider"){
   body+="<path class=\"state-divider\" d=\"M"+num(node.x-node.width/2)+","+num(node.y)+
       " L"+num(node.x+node.width/2)+","+num(node.y)+"\" stroke=\""+xml(t.border)+"\"/>";
  }else{
   body+=rect(node.x-node.width/2,node.y-node.height/2,node.width,node.height,5,
       paint(source,u"fill",t.fill),paint(source,u"stroke",t.border));
   body+=draw_label(labels.nodes[i],node.x,node.y-labels.nodes[i].height/2,
       paint(source,u"color",t.text),"node-label");
  }
  body+="</g>";
 }
 double min_y=scene->min_y,max_y=scene->max_y;double min_x=scene->min_x,max_x=scene->max_x;if(!d.title.empty()){auto m=metric(d.title,18);auto y=-in.title_margin-8;body+="<text class=\"state-title\" x=\""+num((scene->min_x+scene->max_x)/2)+"\" y=\""+num(y)+"\" font-size=\"18\" text-anchor=\"middle\" fill=\""+xml(t.edge_text)+"\">"+xml(d.title)+"</text>";min_y=std::min(min_y,y-m.baseline);min_x=std::min(min_x,(scene->min_x+scene->max_x-m.width)/2);max_x=std::max(max_x,(scene->min_x+scene->max_x+m.width)/2);}const double height=max_y-min_y+16,width=max_x-min_x+16;return StateSvg{"<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""+num(width)+"\" height=\""+num(height)+"\" viewBox=\""+num(min_x)+" "+num(min_y)+" "+num(width)+" "+num(height)+"\" font-family=\""+xml(t.font_family)+"\" font-size=\""+num(t.font_size)+"\"><defs><marker id=\"state-barb\" refX=\"19\" refY=\"7\" markerWidth=\"20\" markerHeight=\"14\" markerUnits=\"userSpaceOnUse\" orient=\"auto\"><path d=\"M 19,7 L9,13 L14,7 L9,1 Z\" fill=\""+xml(t.line)+"\"/></marker></defs>"+"<g transform=\"translate(8,8)\">"+body+"</g></svg>",width,height};
 }catch(const std::runtime_error& e){return std::unexpected(e.what());}}
}
