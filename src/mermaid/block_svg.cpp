module folia.mermaid.block_svg;
import std;
import folia.mermaid.number;
import folia.mermaid.svg_text;
import folia.mermaid.theme_color;
import folia.mermaid.flowchart_svg;
namespace folia::mermaid {
namespace {
#include "flowchart_markers.inc"
const ConfigValue& member(const ConfigValue& v,std::u16string_view key){auto p=v.find(key);if(!p)throw std::runtime_error("Missing Block configuration");return *p;}
std::u16string text(const ConfigValue& v){if(v.kind!=ConfigKind::string)throw std::runtime_error("Expected Block style");return v.string;}
double number(const ConfigValue& v){if(v.kind==ConfigKind::number)return v.number;auto s=text(v);std::string raw(s.begin(),s.end());double n=0;auto p=std::from_chars(raw.data(),raw.data()+raw.size(),n);if(p.ec!=std::errc{})throw std::runtime_error("Invalid Block dimension");return n;}
std::string num(double value){auto s=format_number(value);if(!s)throw std::runtime_error("Invalid Block geometry");return *s;}
std::string xml(std::u16string_view value){auto s=escape_svg_text(value,u"Block text");if(!s)throw std::runtime_error("Invalid Block text");return *s;}
std::string attr(std::u16string_view value){std::string s;for(auto c:xml(value))s+=c=='"'?"&quot;":std::string(1,c);return s;}
struct Size {double x=0,y=0,w=0,h=0;};
#include "block_shapes.inc"
struct Bounds {double left=0,top=0,right=0,bottom=0;bool empty=true;void add(double x,double y,double w,double h){if(empty){left=x;top=y;right=x+w;bottom=y+h;empty=false;}else{left=std::min(left,x);top=std::min(top,y);right=std::max(right,x+w);bottom=std::max(bottom,y+h);}}};
using Point=std::pair<double,double>;
std::vector<Point> arrow_points(const std::vector<std::u16string>& directions,double height,double padding,double width){
 std::vector<std::u16string> ordered;
 auto add=[&](std::u16string_view d){if(std::ranges::find(ordered,d)==ordered.end())ordered.emplace_back(d);};
 for(const auto& d:directions){if(d==u"x"){add(u"right");add(u"left");}else if(d==u"y"){add(u"up");add(u"down");}else if(d==u"right"||d==u"left"||d==u"up"||d==u"down")add(d);}
 std::vector<std::u16string> canonical;for(auto d:{u"right",u"left",u"up",u"down"})if(std::ranges::find(ordered,d)!=ordered.end())canonical.emplace_back(d);
 const auto key=[&]{std::u16string s;for(auto d:canonical){if(!s.empty())s+=u'|';s+=d;}return s;}();
 const double mid=height/2;
 auto pts=[&](std::initializer_list<Point> p){return std::vector<Point>(p);};
 if(key==u"right")return pts({{mid,-padding},{mid,-padding},{width-mid,-padding},{width-mid,0},{width,-mid},{width-mid,-height},{width-mid,-height+padding},{mid,-height+padding},{mid,-height+padding}});
 if(key==u"left")return pts({{mid,0},{mid,-padding},{width-mid,-padding},{width-mid,-height+padding},{mid,-height+padding},{mid,-height},{0,-mid}});
 if(key==u"up")return pts({{mid,-padding},{mid,-height+padding},{0,-height+padding},{width/2,-height},{width,-height+padding},{width-mid,-height+padding},{width-mid,-padding}});
 if(key==u"down")return pts({{width/2,0},{0,-padding},{mid,-padding},{mid,-height+padding},{width-mid,-height+padding},{width-mid,-padding},{width,-padding}});
 if(key==u"right|left")return pts({{mid,0},{mid,-padding},{width-mid,-padding},{width-mid,0},{width,-mid},{width-mid,-height},{width-mid,-height+padding},{mid,-height+padding},{mid,-height},{0,-mid}});
 if(key==u"up|down")return pts({{width/2,0},{0,-padding},{mid,-padding},{mid,-height+padding},{0,-height+padding},{width/2,-height},{width,-height+padding},{width-mid,-height+padding},{width-mid,-padding},{width,-padding}});
 if(key==u"right|up")return pts({{0,0},{width,-mid},{0,-height}});
 if(key==u"right|down")return pts({{0,0},{width,0},{0,-height}});
 if(key==u"left|up")return pts({{width,0},{0,-mid},{width,-height}});
 if(key==u"left|down")return pts({{width,0},{0,0},{width,-height}});
 if(key==u"right|left|up")return pts({{mid,0},{width-mid,0},{width,-mid},{width-mid,-height},{mid,-height},{0,-mid}});
 if(key==u"right|left|down")return pts({{0,0},{mid,-height},{width-mid,-height},{width,0}});
 if(key==u"right|up|down")return pts({{0,0},{width,-mid},{width,-height+mid},{0,-height}});
 if(key==u"left|up|down")return pts({{width,0},{0,-mid},{0,-height+mid},{width,-height}});
 if(key==u"right|left|up|down")return pts({{0,0},{mid,0},{width/2,2*padding},{width-mid,0},{width,0},{width,-height/3},{width+2*padding,-mid},{width,-2*height/3},{width,-height},{width-mid,-height},{width/2,-height-2*padding},{mid,-height},{0,-height},{0,-2*height/3},{-2*padding,-mid},{0,-height/3}});
 return pts({{0,0}});
}
Point arrow_extent(const std::vector<std::u16string>& directions,double height,double padding,double width){
 const auto points=arrow_points(directions,height,padding,width);
 double left=0,top=0,right=0,bottom=0;bool first=true;
 for(const auto& [x,y]:points){if(first){left=right=x;top=bottom=y;first=false;}else{left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}}
 return {right-left,bottom-top};
}
}
std::expected<BlockRenderInput,std::string> prepare_block(
    std::u16string_view source,const ConfigSession& session) {
    auto prepared=prepare_diagram(source,session);
    if(!prepared) return std::unexpected(prepared.error());
    auto document=parse_block(prepared->code.cleaned);
    if(!document) return std::unexpected(document.error());
    try {
        BlockRenderInput out;
        out.document=std::move(*document);
        if(prepared->title) out.document.title=*prepared->title;
        const auto& effective=prepared->effective_config;
        const auto& config=member(effective,u"block");
        const auto& theme=member(effective,u"themeVariables");
        out.config.padding=number(member(config,u"padding"));
        out.config.font_size=number(member(effective,u"fontSize"));
        const auto* html=effective.find(u"htmlLabels");
        if(html&&html->kind!=ConfigKind::null) out.config.html_labels=config_truthy(*html);
        // Block nodes read the root flag. Shared edge labels additionally
        // honor the deprecated flowchart override when the root is unset.
        if(!html||html->kind==ConfigKind::null) {
            const auto* flowchart=effective.find(u"flowchart");
            html=flowchart?flowchart->find(u"htmlLabels"):nullptr;
        }
        out.config.edge_html_labels=!html||html->kind==ConfigKind::null||config_truthy(*html);
        auto& style=out.theme;
        style.font_family=text(member(effective,u"fontFamily"));
        style.fill=text(member(theme,u"mainBkg"));
        style.stroke=text(member(theme,u"nodeBorder"));
        const auto* node_text=theme.find(u"nodeTextColor");
        style.text_color=node_text&&node_text->kind==ConfigKind::string&&!node_text->string.empty()
            ?node_text->string:text(member(theme,u"textColor"));
        style.line_color=text(member(theme,u"lineColor"));
        style.cluster_fill=theme_detail::color_adjust(text(member(theme,u"clusterBkg")),{{'a',1}});
        style.cluster_stroke=theme_detail::color_adjust(text(member(theme,u"clusterBorder")),{{'a',1}});
        return out;
    } catch(const std::runtime_error& error) {
        return std::unexpected(error.what());
    }
}

std::expected<BlockSvg,std::string> render_block_svg(
    const BlockRenderInput& in,const BlockMeasure& measure,const MathLabelMeasure& math_measure) {
    try {
        const auto& d=in.document;
        const auto& c=in.config;
        const auto& t=in.theme;
        if((!measure&&!math_measure)||d.nodes.empty())
            throw std::runtime_error("Block requires measured labels");
        struct MeasuredLabel {
            BlockTextBox text;
            std::optional<MathLabel> math;
            double width=0,height=0;
        };
        std::vector<Size> sizes(d.nodes.size());
        std::vector<MeasuredLabel> labels(d.nodes.size());
        const double pad=c.padding,line_height=c.font_size*1.5;
        const auto metric=[&](std::u16string_view value,bool html) {
            MeasuredLabel label;
            if(html&&math_measure&&has_math_label(value)) {
                auto math=math_measure(value,t.font_family,c.font_size,400,false);
                if(!math) throw std::runtime_error(math.error());
                if(*math) {
                    label.math=std::move(**math);
                    label.width=label.math->width;
                    label.height=label.math->height;
                    return label;
                }
            }
            if(!measure) throw std::runtime_error("Block requires measured text labels");
            auto plain=measure(value,c.font_size);
            if(!plain) throw std::runtime_error(plain.error());
            label.text=*plain;
            label.width=plain->advance;
            label.height=value.empty()?0:line_height;
            return label;
        };
        const auto shape_label_height=[&](const MeasuredLabel& label) {
            return label.math?label.height:line_height;
        };
        for(std::size_t i=1;i<d.nodes.size();++i) {
            const auto& node=d.nodes[i];
            if(node.type==u"space") continue;
            labels[i]=metric(node.label,c.html_labels);
            auto& size=sizes[i];
            const auto width=labels[i].width,height=labels[i].height;
            if(node.type==u"composite") {
                size.w=width;size.h=height;
            } else if(node.type==u"block_arrow") {
                const auto arrow_height=shape_label_height(labels[i])+2*pad;
                const auto arrow_width=width+arrow_height+pad;
                const auto extent=arrow_extent(node.directions,arrow_height,pad/2,arrow_width);
                size.w=extent.first;size.h=extent.second;
            } else {
                const auto box=block_shapes::intrinsic(node.type,width,height,pad);
                size.w=box.width;size.h=box.height;
            }
        }
        std::function<void(std::size_t,double,double)> set_sizes=
            [&](std::size_t i,double sibling_width,double sibling_height) {
            auto& size=sizes[i];
            const auto& node=d.nodes[i];
            if(!size.w) size={0,0,sibling_width,sibling_height};
            if(node.children.empty()) return;
            for(auto child:node.children) set_sizes(child,0,0);
            double maximum_width=0,maximum_height=0;
            for(auto child:node.children) if(d.nodes[child].type!=u"space") {
                maximum_width=std::max(maximum_width,sizes[child].w/d.nodes[child].span);
                maximum_height=std::max(maximum_height,sizes[child].h);
            }
            for(auto child:node.children) {
                const auto span=d.nodes[child].span;
                sizes[child]={0,0,maximum_width*span+pad*(span-1),maximum_height};
            }
            for(auto child:node.children) set_sizes(child,maximum_width,maximum_height);
            int items=0;
            for(auto child:node.children) items+=d.nodes[child].span;
            auto columns=static_cast<int>(node.children.size());
            if(node.columns>0&&node.columns<items) columns=node.columns;
            const auto rows=std::ceil(double(items)/columns);
            double width=columns*(maximum_width+pad)+pad;
            double height=rows*(maximum_height+pad)+pad;
            if(width<sibling_width) {
                width=sibling_width;height=sibling_height;
                for(auto child:node.children) sizes[child]={0,0,
                    (sibling_width-columns*pad-pad)/columns,(sibling_height-rows*pad-pad)/rows};
            }
            if(width<size.w) {
                width=size.w;
                const auto count=node.columns>0?
                    std::min(static_cast<int>(node.children.size()),node.columns):
                    static_cast<int>(node.children.size());
                for(auto child:node.children) sizes[child].w=(width-count*pad-pad)/count;
            }
            // The composite's own mathematical label also has a real height;
            // a short child row must not replace that height with a text line.
            if(labels[i].math) height=std::max(height,labels[i].height+2*pad);
            size={0,0,width,height};
        };
 set_sizes(0,0,0);
 std::function<void(std::size_t)> position=[&](std::size_t i){const auto& n=d.nodes[i];const auto parent=sizes[i];std::map<int,double> heights,offsets;int col=0;for(auto child:n.children){const int row=n.columns>0?col/n.columns:0;heights[row]=std::max(heights[row],sizes[child].h);col+=n.columns>0?std::min(d.nodes[child].span,n.columns-col%n.columns):d.nodes[child].span;}double offset=0;for(auto [row,h]:heights){offsets[row]=offset;offset+=h+pad;}double x=parent.x?parent.x-parent.w/2:-pad;int previous=0;col=0;for(auto child:n.children){const int row=n.columns>0?col/n.columns:0;if(row!=previous){x=parent.x?parent.x-parent.w/2:-pad;previous=row;}auto& s=sizes[child];s.x=x+pad+s.w/2;s.y=parent.y-parent.h/2+offsets[row]+heights[row]/2+pad;x=s.x+s.w/2;position(child);col+=n.columns>0?std::min(d.nodes[child].span,n.columns-col%n.columns):d.nodes[child].span;}};position(0);
 Bounds bounds;std::string svg;
 const auto label=[&](std::u16string_view value,const MeasuredLabel& measured,
     double x,double y,std::u16string_view color,bool placeholder) {
     const auto width=value.empty()?0:measured.width;
     const auto height=value.empty()?0:measured.height;
     const auto left=x-width/2,top=y-height/2;
     if(placeholder) svg+="<rect x=\""+num(left)+"\" y=\""+num(top)+
         "\" width=\"0\" height=\"0\" fill=\""+attr(t.fill)+"\" stroke=\""+attr(t.stroke)+"\"/>";
     if(measured.math) {
         const std::string paint(color.begin(),color.end());
         auto rendered=render_math_label_svg(*measured.math,left,top,
             {t.font_family,paint,"block-math-label"});
         if(!rendered) throw std::runtime_error(rendered.error());
         svg+=*rendered;
     } else {
         const auto baseline=value.empty()?y:
             top+(line_height-measured.text.line_height)/2+measured.text.baseline;
         svg+="<text data-html-label=\"true\" x=\""+num(left)+"\" y=\""+num(baseline)+
             "\" fill=\""+attr(color)+"\">"+xml(value)+"</text>";
     }
     bounds.add(left,top,width,height);
 };
 std::function<void(std::size_t)> draw=[&](std::size_t i){const auto& n=d.nodes[i];auto s=sizes[i];if(i&&n.type!=u"space"){std::u16string fill=t.fill,stroke=t.stroke,color=t.text_color;auto apply=[&](const std::vector<std::u16string>& styles){for(const auto& style:styles){auto p=style.find(u':');if(p==style.npos)continue;auto key=style.substr(0,p),value=style.substr(p+1);if(key==u"fill")fill=value;else if(key==u"stroke")stroke=value;else if(key==u"color")color=value;}};for(const auto& name:n.classes)for(const auto& cls:d.classes)if(name==cls.id)apply(cls.styles);apply(n.styles);std::string paint=" fill=\""+attr(fill)+"\" stroke=\""+attr(stroke)+"\" stroke-width=\"1\"";
 if(n.type==u"diamond"){s.w=s.h=labels[i].width+shape_label_height(labels[i])+2*pad;svg+="<polygon transform=\"translate("+num(s.x-s.w/2+.5)+","+num(s.y+s.h/2)+")\" points=\""+num(s.w/2)+",0 "+num(s.w)+","+num(-s.h/2)+" "+num(s.w/2)+","+num(-s.h)+" 0,"+num(-s.h/2)+"\""+paint+"/>";bounds.add(s.x-s.w/2+.5,s.y-s.h/2,s.w,s.h);}
 else if(n.type==u"block_arrow"){s.h=shape_label_height(labels[i])+2*pad;if(n.span==1)s.w=labels[i].width+s.h+pad;const auto p=pad/2;const auto points=arrow_points(n.directions,s.h,p,s.w);std::string encoded;double left=0,top=0,right=0,bottom=0;bool first=true;for(const auto& [x,y]:points){if(!encoded.empty())encoded+=' ';encoded+=num(x)+","+num(y);if(first){left=right=x;top=bottom=y;first=false;}else{left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}}svg+="<polygon transform=\"translate("+num(s.x-s.w/2)+","+num(s.y+s.h/2)+")\" points=\""+encoded+"\""+paint+"/>";bounds.add(s.x-s.w/2+left,s.y+s.h/2+top,right-left,bottom-top);}else {
     if(n.type==u"composite") {
         paint=" fill=\""+attr(t.cluster_fill)+"\" fill-opacity=\"0.5\" stroke=\""+attr(t.cluster_stroke)+"\" stroke-opacity=\"0.2\" stroke-width=\"1\"";
         svg+="<rect x=\""+num(s.x-s.w/2)+"\" y=\""+num(s.y-s.h/2)+"\" width=\""+num(s.w)+"\" height=\""+num(s.h)+"\""+paint+"/>";
     } else {
         const auto box=block_shapes::positioned(n.type,labels[i].width,labels[i].height,pad,{s.w,s.h});
         s.w=box.width;s.h=box.height;
         svg+="<g data-block-shape=\""+attr(n.type)+"\" transform=\"translate("+num(s.x)+","+num(s.y)+")\">"+
             block_shapes::paint(n.type,box,paint)+"</g>";
     }
     bounds.add(s.x-s.w/2,s.y-s.h/2,s.w,s.h);
 }
 const auto offset=block_shapes::label_offset(n.type,{s.w,s.h},pad);
 label(n.label,labels[i],s.x+offset.x,s.y+offset.y,color,true);if(n.type!=u"block_arrow")sizes[i]=s;}for(auto child:n.children)draw(child);};draw(0);
 for(const auto& edge:d.edges) {
     const auto index=[&](std::u16string_view id) {
         for(std::size_t i=0;i<d.nodes.size();++i) if(d.nodes[i].id==id) return i;
         throw std::runtime_error("Unknown Block edge node");
     };
     const auto from=index(edge.start),to=index(edge.end);
     const auto a=sizes[from],b=sizes[to];
     auto [x1,y1]=block_shapes::intersect(d.nodes[from].type,a,{b.x,b.y});
     auto [x3,y3]=block_shapes::intersect(d.nodes[to].type,b,{a.x,a.y});
     const auto x2=(a.x+b.x)/2,y2=(a.y+b.y)/2;
     const auto shorten=[&](double& x,double& y) {
         const auto length=std::hypot(x-x2,y-y2);
         if(length) {x-=4*(x-x2)/length;y-=4*(y-y2)/length;}
     };
     // lineWithOffset shortens only point markers in the classic look.
     if(edge.arrow_start==u"arrow_point") shorten(x1,y1);
     if(edge.arrow_end==u"arrow_point") shorten(x3,y3);
     const auto path="M"+num(x1)+","+num(y1)+"L"+num((5*x1+x2)/6)+","+num((5*y1+y2)/6)+
         "C"+num((2*x1+x2)/3)+","+num((2*y1+y2)/3)+","+num((x1+2*x2)/3)+","+
         num((y1+2*y2)/3)+","+num((x1+4*x2+x3)/6)+","+num((y1+4*y2+y3)/6)+
         "C"+num((2*x2+x3)/3)+","+num((2*y2+y3)/3)+","+num((x2+2*x3)/3)+","+
         num((y2+2*y3)/3)+","+num((x2+5*x3)/6)+","+num((y2+5*y3)/6)+
         "L"+num(x3)+","+num(y3);
     svg+="<path class=\"block-edge\" d=\""+path+"\" fill=\"none\" stroke=\""+attr(t.line_color)+
         "\" stroke-width=\""+(edge.thickness==u"thick"?"3.5":"1")+
         "\" stroke-dasharray=\""+(edge.pattern==u"dotted"?"2":"0")+"\"";
     if(!edge.arrow_start.empty()&&edge.arrow_start!=u"arrow_open")
         svg+=" marker-start=\"url(#block-"+std::string(marker_name(edge.arrow_start))+"-start)\"";
     if(!edge.arrow_end.empty())
         svg+=" marker-end=\"url(#block-"+std::string(marker_name(edge.arrow_end))+"-end)\"";
     svg+="/>";
     bounds.add(std::min(x1,x3),std::min(y1,y3),std::abs(x3-x1),std::abs(y3-y1));
     if(!edge.label.empty()) label(edge.label,metric(edge.label,c.edge_html_labels),x2,y2,t.text_color,false);
 }
 const auto w=bounds.right-bounds.left,h=bounds.bottom-bounds.top;
 const auto width=w+10,height=h+std::max(1.0,std::round(.125*w/h))+10;
 const auto header="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""+num(width)+
     "\" height=\""+num(height)+"\" viewBox=\""+num(bounds.left-5)+" "+num(bounds.top-5)+
     " "+num(width)+" "+num(h+10)+"\" style=\"font-family:"+attr(t.font_family)+
     ";font-size:"+num(c.font_size)+"px\">"+marker_definitions(attr(t.line_color),"block");
 return BlockSvg{header+svg+"</svg>",width,height};
 }catch(const std::runtime_error& e){return std::unexpected(e.what());}}
}
