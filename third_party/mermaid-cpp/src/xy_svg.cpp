module folia.mermaid.xy_svg;
import std;
import folia.mermaid.number;
import folia.mermaid.svg_text;
namespace folia::mermaid {
namespace {
const ConfigValue& member(const ConfigValue& v,std::u16string_view key){auto p=v.find(key);if(!p)throw std::runtime_error("Missing XY configuration");return *p;}
std::u16string text(const ConfigValue& v){if(v.kind!=ConfigKind::string)throw std::runtime_error("Expected XY string");return v.string;}
double number(const ConfigValue& v){if(v.kind!=ConfigKind::number||!std::isfinite(v.number)||v.number<0)throw std::runtime_error("Expected XY dimension");return v.number;}
bool boolean(const ConfigValue& v){if(v.kind!=ConfigKind::boolean)throw std::runtime_error("Expected XY boolean");return v.boolean;}
std::string num(double v){auto n=format_number(v);if(!n)throw std::runtime_error("Invalid XY geometry");return *n;}
std::u16string utext(double v){auto s=num(v);return {s.begin(),s.end()};}
std::string xml(std::u16string_view s){auto t=escape_svg_text(s,u"XY text");if(!t)throw std::runtime_error("Invalid XY text");return *t;}
std::string attr(std::u16string_view s){std::string out;for(auto c:xml(s)){if(c=='"')out+="&quot;";else out+=c;}return out;}
// d3-array ticks, with the default scaleLinear count of ten.
std::vector<double> ticks(double start,double stop){
    bool reverse=stop<start;if(reverse)std::swap(start,stop);if(start==stop)return {start};
    double step=(stop-start)/10,power=std::floor(std::log10(step)),error=step/std::pow(10,power);
    double factor=error>=std::sqrt(50)?10:error>=std::sqrt(10)?5:error>=std::sqrt(2)?2:1;
    double inc;long long first,last;
    if(power<0){inc=std::pow(10,-power)/factor;first=static_cast<long long>(std::floor(start*inc+.5));last=static_cast<long long>(std::floor(stop*inc+.5));if(first/inc<start)++first;if(last/inc>stop)--last;inc=-inc;}
    else{inc=std::pow(10,power)*factor;first=static_cast<long long>(std::floor(start/inc+.5));last=static_cast<long long>(std::floor(stop/inc+.5));if(first*inc<start)++first;if(last*inc>stop)--last;}
    std::vector<double> out;for(auto i=first;i<=last;++i)out.push_back(inc<0?i/-inc:i*inc);if(reverse)std::ranges::reverse(out);return out;
}
std::vector<std::u16string> labels(const XYAxis& a,bool left){if(a.band)return a.categories;auto values=ticks(left?a.max:a.min,left?a.min:a.max);std::vector<std::u16string> out;for(auto v:values)out.push_back(utext(v));return out;}
void axis_config(XYAxisConfig& out,const ConfigValue& v){
    for(auto [key,target]:{std::pair{u"labelFontSize",&out.label_size},{u"labelPadding",&out.label_padding},{u"titleFontSize",&out.title_size},{u"titlePadding",&out.title_padding},{u"tickLength",&out.tick_length},{u"tickWidth",&out.tick_width},{u"axisLineWidth",&out.line_width}})*target=number(member(v,key));
    for(auto [key,target]:{std::pair{u"showLabel",&out.show_label},{u"showTitle",&out.show_title},{u"showTick",&out.show_tick},{u"showAxisLine",&out.show_line}})*target=boolean(member(v,key));
    if(auto p=v.find(u"labelRotation")){if(p->kind!=ConfigKind::number)throw std::runtime_error("Invalid XY rotation");out.rotation=p->number>=-90&&p->number<=90?p->number:0;}
}
void axis_theme(XYAxisTheme& out,const ConfigValue& v,std::u16string prefix){out.title=text(member(v,prefix+u"TitleColor"));out.label=text(member(v,prefix+u"LabelColor"));out.tick=text(member(v,prefix+u"TickColor"));out.line=text(member(v,prefix+u"LineColor"));}
}
std::expected<XYRenderInput,std::string> prepare_xy(std::u16string_view source,const ConfigSession& session){
    auto p=prepare_diagram(source,session);if(!p)return std::unexpected(p.error());auto d=parse_xy(encode_diagram_source(p->code.cleaned));if(!d)return std::unexpected(d.error());
    try{XYRenderInput out;out.document=std::move(*d);if(out.document.title.empty()&&p->title)out.document.title=*p->title;auto& c=out.config;auto& t=out.theme;
        const auto& cfg=member(p->effective_config,u"xyChart");const auto& theme=member(member(p->effective_config,u"themeVariables"),u"xyChart");
        for(auto [key,target]:{std::pair{u"width",&c.width},{u"height",&c.height},{u"titleFontSize",&c.title_size},{u"titlePadding",&c.title_padding},{u"legendFontSize",&c.legend_size},{u"legendPadding",&c.legend_padding},{u"plotReservedSpacePercent",&c.reserved}})*target=number(member(cfg,key));
        for(auto [key,target]:{std::pair{u"showTitle",&c.show_title},{u"showLegend",&c.show_legend},{u"showDataLabel",&c.data_labels},{u"showDataLabelOutsideBar",&c.outside_labels}})*target=boolean(member(cfg,key));
        c.horizontal=out.document.horizontal.value_or(text(member(cfg,u"chartOrientation"))==u"horizontal");axis_config(c.x,member(cfg,u"xAxis"));axis_config(c.y,member(cfg,u"yAxis"));
        t.font_family=text(member(p->effective_config,u"fontFamily"));t.title=text(member(theme,u"titleColor"));t.background=text(member(theme,u"backgroundColor"));t.legend=text(member(theme,u"legendTextColor"));t.data_label=text(member(theme,u"dataLabelColor"));axis_theme(t.x,theme,u"xAxis");axis_theme(t.y,theme,u"yAxis");
        auto palette=text(member(theme,u"plotColorPalette"));std::size_t start=0;do{auto end=palette.find(u',',start);if(end==palette.npos)end=palette.size();auto color=palette.substr(start,end-start);while(!color.empty()&&color.front()==u' ')color.erase(0,1);while(!color.empty()&&color.back()==u' ')color.pop_back();t.colors.push_back(color);start=end+1;}while(start<=palette.size());
        if(c.width<=0||c.height<=0||out.document.plots.empty())throw std::runtime_error("XY requires a positive canvas and a plot");return out;
    }catch(const std::runtime_error& e){return std::unexpected(e.what());}
}
std::vector<XYTextRequest> xy_measurement_requests(const XYRenderInput& in){
    const auto& d=in.document;const auto& c=in.config;std::vector<XYTextRequest> out{{d.title,c.title_size},{d.x.title,c.x.title_size},{d.y.title,c.y.title_size}};
    for(auto s:labels(d.x,c.horizontal))out.push_back({std::move(s),c.x.label_size});for(auto s:labels(d.y,!c.horizontal))out.push_back({std::move(s),c.y.label_size});for(const auto& p:d.plots)if(!p.title.empty())out.push_back({p.title,c.legend_size});return out;
}
namespace {
struct Axis {
    const XYAxis& data;const XYAxisConfig& cfg;const XYAxisTheme& theme;
    bool left=false,top=false,line=false,label=false,tick=false,title=false;
    double x=0,y=0,width=0,height=0,start=0,end=10,padding=0,title_height=0,label_height=0,label_width=0;
    std::vector<std::u16string> tick_labels;
    void space(double w,double h,XYTextBox label_box,XYTextBox title_box){
        double available=left?w:h;title_height=title_box.height;label_height=label_box.height;label_width=label_box.width;
        if(cfg.show_line&&available>cfg.line_width){line=true;available-=cfg.line_width;}
        if(cfg.show_label){padding=std::min((left?label_box.height:label_box.width)/2,.2*(left?h:w));double need=left?label_box.width:label_box.height;if(!left&&!top&&cfg.rotation!=0){auto r=cfg.rotation*std::numbers::pi/180;need=std::max(need,std::abs(std::sin(r)*label_box.width)+std::abs(std::cos(r)*label_box.height));}need+=cfg.label_padding*2;if(need<=available){label=true;available-=need;}}
        if(cfg.show_tick&&available>=cfg.tick_length){tick=true;available-=cfg.tick_length;}
        if(cfg.show_title&&!data.title.empty()){auto need=title_box.height+2*cfg.title_padding;if(need<=available){title=true;available-=need;}}
        width=left?w-available:w;height=left?h:h-available;
    }
    double distance()const{return std::abs(end-start-2*padding)/tick_labels.size();}
    double scale(double value,std::u16string_view category={})const{
        const double a=start+padding,b=end-padding;
        if(data.band){std::vector<std::u16string> unique;for(const auto& s:data.categories)if(std::ranges::find(unique,s)==unique.end())unique.push_back(s);auto i=std::ranges::find(unique,category);if(i==unique.end())return a;return unique.size()==1?(a+b)/2:a+(b-a)*(i-unique.begin())/(unique.size()-1);}
        const auto lo=left?data.max:data.min,hi=left?data.min:data.max;const auto ratio=lo==hi?.5:(value-lo)/(hi-lo);return a*(1-ratio)+b*ratio;
    }
    double at(std::size_t index)const{if(data.band)return scale(0,tick_labels[index]);auto values=ticks(left?data.max:data.min,left?data.min:data.max);return scale(values[index]);}
};
struct Svg {
    std::string s;
    void open(std::string_view cls){s+="<g class=\""+std::string(cls)+"\">";}void close(){s+="</g>";}
    void text(std::u16string_view value,double x,double y,double size,std::u16string_view color,std::string_view anchor="middle",std::string_view baseline="text-before-edge",double rotation=0){s+="<text x=\"0\" y=\"0\" fill=\""+attr(color)+"\" font-size=\""+num(size)+"\" dominant-baseline=\""+std::string(baseline)+"\" text-anchor=\""+std::string(anchor)+"\" transform=\"translate("+num(x)+", "+num(y)+") rotate("+num(rotation)+")\">"+xml(value)+"</text>";}
    void data_text(std::u16string_view value,double x,double y,double size,std::u16string_view color,std::string_view anchor,std::string_view baseline){s+="<text x=\""+num(x)+"\" y=\""+num(y)+"\" fill=\""+attr(color)+"\" font-size=\""+num(size)+"px\" dominant-baseline=\""+std::string(baseline)+"\" text-anchor=\""+std::string(anchor)+"\">"+xml(value)+"</text>";}
    void path(std::string_view d,std::u16string_view color,double width){s+="<path d=\""+std::string(d)+"\" fill=\"none\" stroke=\""+attr(color)+"\" stroke-width=\""+num(width)+"\"/>";}
    void line(double x1,double y1,double x2,double y2,std::u16string_view color,double width){path("M "+num(x1)+","+num(y1)+" L "+num(x2)+","+num(y2),color,width);}
    void rect(double x,double y,double width,double height,std::u16string_view color){s+="<rect x=\""+num(x)+"\" y=\""+num(y)+"\" width=\""+num(width)+"\" height=\""+num(height)+"\" fill=\""+attr(color)+"\" stroke=\""+attr(color)+"\" stroke-width=\"0\"/>";}
};
void draw_axis(Svg& svg,const Axis& a){
    const auto& c=a.cfg;const double line=a.line?c.line_width:0,tick=a.tick?c.tick_length:0;
    svg.open(a.left?"left-axis":a.top?"top-axis":"bottom-axis");
    if(a.line){svg.open(a.left?"axisl-line":"axis-line");if(a.left)svg.line(a.x+a.width-line/2,a.y,a.x+a.width-line/2,a.y+a.height,a.theme.line,line);else{auto y=a.top?a.y+a.height-line/2:a.y+line/2;svg.line(a.x,y,a.x+a.width,y,a.theme.line,line);}svg.close();}
    if(a.label){svg.open("label");for(std::size_t i=0;i<a.tick_labels.size();++i){if(a.left)svg.text(a.tick_labels[i],a.x+a.width-c.label_padding-tick-line,a.at(i),c.label_size,a.theme.label,"end","middle");else{double x=a.at(i),y=a.top?a.y+(a.title?a.title_height+2*c.title_padding:0)+c.label_padding:a.y+c.label_padding+tick+line;if(!a.top){auto r=c.rotation*std::numbers::pi/180;x+=std::sin(r)*a.label_height/2;y+=std::abs(std::sin(r)*a.label_width/2);}svg.text(a.tick_labels[i],x,y,c.label_size,a.theme.label,"middle","text-before-edge",a.top?0:c.rotation);}}svg.close();}
    if(a.tick){svg.open("ticks");for(std::size_t i=0;i<a.tick_labels.size();++i){double v=a.at(i);if(a.left)svg.line(a.x+a.width-line,v,a.x+a.width-line-tick,v,a.theme.tick,c.tick_width);else{auto y=a.top?a.y+a.height-line:a.y+line;svg.line(v,y,v,y+(a.top?-tick:tick),a.theme.tick,c.tick_width);}}svg.close();}
    if(a.title){svg.open("title");if(a.left)svg.text(a.data.title,a.x+c.title_padding,a.y+a.height/2,c.title_size,a.theme.title,"middle","text-before-edge",270);else svg.text(a.data.title,a.x+a.width/2,a.top?a.y+c.title_padding:a.y+a.height-c.title_padding-a.title_height,c.title_size,a.theme.title);svg.close();}svg.close();
}
}
std::expected<XYSvg,std::string> render_xy_svg(const XYRenderInput& in,const XYMeasurements& measurements){try{
    const auto& d=in.document;const auto& c=in.config;const auto& t=in.theme;auto requests=xy_measurement_requests(in);if(measurements.text.size()!=requests.size())throw std::runtime_error("XY needs measured title, axis and legend text");for(auto m:measurements.text)if(!std::isfinite(m.width)||!std::isfinite(m.height)||m.width<0||m.height<0)throw std::runtime_error("Invalid XY text measurement");
    Axis xa{d.x,c.x,t.x},ya{d.y,c.y,t.y};xa.left=c.horizontal;ya.left=!c.horizontal;ya.top=c.horizontal;xa.tick_labels=labels(d.x,xa.left);ya.tick_labels=labels(d.y,ya.left);
    std::size_t mi=3;auto maximum=[&](std::size_t n){XYTextBox box;while(n--){auto m=measurements.text[mi++];box.width=std::max(box.width,m.width);box.height=std::max(box.height,m.height);}return box;};auto xm=maximum(xa.tick_labels.size()),ym=maximum(ya.tick_labels.size());
    double cw=std::floor(c.width*c.reserved/100),ch=std::floor(c.height*c.reserved/100),aw=c.width-cw,ah=c.height-ch;
    double title_h=0;if(c.show_title&&!d.title.empty()&&measurements.text[0].height+2*c.title_padding<=ah)title_h=measurements.text[0].height+2*c.title_padding;ah-=title_h;
    xa.space(aw,ah,xm,measurements.text[1]);if(c.horizontal)aw-=xa.width;else ah-=xa.height;
    ya.space(aw,ah,ym,measurements.text[2]);if(c.horizontal)ah-=ya.height;else aw-=ya.width;
    double px=c.horizontal?xa.width:ya.width,py=title_h+(c.horizontal?ya.height:0),legend_w=0,legend_h=0;std::vector<std::size_t> legend;
    double legend_text_width=0;for(std::size_t i=0;i<d.plots.size();++i)if(!d.plots[i].title.empty()){legend_text_width=std::max(legend_text_width,measurements.text[mi++].width);if(c.show_legend)legend.push_back(i);}
    if(!legend.empty()){legend_w=2*c.legend_padding+c.legend_size*1.1+legend_text_width;legend_h=2*c.legend_padding+legend.size()*c.legend_size+(legend.size()-1)*c.legend_size*.5;if(legend_w>aw||legend_h>ch){legend.clear();legend_w=legend_h=0;}}aw-=legend_w;cw+=std::max(aw,0.0);ch+=std::max(ah,0.0);
    if(c.horizontal){xa.start=py;xa.end=py+ch;xa.y=py;xa.height=ch;ya.start=px;ya.end=px+cw;ya.x=px;ya.y=title_h;ya.width=cw;}
    else{xa.start=px;xa.end=px+cw;xa.x=px;xa.y=py+ch;xa.width=cw;ya.start=py;ya.end=py+ch;ya.y=py;ya.height=ch;}
    if(std::ranges::any_of(d.plots,[](const auto& p){return p.bar;})){auto need=.7*xa.distance()/2;if(need>xa.padding)xa.padding=std::floor(need);}
    Svg svg;svg.s="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""+num(c.width)+"\" height=\""+num(c.height)+"\" viewBox=\"0 0 "+num(c.width)+" "+num(c.height)+"\" style=\"font-family:"+attr(t.font_family)+";font-size:16px\" role=\"img\">";if(!d.acc_title.empty())svg.s+="<title>"+xml(d.acc_title)+"</title>";if(!d.acc_description.empty())svg.s+="<desc>"+xml(d.acc_description)+"</desc>";svg.open("main");svg.s+="<rect class=\"background\" width=\""+num(c.width)+"\" height=\""+num(c.height)+"\" fill=\""+attr(t.background)+"\"/>";
    if(title_h){auto half=std::min(measurements.text[0].width,c.width)/2;auto center=std::min(std::max(px+cw/2,half),c.width-half);svg.open("chart-title");svg.text(d.title,center,title_h/2,c.title_size,t.title,"middle","middle");svg.close();}
    svg.open("plot");for(std::size_t index=0;index<d.plots.size();++index){const auto& p=d.plots[index];const auto& color=t.colors[index%t.colors.size()];svg.open(std::string(p.bar?"bar-plot-":"line-plot-")+std::to_string(index));
        const auto bw=std::min(xa.padding*2,xa.distance())*.95;struct Rect{double x,y,w,h;std::u16string label;};std::vector<Rect> bars;std::string path;
        for(std::size_t i=0;i<p.points.size();++i){auto& point=p.points[i];auto x=xa.scale(point.x,point.category),y=ya.scale(point.y);if(p.bar){Rect r=c.horizontal?Rect{px,x-bw/2,y-px,bw,{}}:Rect{x-bw/2,y,bw,py+ch-y,{}};if(i<d.plots.front().points.size())r.label=utext(d.plots.front().points[i].y);svg.rect(r.x,r.y,r.w,r.h,color);bars.push_back(r);}else{auto round=[](double v){return std::round(v*1000)/1000;};path+=(i?"L":"M")+num(round(c.horizontal?y:x))+","+num(round(c.horizontal?x:y));}}
        if(!p.bar&&!path.empty()){if(p.points.size()==1)path+='Z';svg.path(path,color,2);bool any=std::ranges::any_of(p.points,[](const auto& v){return !v.label.empty();});if(any){svg.open("labels");for(const auto& point:p.points)if(!point.label.empty()){auto x=xa.scale(point.x,point.category),y=ya.scale(point.y);svg.text(point.label,c.horizontal?y+10:x,c.horizontal?x:y-10,12,color,c.horizontal?"start":"middle","middle");}svg.close();}}
        if(p.bar&&c.data_labels){double size=std::numeric_limits<double>::infinity();for(const auto& r:bars)if(r.w>0&&r.h>0){double n=c.horizontal?r.h*.7:r.w/(r.label.size()*.7);while(n>0&&(c.horizontal?n*r.label.size()*.7>r.w-10:n*r.label.size()*.7>r.w||10+n>r.h))--n;size=std::min(size,n);}size=std::floor(size);for(const auto& r:bars)if(r.w>0&&r.h>0){auto x=c.horizontal?r.x+r.w+(c.outside_labels?10:-10):r.x+r.w/2;auto y=c.horizontal?r.y+r.h/2:r.y+(c.outside_labels?-10:10);svg.data_text(r.label,x,y,size,t.data_label,c.horizontal?(c.outside_labels?"start":"end"):"middle",c.horizontal?"middle":c.outside_labels?"auto":"hanging");}}
        svg.close();}svg.close();draw_axis(svg,xa);draw_axis(svg,ya);
    if(!legend.empty()){const auto x=px+cw+c.legend_padding,y=py+std::max((ch-legend_h)/2,0.0)+c.legend_padding,marker=c.legend_size*.75;svg.open("legend");svg.open("markers");for(std::size_t i=0;i<legend.size();++i){const auto index=legend[i];auto ry=y+i*c.legend_size*1.5;const auto& color=t.colors[index%t.colors.size()];if(d.plots[index].bar)svg.rect(x,ry,marker,marker,color);else svg.line(x,ry+marker/2,x+marker,ry+marker/2,color,2);}svg.close();svg.open("label");for(std::size_t i=0;i<legend.size();++i)svg.text(d.plots[legend[i]].title,x+marker+c.legend_size*.35,y+i*c.legend_size*1.5+marker/2,c.legend_size,t.legend,"start","middle");svg.close();svg.close();}
    svg.close();svg.s+="</svg>";return XYSvg{std::move(svg.s),c.width,c.height};
}catch(const std::runtime_error& e){return std::unexpected(e.what());}}
}
