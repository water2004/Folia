module folia.mermaid.radar_svg;
import std;
import folia.mermaid.number;
import folia.mermaid.svg_text;
namespace folia::mermaid {
namespace {
const ConfigValue& member(const ConfigValue& v,std::u16string_view key){auto p=v.find(key);if(!p)throw std::runtime_error("Missing Radar configuration");return *p;}
std::u16string text(const ConfigValue& v){if(v.kind!=ConfigKind::string)throw std::runtime_error("Expected Radar string");return v.string;}
double number(const ConfigValue& v){if(v.kind!=ConfigKind::number||!std::isfinite(v.number)||v.number<0)throw std::runtime_error("Expected nonnegative Radar dimension");return v.number;}
double size(const ConfigValue& v){if(v.kind==ConfigKind::number)return number(v);auto value=text(v);if(value.ends_with(u"px"))value.resize(value.size()-2);std::string s(value.begin(),value.end());double out{};auto p=std::from_chars(s.data(),s.data()+s.size(),out);if(p.ec!=std::errc{}||p.ptr!=s.data()+s.size()||!std::isfinite(out)||out<0)throw std::runtime_error("Invalid Radar font size");return out;}
std::string num(double v){auto result=format_number(v);if(!result)throw std::runtime_error("Invalid Radar geometry");return *result;}
std::string xml(std::u16string_view value){auto result=escape_svg_text(value,u"Radar text");if(!result)throw std::runtime_error("Invalid Radar XML text");return *result;}
std::string attr(std::u16string_view value){std::string out;for(auto c:xml(value)){if(c=='"')out+="&quot;";else if(c=='\n')out+="&#10;";else if(c=='\t')out+="&#9;";else out+=c;}return out;}
std::string point(RadarPoint p){return num(p.x)+","+num(p.y);}
std::string points(const std::vector<RadarPoint>& list){std::string out;for(auto p:list){if(!out.empty())out+=' ';out+=point(p);}return out;}
std::string rounded(const std::vector<RadarPoint>& p,double tension) {
    auto n=p.size();std::string out="M"+point(p[0]);
    for(std::size_t i=0;i<n;++i){const auto p0=p[(i+n-1)%n],p1=p[i],p2=p[(i+1)%n],p3=p[(i+2)%n];
        const RadarPoint cp1{p1.x+(p2.x-p0.x)*tension,p1.y+(p2.y-p0.y)*tension};
        const RadarPoint cp2{p2.x-(p3.x-p1.x)*tension,p2.y-(p3.y-p1.y)*tension};
        out+=" C"+point(cp1)+" "+point(cp2)+" "+point(p2);
    }return out+" Z";
}
}
std::expected<RadarRenderInput,std::string> prepare_radar(std::u16string_view source,const ConfigSession& session) {
    auto prepared=prepare_diagram(source,session);if(!prepared)return std::unexpected(prepared.error());
    auto doc=parse_radar(encode_diagram_source(prepared->code.cleaned));if(!doc)return std::unexpected(doc.error());
    if(doc->title.empty()&&prepared->title)doc->title=*prepared->title;
    try {
        RadarRenderInput out;out.document=std::move(*doc);auto& c=out.config;auto& t=out.theme;
        const auto& cfg=member(prepared->effective_config,u"radar");const auto& theme=member(prepared->effective_config,u"themeVariables");const auto& style=member(theme,u"radar");
        for(auto [key,target]:{std::pair{u"width",&c.width},{u"height",&c.height},{u"marginTop",&c.top},{u"marginRight",&c.right},{u"marginBottom",&c.bottom},{u"marginLeft",&c.left},
            {u"axisScaleFactor",&c.axis_scale},{u"axisLabelFactor",&c.label_scale},{u"curveTension",&c.tension}})*target=number(member(cfg,key));
        if(auto max=cfg.find(u"useMaxWidth")){if(max->kind!=ConfigKind::boolean)throw std::runtime_error("Expected Radar useMaxWidth boolean");c.use_max_width=max->boolean;}
        if(c.width<=0||c.height<=0)throw std::runtime_error("Radar canvas must have positive dimensions");
        t.font_family=text(member(prepared->effective_config,u"fontFamily"));t.font_size=size(member(theme,u"fontSize"));t.text=text(member(theme,u"textColor"));t.title=text(member(theme,u"titleColor"));
        t.axis=text(member(style,u"axisColor"));t.graticule=text(member(style,u"graticuleColor"));
        for(auto [key,target]:{std::pair{u"axisStrokeWidth",&t.axis_width},{u"axisLabelFontSize",&t.axis_size},{u"curveOpacity",&t.curve_opacity},{u"curveStrokeWidth",&t.curve_width},
            {u"graticuleOpacity",&t.graticule_opacity},{u"graticuleStrokeWidth",&t.graticule_width},{u"legendFontSize",&t.legend_size}})*target=number(member(style,key));
        const auto count=number(member(theme,u"THEME_COLOR_LIMIT"));
        for(std::size_t i=0;i<count;++i){auto n=std::to_string(i);t.colors.push_back(text(member(theme,u"cScale"+std::u16string(n.begin(),n.end()))));}
        return out;
    }catch(const std::runtime_error& e){return std::unexpected(e.what());}
}
std::expected<std::vector<RadarCurveGeometry>,std::string> radar_curve_geometry(const RadarRenderInput& input) {
    try {
        const auto& d=input.document;const auto& c=input.config;const auto n=d.axes.size();
        double max=-std::numeric_limits<double>::infinity();for(const auto& curve:d.curves)for(auto value:curve.entries)max=std::max(max,value);if(d.options.max)max=*d.options.max;
        const double radius=std::min(c.width,c.height)/2,min=d.options.min;std::vector<RadarCurveGeometry> out;
        for(std::size_t index=0;index<d.curves.size();++index) {
            const auto& curve=d.curves[index];if(curve.entries.size()!=n)continue;
            if(n==0||max<=min)throw std::runtime_error("Radar curves require a nonempty positive value range");
            RadarCurveGeometry g;g.index=index;
            for(std::size_t i=0;i<n;++i){const auto angle=2*std::numbers::pi*i/n-std::numbers::pi/2;const auto value=std::min(std::max(curve.entries[i],min),max);const auto r=radius*(value-min)/(max-min);g.points.push_back({r*std::cos(angle),r*std::sin(angle)});}
            if(!d.options.polygon)g.path=rounded(g.points,c.tension);out.push_back(std::move(g));
        }return out;
    }catch(const std::runtime_error& e){return std::unexpected(e.what());}
}
std::expected<RadarSvg,std::string> render_radar_svg(const RadarRenderInput& input) {
    auto curves=radar_curve_geometry(input);if(!curves)return std::unexpected(curves.error());
    try {
        const auto& d=input.document;const auto& c=input.config;const auto& t=input.theme;const auto n=d.axes.size();
        const auto width=c.width+c.left+c.right,height=c.height+c.top+c.bottom,radius=std::min(c.width,c.height)/2;
        std::string svg="<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 "+num(width)+" "+num(height)+"\" overflow=\"visible\" font-family=\""+attr(t.font_family)+"\" font-size=\""+num(t.font_size)+"\" fill=\""+attr(t.text)+"\"";
        if(c.use_max_width)svg+=" width=\"100%\" style=\"max-width: "+num(width)+"px;\"";else svg+=" width=\""+num(width)+"\" height=\""+num(height)+"\"";
        svg+='>';for(auto [tag,value]:{std::pair{"title",d.acc_title},{"desc",d.acc_description}})if(!value.empty())svg+="<"+std::string(tag)+">"+xml(value)+"</"+tag+">";
        svg+="<g class=\"radar-main\" transform=\"translate("+num(c.left+c.width/2)+","+num(c.top+c.height/2)+")\">";
        for(int i=0;i<d.options.ticks;++i) {
            const auto r=radius*(i+1)/d.options.ticks;const auto paint=" class=\"radarGraticule\" fill=\""+attr(t.graticule)+"\" fill-opacity=\""+num(t.graticule_opacity)+"\" stroke=\""+attr(t.graticule)+"\" stroke-width=\""+num(t.graticule_width)+"\"/>";
            if(d.options.polygon){std::vector<RadarPoint> ring;for(std::size_t j=0;j<n;++j){const auto angle=2*j*std::numbers::pi/n-std::numbers::pi/2;ring.push_back({r*std::cos(angle),r*std::sin(angle)});}svg+="<polygon points=\""+points(ring)+"\""+paint;}
            else svg+="<circle r=\""+num(r)+"\""+paint;
        }
        for(std::size_t i=0;i<n;++i) {
            const auto angle=2*i*std::numbers::pi/n-std::numbers::pi/2,cos=std::cos(angle),sin=std::sin(angle);
            svg+="<line class=\"radarAxisLine\" x1=\"0\" y1=\"0\" x2=\""+num(radius*c.axis_scale*cos)+"\" y2=\""+num(radius*c.axis_scale*sin)+"\" stroke=\""+attr(t.axis)+"\" stroke-width=\""+num(t.axis_width)+"\"/>";
            const auto anchor=cos>.01?"start":cos<-.01?"end":"middle",baseline=sin>.01?"hanging":sin<-.01?"auto":"central";
            // Official axis/title CSS sets color, not fill. Text inherits textColor.
            svg+="<text class=\"radarAxisLabel\" x=\""+num(radius*c.label_scale*cos+4*cos)+"\" y=\""+num(radius*c.label_scale*sin+4*sin)+"\" font-size=\""+num(t.axis_size)+"\" text-anchor=\""+anchor+"\" dominant-baseline=\""+baseline+"\">"+xml(d.axes[i].label)+"</text>";
        }
        for(const auto& curve:*curves) {
            std::string paint=" class=\"radarCurve-"+std::to_string(curve.index)+"\"";
            if(curve.index<t.colors.size())paint+=" fill=\""+attr(t.colors[curve.index])+"\" fill-opacity=\""+num(t.curve_opacity)+"\" stroke=\""+attr(t.colors[curve.index])+"\" stroke-width=\""+num(t.curve_width)+"\"";
            svg+=d.options.polygon?"<polygon points=\""+points(curve.points)+"\""+paint+"/>":"<path d=\""+curve.path+"\""+paint+"/>";
        }
        if(d.options.show_legend)for(std::size_t i=0;i<d.curves.size();++i) {
            svg+="<g transform=\"translate("+num((c.width/2+c.right)*3/4)+","+num(-(c.height/2+c.top)*3/4+i*20)+")\"><rect class=\"radarLegendBox-"+std::to_string(i)+"\" width=\"12\" height=\"12\"";
            if(i<t.colors.size())svg+=" fill=\""+attr(t.colors[i])+"\" fill-opacity=\""+num(t.curve_opacity)+"\" stroke=\""+attr(t.colors[i])+"\"";
            svg+="/><text class=\"radarLegendText\" x=\"16\" y=\"0\" text-anchor=\"start\" dominant-baseline=\"hanging\" font-size=\""+num(t.legend_size)+"\">"+xml(d.curves[i].label)+"</text></g>";
        }
        svg+="<text class=\"radarTitle\" x=\"0\" y=\""+num(-c.height/2-c.top)+"\" font-size=\""+num(t.font_size)+"\" dominant-baseline=\"hanging\" text-anchor=\"middle\">"+xml(d.title)+"</text></g></svg>";
        return RadarSvg{std::move(svg),width,height};
    }catch(const std::runtime_error& e){return std::unexpected(e.what());}
}
}
