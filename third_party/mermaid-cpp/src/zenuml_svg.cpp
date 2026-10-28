module folia.mermaid.zenuml_svg;
import std;
import folia.mermaid.number;
import folia.mermaid.svg_text;
namespace folia::mermaid {
namespace {
#include "zenuml_assets.inc"
std::string num(double v){auto n=format_number(v);if(!n)throw std::runtime_error("Invalid ZenUML geometry");return *n;}
std::string esc(std::u16string_view s){auto e=escape_svg_text(s,u"ZenUML text");if(!e)throw std::runtime_error(e.error());return *e;}
std::string attr(std::string_view key,double n){return " "+std::string(key)+"=\""+num(n)+"\"";}
std::string rect(double x,double y,double w,double h,std::string_view cls,double radius=0){return "<rect"+attr("x",x)+attr("y",y)+attr("width",w)+attr("height",h)+attr("rx",radius)+" class=\""+std::string(cls)+"\"/>";}
std::string text(double x,double y,std::u16string_view label,std::string_view cls,std::string_view anchor="start",bool central=false,double opacity=1){return "<text"+(opacity==1?std::string{}:attr("opacity",opacity))+attr("x",x)+attr("y",y)+" text-anchor=\""+std::string(anchor)+"\" class=\""+std::string(cls)+"\""+(central?" dominant-baseline=\"central\"":"")+">"+esc(label)+"</text>";}
std::string line(double x,double y,double x2,double y2,std::string_view cls,std::string_view extra=""){return "<line"+attr("x1",x)+attr("y1",y)+attr("x2",x2)+attr("y2",y2)+" class=\""+std::string(cls)+"\""+std::string(extra)+"/>";}
std::string icon(double x,double y,std::string_view value){return "<g class=\"participant-icon\" transform=\"translate("+num(x)+","+num(y)+")\" fill=\"none\">"+std::string(value)+"</g>";}
std::u16string number(std::size_t n){auto s=std::to_string(n);return {s.begin(),s.end()};}
std::string arrow(double x,double y,bool left,bool open){return "<svg"+attr("x",left?x:x-7)+attr("y",y-5)+" width=\"7\" height=\"10\" viewBox=\"0 0 7 9\" overflow=\"visible\" class=\"arrow-head"+(open?" arrow-open":"")+"\"><g"+(left?" transform=\"scale(-1, 1) translate(-7, 0)\"":"")+"><path d=\"M1 1.25 L6.15 4.5 L1 7.75"+(open?"":" Z")+"\" stroke=\"#000\" stroke-linecap=\"round\" stroke-width=\"2\" fill=\""+(open?"none":"#000")+"\"/></g></svg>";}
std::string open_arrow(double x,double y,bool left,std::string_view cls){double a=x+(left?5.15:-5.15);return "<polyline points=\""+num(a)+","+num(y-3.25)+" "+num(x)+","+num(y)+" "+num(a)+","+num(y+3.25)+"\" fill=\"none\" stroke-linecap=\"round\" class=\""+std::string(cls)+"\"/>";}
struct Painter {
 const ZenDocument& doc;const ZenLayout& layout;const ZenSvgMeasure& measure;
 std::vector<int> depth;std::vector<double> py,pw;double extra{},border{},maxReturn{};
 std::string occurrences,messages,selfs,creations,returns,fragments;
 std::size_t index(std::u16string_view s)const{for(std::size_t i=0;i<doc.participants.size();++i)if(doc.participants[i].name==s)return i;throw std::runtime_error("Unknown ZenUML participant");}
 const ZenCoordinate& coordinate(const ZenStatement& s)const{for(const auto& c:layout.vertical)if(c.begin==s.begin)return c;throw std::runtime_error("Missing ZenUML coordinate");}
 void ret(double from,double to,double y,const std::u16string& label,const std::u16string& seq,bool self){
  if(self){returns+="<g class=\"return return-self\"><g transform=\"translate("+num(from+4)+","+num(y-12)+") scale("+num(12.0/512)+")\">"+std::string(zen_return)+"</g>"+text(from+20,y-1,label,"return-label")+"</g>";return;}
  y=std::floor(y);returns+="<g class=\"return\">"+line(from,y,to,y,"return-line")+open_arrow(to,y,to<from,"return-arrow")+text((from+to)/2+(to<from?3.5:-3.5),y-3,label,"return-label","middle")+text(std::min(from,to)-4,y-3,seq,"seq-number","end")+"</g>";maxReturn=std::max(maxReturn,y+46);
 }
 void walk(const std::vector<ZenStatement>& list,std::u16string prefix={},std::size_t start=0,int nesting=0){
  for(std::size_t ordinal=0;ordinal<list.size();++ordinal){const auto& s=list[ordinal];const auto& c=coordinate(s);auto seq=prefix+number(start+ordinal+1);
   if(s.kind==ZenKind::loop||s.kind==ZenKind::alternative){
    std::size_t lo=doc.participants.size(),hi=0;double selfextra=0;std::function<void(const std::vector<ZenStatement>&)> members=[&](const auto& v){for(const auto& k:v){if(k.kind!=ZenKind::loop&&k.kind!=ZenKind::alternative){auto a=index(k.from),b=index(k.to);lo=std::min(lo,std::min(a,b));hi=std::max(hi,std::max(a,b));}members(k.children);members(k.otherwise);}};members(s.children);members(s.otherwise);if(lo==doc.participants.size())throw std::runtime_error("Empty ZenUML fragment");
    std::function<void(const std::vector<ZenStatement>&)> widths=[&](const auto& v){for(const auto& k:v){if(k.from==k.to&&k.kind!=ZenKind::loop&&k.kind!=ZenKind::alternative){auto i=index(k.to);selfextra=std::max(selfextra,std::floor(measure(k.text,16,false)+.5)-(layout.positions[hi]-layout.positions[i])-layout.widths[hi]/2);}widths(k.children);widths(k.otherwise);}};widths(s.children);widths(s.otherwise);
    double x=layout.positions[lo]-layout.widths[lo]/2-10,w=std::max(100.0,layout.positions[hi]-layout.positions[lo]+(layout.widths[lo]+layout.widths[hi])/2)+20+selfextra;extra=std::max(extra,selfextra);border=std::max(border,10.0*(nesting+1));
    fragments+="<g class=\"fragment\">"+rect(x+.5,c.top+.5,w-1,c.height-1,"fragment-border",4)+rect(x+1,c.top+1,w-2,25,"fragment-header");
    fragments+="<svg"+attr("x",x+5)+attr("y",c.top+1)+" width=\"20\" height=\"24\" viewBox=\""+(s.kind==ZenKind::loop?"0 0 1024 1024\" fill=\"#000\" stroke=\"none\">"+std::string(zen_loop):std::string("0 0 24 24\" fill=\"none\"><path d=\"M12 8L20 12L12 16L4 12L12 8Z\" stroke=\"#000\" stroke-width=\"1.5\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>"))+"</svg>";
    fragments+=text(x+27,c.top+13,s.kind==ZenKind::loop?u"Loop":u"Alt","fragment-label","start",true)+text(x-3,c.top+9,seq,"seq-number","end",true);
    if(!s.condition.empty()){auto y=c.top+41;auto width=measure(s.condition,14,false);fragments+=text(x+1,y,u"[","fragment-condition")+text(x+1+3.89+4,y,s.condition,"fragment-condition","start",false,.65)+text(x+1+3.89+8+width,y,u"]","fragment-condition");}
    if(!s.otherwise.empty()){double y=coordinate(s.otherwise.front()).top-37;fragments+=line(x+1,y+.5,x+w-1,y+.5,"fragment-separator")+text(x+5,y+16,u"[else]","fragment-section-label");}fragments+="</g>";
    walk(s.children,seq+u".",0,nesting+1);walk(s.otherwise,seq+u".",s.children.size(),nesting+1);continue;
   }
   auto a=index(s.from),b=index(s.to);double from=layout.positions[a],to=layout.positions[b];bool self=a==b;auto label=s.label.empty()?s.text:s.label;
   if(s.kind==ZenKind::returned){ret(self?from+depth[a]*7+1:from-7,self?to:to+(depth[b]?depth[b]*7+2:1),c.top+(self?15.5:16.5),label,seq,self);continue;}
   double sender=from;if(depth[a]&&!self)sender+=from<to?depth[a]*7:(depth[a]-2)*7;
   if(s.kind==ZenKind::creation){py[b]=c.top;double y=c.top+20,edge=to+(to<sender?pw[b]/2:-pw[b]/2),begin=sender+(sender<to?1:0);std::string labelContent=esc(label);if(label.size()>2&&label.front()==u'\u00ab'&&label!=u"\u00abcreate\u00bb")labelContent="<tspan>\xc2\xab</tspan><tspan dx=\"4\">"+esc(std::u16string_view(label).substr(1,label.size()-2))+"</tspan><tspan dx=\"4\">\xc2\xbb</tspan>";
    creations+="<g class=\"creation\">"+line(begin,y,edge,y,"message-line"," stroke-dasharray=\"6,4\"")+open_arrow(edge,y,to<sender,"arrow-head arrow-open")+"<text"+attr("x",(begin+edge)/2+(to<sender?3.5:-3))+attr("y",y-3)+" text-anchor=\"middle\" class=\"message-label\">"+labelContent+"</text>"+text(std::min(begin,edge)-4,y-3,seq,"seq-number","end")+"</g>";
    occurrences+=rect(to-6,c.top+39,13,std::max(c.height-38,24.0)-2,"occurrence");
   }else{
    double y=c.top+(self?30:16)-.5,offset=depth[b]*7;
    if(self){double x=from+(depth[a]?depth[a]*7:0);selfs+="<g class=\"message self-call\"><svg"+attr("x",x+1)+attr("y",c.top+14)+" width=\"30\" height=\"24\"><path d=\"M0,2 L26,2 Q28,2 28,4 L28,13 Q28,15 26,15 L14,15\" fill=\"none\" stroke=\"#000\" stroke-width=\"2\"/><g transform=\"translate(7, 10)\"><svg height=\"10\" width=\"7\" viewBox=\"0 0 7 9\"><g transform=\"scale(-1, 1) translate(-7, 0)\"><path d=\"M1 1.25 L6.15 4.5 L1 7.75 Z\" stroke=\"#000\" stroke-linecap=\"round\" fill=\"#000\" stroke-width=\"2\"/></g></svg></g></svg>"+text(x+6,c.top+12,label,"message-label")+text(x-3,c.top+12,seq,"seq-number","end")+"</g>";extra=std::max(extra,std::floor(measure(label,16,false)+.5)-(layout.positions.back()-to)-layout.widths.back()/2);
    }else{double end=to;if(s.kind==ZenKind::sync)end+=sender<to?-7+offset:7+offset;bool right=sender<end;double x1=sender+(right?1:0),x2=end+(right?0:1);messages+="<g class=\"message\">"+line(x1,y-.5,x2,y-.5,"message-line")+arrow(x2,y-.5,!right,s.kind==ZenKind::async)+text((sender+end)/2+(right?-3.5:3.5)+.5,y-3.5,label,"message-label","middle")+text(std::min(x1,x2)-4,y-3.5,seq,"seq-number","end")+"</g>";}
    if(s.kind==ZenKind::sync){double h=c.height-(self?30:16)+2,x=to-7+offset;occurrences+=rect(x+1,y-.5,13,h-2,"occurrence");if(!s.assignment.empty()&&!self)ret(to-6+offset,sender+(depth[a]?2:2),y-1.5+h-2,s.assignment,seq+u"."+number(s.children.size()+1),false);}
   }
   if(s.kind==ZenKind::sync||s.kind==ZenKind::creation){++depth[b];walk(s.children,seq+u".",0,nesting);--depth[b];}
  }
 }
};
}
std::expected<ZenSvg,std::string> render_zenuml_svg(const ZenDocument& doc,const ZenSvgMeasure& measure){
 if(!measure)return std::unexpected("ZenUML requires real text measurements");try{
 auto layout=layout_zenuml(doc,[&](auto t){return measure(t,16,false);});if(!layout)return std::unexpected(layout.error());if(doc.participants.empty())return ZenSvg{"<svg></svg>",0,0};Painter p{doc,*layout,measure};p.depth.resize(doc.participants.size());p.py.assign(doc.participants.size(),28);
 for(const auto& v:doc.participants){auto label=v.label.empty()?v.name:v.label;p.pw.push_back(v.name==u"_STARTER_"?80:std::clamp(measure(label,16,false)+16+(v.type.empty()?0:28),80.0,250.0));}p.walk(doc.statements);
 double height=std::max(layout->height+28,p.maxReturn),width=layout->width+p.extra,viewWidth=width+22+2*p.border,viewHeight=height+47;std::string life,parts;
 for(std::size_t i=0;i<doc.participants.size();++i){auto& v=doc.participants[i];double x=layout->positions[i],y=p.py[i],w=p.pw[i];life+="<g class=\"lifeline\">"+line(x+.5,y+40,x+.5,height+12,"lifeline"," stroke-dasharray=\"5,5\" stroke-dashoffset=\"5\" shape-rendering=\"crispEdges\"")+"</g>";parts+="<g class=\"participant\">"+rect(x-w/2+1,y+1,w-2,38,"participant-box",3);if(v.name==u"_STARTER_")parts+=icon(x-14,y+8,zen_actor);else{auto label=v.label.empty()?v.name:v.label;if(!v.type.empty()){double group=x-(44+measure(label,16,false))/2;parts+=icon(group+4,y+8,v.type==u"Actor"?zen_actor:zen_database)+text(group+36,y+19.75,label,"participant-label","start",true);}else parts+=text(x,y+19.75,label,"participant-label","middle",true);}parts+="</g>";}
 std::string frame="<rect x=\"0\" y=\"0\""+attr("width",viewWidth)+attr("height",viewHeight)+" rx=\"4\" fill=\"#666\"/><rect x=\"1\" y=\"1\""+attr("width",viewWidth-2)+attr("height",viewHeight-2)+" rx=\"3\" fill=\"#fff\"/>"+line(1,33.5,viewWidth-1,33.5,"frame-header-line");if(!doc.title.empty())frame+=text(5,16.75,doc.title,"frame-title","start",true);
 auto svg="<svg xmlns=\"http://www.w3.org/2000/svg\""+attr("width",viewWidth)+attr("height",viewHeight)+" viewBox=\"0 0 "+num(viewWidth)+" "+num(viewHeight)+"\"><defs><style>"+std::string(zen_styles)+"</style></defs>"+frame+"<g transform=\"translate("+num(11+p.border)+",34)\">"+life+parts+p.occurrences+p.messages+p.selfs+p.creations+p.returns+p.fragments+"</g></svg>";return ZenSvg{std::move(svg),viewWidth,viewHeight};
 }catch(const std::exception& e){return std::unexpected(e.what());}
}
}
