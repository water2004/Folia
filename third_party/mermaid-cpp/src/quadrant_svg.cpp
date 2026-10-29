module folia.mermaid.quadrant_svg;
import std;
import folia.mermaid.number;
import folia.mermaid.svg_text;

namespace folia::mermaid {
namespace {
const ConfigValue& member(const ConfigValue& v,std::u16string_view key) {
    auto p=v.find(key);if(!p)throw std::runtime_error("Missing Quadrant configuration");return *p;
}
std::u16string text(const ConfigValue& v) {if(v.kind!=ConfigKind::string)throw std::runtime_error("Expected Quadrant string");return v.string;}
double number(const ConfigValue& v) {if(v.kind!=ConfigKind::number||!std::isfinite(v.number)||v.number<0)throw std::runtime_error("Expected nonnegative Quadrant dimension");return v.number;}
std::string xml(std::u16string_view s) {auto result=escape_svg_text(s,u"Quadrant text");if(!result)throw std::runtime_error("Invalid Quadrant XML text");return *result;}
std::string attr(std::u16string_view s) {auto raw=xml(s);std::string out;for(auto c:raw){if(c=='"')out+="&quot;";else if(c=='\n')out+="&#10;";else if(c=='\t')out+="&#9;";else out+=c;}return out;}
}
std::expected<QuadrantRenderInput,std::string> prepare_quadrant(std::u16string_view source,const ConfigSession& session) {
    auto prepared=prepare_diagram(source,session);if(!prepared)return std::unexpected(prepared.error());
    auto doc=parse_quadrant(encode_diagram_source(prepared->code.cleaned));if(!doc)return std::unexpected(doc.error());
    if(doc->title.empty()&&prepared->title)doc->title=*prepared->title;
    try {
        QuadrantRenderInput out;out.document=std::move(*doc);auto& c=out.config;auto& t=out.theme;
        const auto& cfg=member(prepared->effective_config,u"quadrantChart");const auto& theme=member(prepared->effective_config,u"themeVariables");
        for(auto [key,target]:{std::pair{u"chartWidth",&c.width},{u"chartHeight",&c.height},{u"titlePadding",&c.title_padding},{u"titleFontSize",&c.title_size},
            {u"quadrantPadding",&c.padding},{u"xAxisLabelPadding",&c.x_padding},{u"yAxisLabelPadding",&c.y_padding},{u"xAxisLabelFontSize",&c.x_size},
            {u"yAxisLabelFontSize",&c.y_size},{u"quadrantLabelFontSize",&c.quadrant_size},{u"quadrantTextTopPadding",&c.quadrant_top_padding},
            {u"pointTextPadding",&c.point_padding},{u"pointLabelFontSize",&c.point_size},{u"pointRadius",&c.point_radius},
            {u"quadrantInternalBorderStrokeWidth",&c.inner_border},{u"quadrantExternalBorderStrokeWidth",&c.outer_border}})*target=number(member(cfg,key));
        auto x=text(member(cfg,u"xAxisPosition")),y=text(member(cfg,u"yAxisPosition"));
        if((x!=u"top"&&x!=u"bottom")||(y!=u"left"&&y!=u"right"))throw std::runtime_error("Invalid Quadrant axis position");
        c.x_top=x==u"top";c.y_left=y==u"left";
        const auto& max=member(cfg,u"useMaxWidth");if(max.kind!=ConfigKind::boolean)throw std::runtime_error("Expected Quadrant useMaxWidth boolean");c.use_max_width=max.boolean;
        for(std::size_t i=0;i<4;++i){auto prefix=u"quadrant"+std::u16string(1,static_cast<char16_t>(u'1'+i));t.fill[i]=text(member(theme,prefix+u"Fill"));t.text[i]=text(member(theme,prefix+u"TextFill"));}
        for(auto [key,target]:{std::pair{u"quadrantPointFill",&t.point},{u"quadrantPointTextFill",&t.point_text},{u"quadrantXAxisTextFill",&t.x_text},
            {u"quadrantYAxisTextFill",&t.y_text},{u"quadrantTitleFill",&t.title},{u"quadrantInternalBorderStrokeFill",&t.inner_border},{u"quadrantExternalBorderStrokeFill",&t.outer_border}})*target=text(member(theme,key));
        t.font_family=text(member(prepared->effective_config,u"fontFamily"));t.inherited_text=text(member(theme,u"textColor"));
        if(c.width<=0||c.height<=0)throw std::runtime_error("Quadrant canvas must have positive dimensions");
        return out;
    }catch(const std::runtime_error& e){return std::unexpected(e.what());}
}
QuadrantLayout layout_quadrant(const QuadrantRenderInput& input) {
    const auto& d=input.document;const auto& c=input.config;const auto& t=input.theme;QuadrantLayout out;
    const bool show_x=!d.x_axis[0].empty()||!d.x_axis[1].empty(),show_y=!d.y_axis[0].empty()||!d.y_axis[1].empty();
    const bool x_top=d.points.empty()&&c.x_top;
    const double xs=show_x?c.x_padding*2+c.x_size:0,ys=show_y?c.y_padding*2+c.y_size:0,ts=d.title.empty()?0:c.title_size+c.title_padding*2;
    const double left=c.padding+(c.y_left?ys:0),top=c.padding+(x_top?xs:0)+ts;
    const double width=c.width-c.padding*2-ys,height=c.height-c.padding*2-xs-ts,hw=width/2,hh=height/2;
    for(std::size_t i=0;i<4;++i) {
        const double x=left+((i==0||i==3)?hw:0),y=top+(i>=2?hh:0);
        out.quadrants[i]={x,y,hw,hh,t.fill[i],{d.quadrants[i],t.text[i],x+hw/2,y+(d.points.empty()?hh/2:c.quadrant_top_padding),c.quadrant_size,0,true,d.points.empty()}};
    }
    const auto half=c.outer_border/2;
    out.borders={QuadrantBorder{left-half,top,left+width+half,top,c.outer_border,t.outer_border},
        {left+width,top+half,left+width,top+height-half,c.outer_border,t.outer_border},
        {left-half,top+height,left+width+half,top+height,c.outer_border,t.outer_border},
        {left,top+half,left,top+height-half,c.outer_border,t.outer_border},
        {left+hw,top+half,left+hw,top+height-half,c.inner_border,t.inner_border},
        {left+half,top+hh,left+width-half,top+hh,c.inner_border,t.inner_border}};
    for(std::size_t i=0;i<2;++i) {
        const bool center=!d.x_axis[1].empty();
        if(!d.x_axis[i].empty())out.axes.push_back({d.x_axis[i],t.x_text,left+i*hw+(center?hw/2:0),c.x_padding+(x_top?ts:top+height+c.padding),c.x_size,0,center,false});
    }
    for(std::size_t i=0;i<2;++i) {
        const bool center=!d.y_axis[1].empty();
        if(!d.y_axis[i].empty())out.axes.push_back({d.y_axis[i],t.y_text,c.y_padding+(c.y_left?0:left+width+c.padding),top+height-i*hh-(center?hh/2:0),c.y_size,-90,center,false});
    }
    for(const auto& p:d.points) {
        QuadrantStyle style;auto found=d.classes.find(p.class_name);if(found!=d.classes.end())style=found->second;
        if(p.style.radius)style.radius=p.style.radius;if(p.style.color)style.color=p.style.color;if(p.style.stroke_color)style.stroke_color=p.style.stroke_color;if(p.style.stroke_width)style.stroke_width=p.style.stroke_width;
        // d3 interpolateNumber uses a*(1-t)+b*t; preserve that arithmetic.
        const auto x=left*(1-p.x)+(left+width)*p.x,y=(top+height)*(1-p.y)+top*p.y;
        out.points.push_back({x,y,style.radius.value_or(c.point_radius),style.color.value_or(t.point),style.stroke_color.value_or(t.point),style.stroke_width.value_or(u"0px"),{p.text,t.point_text,x,y+c.point_padding,c.point_size}});
    }
    if(!d.title.empty())out.title=QuadrantText{d.title,t.title,c.width/2,c.title_padding,c.title_size};
    return out;
}
std::expected<QuadrantSvg,std::string> render_quadrant_svg(const QuadrantRenderInput& input) {
    try {
        const auto& c=input.config;const auto l=layout_quadrant(input);
        if(l.quadrants[0].width<=0||l.quadrants[0].height<=0)throw std::runtime_error("Quadrant labels leave no chart area");
        auto num=[](double v){auto n=format_number(v);if(!n)throw std::runtime_error("Invalid Quadrant geometry");return *n;};
        auto text_node=[&](const QuadrantText& v){return "<text x=\"0\" y=\"0\" fill=\""+attr(v.fill)+"\" font-size=\""+num(v.size)+"\" dominant-baseline=\""+(v.middle?"middle":"hanging")+"\" text-anchor=\""+(v.centered?"middle":"start")+"\" transform=\"translate("+num(v.x)+","+num(v.y)+") rotate("+num(v.rotation)+")\">"+xml(v.text)+"</text>";};
        std::string svg="<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 "+num(c.width)+" "+num(c.height)+"\"";
        if(c.use_max_width)svg+=" width=\"100%\" style=\"max-width: "+num(c.width)+"px;\"";
        else svg+=" width=\""+num(c.width)+"\" height=\""+num(c.height)+"\"";
        svg+='>';
        for(auto [tag,value]:{std::pair{"title",input.document.acc_title},{"desc",input.document.acc_description}})
            if(!value.empty())svg+="<"+std::string(tag)+">"+xml(value)+"</"+tag+">";
        svg+="<g class=\"main\" font-family=\""+attr(input.theme.font_family)+"\" fill=\""+attr(input.theme.inherited_text)+"\"><g class=\"quadrants\">";
        for(const auto& q:l.quadrants)svg+="<g class=\"quadrant\"><rect x=\""+num(q.x)+"\" y=\""+num(q.y)+"\" width=\""+num(q.width)+"\" height=\""+num(q.height)+"\" fill=\""+attr(q.fill)+"\"/>"+text_node(q.text)+"</g>";
        svg+="</g><g class=\"border\">";
        for(const auto& b:l.borders)svg+="<line x1=\""+num(b.x1)+"\" y1=\""+num(b.y1)+"\" x2=\""+num(b.x2)+"\" y2=\""+num(b.y2)+"\" stroke=\""+attr(b.fill)+"\" stroke-width=\""+num(b.width)+"\"/>";
        svg+="</g><g class=\"data-points\">";
        for(const auto& p:l.points)svg+="<g class=\"data-point\"><circle cx=\""+num(p.x)+"\" cy=\""+num(p.y)+"\" r=\""+num(p.radius)+"\" fill=\""+attr(p.fill)+"\" stroke=\""+attr(p.stroke)+"\" stroke-width=\""+attr(p.stroke_width)+"\"/>"+text_node(p.text)+"</g>";
        svg+="</g><g class=\"labels\">";for(const auto& a:l.axes)svg+="<g class=\"label\">"+text_node(a)+"</g>";
        svg+="</g><g class=\"title\">";if(l.title)svg+=text_node(*l.title);svg+="</g></g></svg>";
        return QuadrantSvg{std::move(svg),c.width,c.height};
    }catch(const std::runtime_error& e){return std::unexpected(e.what());}
}
}
