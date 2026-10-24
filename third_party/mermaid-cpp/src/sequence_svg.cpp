// Original Sequence calculateActorMargins/addActorRenderingData/boundMessage
// and classic participant/message SVG emission. Mermaid 12.2.0, MIT.
module folia.mermaid.sequence_svg;
import std;
import folia.mermaid.number;
import folia.mermaid.svg_text;

namespace folia::mermaid {
namespace {
std::string num(double value) {
    auto result = format_number(value);
    if (!result) throw std::runtime_error("Invalid Sequence geometry");
    return *result;
}
std::string xml(std::u16string_view value) {
    auto result = escape_svg_text(value,u"Sequence text");
    if (!result) throw std::runtime_error("Invalid Sequence SVG text");
    return *result;
}
std::string paint(std::string_view value) {
    // Configured theme paints use the same checked XML boundary as labels.
    return xml(std::u16string{value.begin(),value.end()});
}
bool dotted(SequenceKind kind) {
    return kind == SequenceKind::dotted || kind == SequenceKind::dotted_open || kind == SequenceKind::bidirectional_dotted;
}
bool open(SequenceKind kind) { return kind == SequenceKind::solid_open || kind == SequenceKind::dotted_open; }
bool both(SequenceKind kind) { return kind == SequenceKind::bidirectional_solid || kind == SequenceKind::bidirectional_dotted; }
}
std::expected<SequenceSvg,std::string> render_sequence_svg(
    const SequenceRenderInput& input, const SequenceMeasurements& measured) {
    try {
        const auto& doc = input.document; const auto& c = input.config; const auto& t = input.theme;
        if (measured.actors.size() != doc.actors.size() || measured.messages.size() != doc.messages.size())
            return std::unexpected("Sequence measurement counts do not match the document");
        if (doc.actors.empty()) return std::unexpected("Sequence SVG needs at least one participant");
        struct Actor { double x = 0, width = 0, margin = 0; };
        std::vector<Actor> actors(doc.actors.size());
        std::map<std::u16string,std::size_t,std::less<>> ids;
        std::vector<double> max_message(doc.actors.size(),0);
        for (std::size_t i=0;i<doc.actors.size();++i) {
            const auto& a=doc.actors[i]; const auto& box=measured.actors[i];
            if(a.type!=u"participant" || a.wrap) return std::unexpected("Sequence actor shapes/wrapping require the next SVG adapter");
            if(!std::isfinite(box.width)||box.width<0||!std::isfinite(box.height)||box.height<0)
                return std::unexpected("Invalid Sequence actor measurement");
            ids.emplace(a.id,i); actors[i].width=std::max(c.width,box.width+2*c.wrap_padding);
        }
        for(std::size_t i=0;i<doc.messages.size();++i) {
            const auto& m=doc.messages[i]; const auto& box=measured.messages[i];
            if(m.wrap || m.activate || !(m.type==SequenceKind::solid||m.type==SequenceKind::dotted||open(m.type)||both(m.type)))
                return std::unexpected("Sequence notes/activation/extended arrows require the next SVG adapter");
            if(!std::isfinite(box.width)||box.width<0||!std::isfinite(box.height)||box.height<0)
                return std::unexpected("Invalid Sequence message measurement");
            auto from=ids.find(m.from),to=ids.find(m.to);
            if(from==ids.end()||to==ids.end()) return std::unexpected("Sequence message actor is missing");
            const auto a=from->second,b=to->second; const auto width=box.width+2*c.wrap_padding;
            if(a==b) max_message[a]=std::max(max_message[a],width/2);
            else if(a+1==b||b+1==a) max_message[std::min(a,b)]=std::max(max_message[std::min(a,b)],width);
        }
        for(std::size_t i=0;i<actors.size();++i) {
            auto& a=actors[i];
            const auto next_half=i+1<actors.size()?actors[i+1].width/2:0;
            a.margin=std::max(c.actor_margin,max_message[i]+c.actor_margin-a.width/2-next_half);
            if(i) a.x=actors[i-1].x+actors[i-1].width+actors[i-1].margin;
        }
        double left=0,right=actors.back().x+actors.back().width,y=c.height,bottom=y;
        std::string signals;
        const auto font=" font-family=\""+xml(t.font_family)+"\" font-size=\""+num(t.font_size)+"\" font-weight=\""+std::to_string(t.font_weight)+"\"";
        for(std::size_t i=0;i<doc.messages.size();++i) {
            const auto& m=doc.messages[i]; const auto& box=measured.messages[i];
            const auto& a=actors[ids.at(m.from)];const auto& b=actors[ids.at(m.to)];
            const bool forward=a.x<=b.x,self=m.from==m.to;
            auto start=a.x+a.width/2+(forward?1:-1),stop=b.x+b.width/2+(forward?-1:1);
            if(self) stop=start;
            else {
                if(!open(m.type)) stop+=forward?-3:3;
                if(both(m.type)) start+=forward?3:-3;
            }
            const auto text_y=std::floor(y+10+c.wrap_padding/2+0.5);
            signals+="<text class=\"messageText\" x=\""+num(std::floor((start+stop)/2+0.5))+"\" y=\""+num(text_y)+
                "\" text-anchor=\"middle\" dominant-baseline=\"middle\" alignment-baseline=\"middle\" dy=\"1em\" fill=\""+paint(t.signal_text)+"\""+font+">"+xml(box.display)+"</text>";
            // Original boundMessage: one measured line, then the remaining
            // text offset and the family-specific self-loop/box margin.
            y+=10+box.height;
            auto offset=box.height-10;
            if(!self||!c.right_angles) offset+=c.box_margin;
            const auto line_y=y+offset;
            if(self) {
                offset+=30;
                const auto dx=std::max(box.width/2,c.width/2);
                left=std::min(left,start-dx);right=std::max(right,stop+dx);
                bottom=std::max(bottom,y+30+offset);
            }
            y+=offset;bottom=std::max(bottom,y);
            const auto cls=dotted(m.type)?"messageLine1":"messageLine0";
            if(self) {
                std::string d="M "+num(start)+","+num(line_y);
                if(c.right_angles) d+=" H "+num(start+std::max(c.width/2,box.width/2))+" V "+num(line_y+25)+" H "+num(start);
                else d+=" C "+num(start+60)+","+num(line_y-10)+" "+num(start+60)+","+num(line_y+30)+" "+num(start)+","+num(line_y+20);
                signals+="<path d=\""+d+"\"";
            } else signals+="<line x1=\""+num(start)+"\" y1=\""+num(line_y)+"\" x2=\""+num(stop)+"\" y2=\""+num(line_y)+"\"";
            signals+=" class=\""+std::string(cls)+"\" stroke=\""+paint(t.signal)+"\" stroke-width=\"2\" fill=\"none\"";
            if(dotted(m.type)) signals+=" stroke-dasharray=\"3 3\"";
            if(!open(m.type)) signals+=" marker-end=\"url(#sequence-arrowhead)\"";
            if(both(m.type)) signals+=" marker-start=\"url(#sequence-arrowhead)\"";
            signals+="/>";
        }
        const auto footer_y=y+2*c.box_margin;
        bottom=std::max(bottom,footer_y+c.height+c.box_margin);
        const auto width=right-left+2*c.margin_x;
        const auto height=bottom+2*c.margin_y-c.box_margin+c.bottom_margin;
        std::string svg="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""+num(width)+"\" height=\""+num(height)+"\" viewBox=\""+
            num(left-c.margin_x)+" "+num(-c.margin_y)+" "+num(width)+" "+num(height)+"\" role=\"img\">";
        if(!doc.acc_title.empty()) svg+="<title>"+xml(doc.acc_title)+"</title>";
        if(!doc.acc_description.empty()) svg+="<desc>"+xml(doc.acc_description)+"</desc>";
        svg+="<defs><marker id=\"sequence-arrowhead\" refX=\"7.9\" refY=\"5\" markerUnits=\"userSpaceOnUse\" markerWidth=\"12\" markerHeight=\"12\" orient=\"auto-start-reverse\"><path d=\"M -1 0 L 10 5 L 0 10 z\" fill=\""+paint(t.signal)+"\" stroke=\""+paint(t.signal)+"\"/></marker></defs>";
        for(std::size_t i=0;i<actors.size();++i) {
            const auto& a=actors[i];const auto center=a.x+a.width/2;
            svg+="<line class=\"actor-line\" x1=\""+num(center)+"\" x2=\""+num(center)+"\" y1=\""+num(c.height)+"\" y2=\""+num(footer_y)+"\" stroke=\""+paint(t.actor_line)+"\" stroke-width=\"2\"/>";
            for(const auto actor_y:{0.0,footer_y}) {
                svg+="<rect class=\"actor "+std::string(actor_y==0?"actor-top":"actor-bottom")+"\" name=\""+xml(doc.actors[i].id)+"\" x=\""+num(a.x)+"\" y=\""+num(actor_y)+"\" width=\""+num(a.width)+"\" height=\""+num(c.height)+"\" rx=\"3\" ry=\"3\" fill=\""+paint(t.actor_fill)+"\" stroke=\""+paint(t.actor_border)+"\"/>";
                svg+="<text class=\"actor actor-box\" x=\""+num(center)+"\" y=\""+num(actor_y+c.height/2)+"\" text-anchor=\"middle\" dominant-baseline=\"central\" alignment-baseline=\"central\" fill=\""+paint(t.actor_text)+"\""+font+">"+xml(measured.actors[i].display)+"</text>";
            }
        }
        svg+=signals+"</svg>";
        return SequenceSvg{std::move(svg),width,height};
    } catch(const std::exception& error) { return std::unexpected(std::string{error.what()}); }
}
}
