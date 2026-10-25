// Original Sequence calculateActorMargins/addActorRenderingData/boundMessage
// and classic participant/message SVG emission. Mermaid 12.2.0, MIT.
module folia.mermaid.sequence_svg;
import std;
import folia.mermaid.number;
import folia.mermaid.svg_text;

namespace folia::mermaid {
bool sequence_region_start(SequenceKind k) {
    return k==SequenceKind::loop_start||k==SequenceKind::opt_start||k==SequenceKind::alt_start||
        k==SequenceKind::par_start||k==SequenceKind::par_over_start||k==SequenceKind::critical_start||k==SequenceKind::break_start;
}
bool sequence_region_end(SequenceKind k) {
    return k==SequenceKind::loop_end||k==SequenceKind::opt_end||k==SequenceKind::alt_end||
        k==SequenceKind::par_end||k==SequenceKind::critical_end||k==SequenceKind::break_end;
}
bool sequence_region_section(SequenceKind k) {
    return k==SequenceKind::alt_else||k==SequenceKind::par_and||k==SequenceKind::critical_option;
}
namespace {
std::u16string_view region_name(SequenceKind k) {
    switch(k) {
    case SequenceKind::loop_start:return u"loop";
    case SequenceKind::opt_start:return u"opt";
    case SequenceKind::alt_start:return u"alt";
    case SequenceKind::critical_start:return u"critical";
    case SequenceKind::break_start:return u"break";
    default:return u"par";
    }
}
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
    return kind == SequenceKind::dotted || kind == SequenceKind::dotted_open || kind == SequenceKind::bidirectional_dotted || kind == SequenceKind::dotted_cross || kind == SequenceKind::dotted_point;
}
bool cross(SequenceKind kind) { return kind==SequenceKind::solid_cross||kind==SequenceKind::dotted_cross; }
bool point(SequenceKind kind) { return kind==SequenceKind::solid_point||kind==SequenceKind::dotted_point; }
bool open(SequenceKind kind) { return kind == SequenceKind::solid_open || kind == SequenceKind::dotted_open; }
bool both(SequenceKind kind) { return kind == SequenceKind::bidirectional_solid || kind == SequenceKind::bidirectional_dotted; }
double participant_height(std::u16string_view type,double width,double height,double label_height) {
    if(type==u"actor") return 65;
    if(type==u"boundary"||type==u"entity") return 44+label_height;
    if(type==u"control") return 44+2*label_height;
    if(type==u"database") return width/3+label_height;
    return height;
}
// The classic svgDraw actor routines mutate height after drawing the header.
// Footers use that height for labels while retaining each shape's own geometry.
std::string participant_svg(const SequenceActor& actor,const SequenceTextBox& label,
    const SequenceSvgTheme& t,const SequenceSvgConfig& c,double x,double width,double y,bool footer,
    const std::string& font) {
    const auto& type=actor.type;
    const bool human=type==u"actor",boundary=type==u"boundary",control=type==u"control",
        entity=type==u"entity",database=type==u"database",queue=type==u"queue",collections=type==u"collections";
    const auto height=footer?participant_height(type,width,c.height,c.label_box_height):c.height;
    const auto center=x+width/2;
    const auto translate=boundary?21:entity?(footer?22:6):0;
    const auto cls=std::string(human||boundary||control?"actor-man ":"actor ")+(footer?"actor-bottom":"actor-top");
    std::string svg="<g class=\""+cls+"\" name=\""+xml(actor.id)+"\" transform=\"translate(0,"+num(translate)+")\" fill=\""+
        paint(t.actor_fill)+"\" stroke=\""+paint(t.actor_border)+"\" stroke-width=\""+(human||boundary||control?"2":"1")+"\">";
    auto line=[&](double x1,double y1,double x2,double y2,std::string extra={}) {
        svg+="<line x1=\""+num(x1)+"\" y1=\""+num(y1)+"\" x2=\""+num(x2)+"\" y2=\""+num(y2)+"\""+extra+"/>";
    };
    auto circle=[&](double cy,double r) {svg+="<circle cx=\""+num(center)+"\" cy=\""+num(cy)+"\" r=\""+num(r)+"\"/>";};
    auto rect=[&](double rx,double ry,bool shadow) {
        svg+="<rect class=\"actor"+(shadow?std::string{}:std::string(footer?" actor-bottom":" actor-top"))+"\" name=\""+xml(actor.id)+
            "\" x=\""+num(rx)+"\" y=\""+num(ry)+"\" width=\""+num(width)+"\" height=\""+num(height)+"\" rx=\"3\" ry=\"3\"/>";
    };
    double label_x=center,label_y=y+height/2;
    if(human) {
        for(const auto& p:std::array<std::array<double,4>,4>{{{0,25,0,45},{-18,33,18,33},{-18,60,0,45},{0,45,16,60}}})
            line(center+p[0],y+p[1],center+p[2],y+p[3]);
        circle(y+10,15);label_y=y+67.5;
    } else if(boundary) {
        line(center-55,y+12,center-15,y+12);line(center-55,y+2,center-55,y+22);
        circle(y+12,22);label_y+=15;
    } else if(control) {
        svg+="<defs><marker id=\"sequence-control-"+xml(actor.id)+(footer?"-bottom":"-top")+"\" refX=\"11\" refY=\"5.8\" markerWidth=\"20\" markerHeight=\"28\" orient=\"172.5\" stroke-width=\"1.2\"><path d=\"M 14.4 5.6 L 7.2 10.4 L 8.8 5.6 L 7.2 0.8 Z\"/></marker></defs>";
        circle(y+32,22);
        svg+="<line marker-end=\"url(#sequence-control-"+xml(actor.id)+(footer?"-bottom":"-top")+")\" transform=\"translate("+num(center)+","+num(y+10)+")\"/>";
        label_y+=footer?27:34;
    } else if(entity) {
        const auto cy=y+(footer?10:25);circle(cy,22);line(center-22,cy+22,center+22,cy+22," stroke-width=\"2\"");
        label_y+=footer?15:30;
    } else if(database) {
        const auto w=width/3,rx=w/2,ry=rx/(2.5+w/50),h=w-2*ry;
        svg+="<path transform=\"translate("+num(w)+","+num(ry)+")\" d=\"M "+num(x)+","+num(y+ry)+
            " a "+num(rx)+","+num(ry)+" 0 0 0 "+num(w)+",0 a "+num(rx)+","+num(ry)+" 0 0 0 -"+num(w)+
            ",0 l 0,"+num(h)+" a "+num(rx)+","+num(ry)+" 0 0 0 "+num(w)+",0 l 0,-"+num(h)+"\"/>";
        label_y+=35;
    } else if(queue) {
        const auto ry=height/2,rx=ry/(2.5+height/50);
        const auto arc="M "+num(x)+","+num(y+ry)+" a "+num(rx)+","+num(ry)+" 0 0 0 0,"+num(height);
        svg+="<path transform=\"translate("+num(rx)+","+num(-ry)+")\" d=\""+arc+" h "+num(width-2*rx)+
            " a "+num(rx)+","+num(ry)+" 0 0 0 0,-"+num(height)+" Z\"/>";
        svg+="<path transform=\"translate("+num(width-rx)+","+num(-ry)+")\" d=\""+arc+"\"/>";
    } else {
        rect(x,y,false);
        if(collections) {rect(x-6,y+6,true);label_x-=6;label_y+=6;}
    }
    for(std::size_t j=0;j<label.lines.size();++j)
        svg+="<text class=\"actor\" x=\""+num(label_x)+"\" y=\""+num(label_y+t.font_size*(static_cast<double>(j)-(label.lines.size()-1)/2.0))+
            "\" text-anchor=\"middle\" dominant-baseline=\"central\" alignment-baseline=\"central\" fill=\""+paint(t.actor_text)+"\" stroke=\"none\" stroke-width=\"1\""+font+">"+xml(label.lines[j])+"</text>";
    return svg+"</g>";
}
}
std::expected<SequenceSvg,std::string> render_sequence_svg(
    const SequenceRenderInput& input, const SequenceMeasurements& measured) {
    try {
        const auto& doc = input.document; auto c = input.config; const auto& t = input.theme;
        auto messages=measured.messages;
        auto wrap=[&](std::u16string_view text,double width) {
            if(!measured.wrap) throw std::runtime_error("Sequence wrapping requires text measurement");
            auto result=measured.wrap(text,width);
            if(!result) throw std::runtime_error(result.error());
            return *result;
        };
        if (measured.actors.size() != doc.actors.size() || measured.messages.size() != doc.messages.size() ||
            measured.boxes.size() != doc.boxes.size())
            return std::unexpected("Sequence measurement counts do not match the document");
        if (doc.actors.empty()) return std::unexpected("Sequence SVG needs at least one participant");
        struct Actor {
            double x=0,width=0,margin=0,start_y=0;
            std::optional<double> stop_y;
            bool visible=true;
        };
        std::vector<Actor> actors(doc.actors.size());
        std::map<std::u16string,std::size_t,std::less<>> ids;
        std::vector<double> max_message(doc.actors.size(),0);
        for (std::size_t i=0;i<doc.actors.size();++i) {
            const auto& a=doc.actors[i]; const auto& box=measured.actors[i];
            if(a.type!=u"participant" && a.type!=u"actor" && a.type!=u"boundary" && a.type!=u"control" &&
                a.type!=u"entity" && a.type!=u"database" && a.type!=u"collections" && a.type!=u"queue")
                return std::unexpected("Unsupported Sequence participant type");
            if(!std::isfinite(box.width)||box.width<0||!std::isfinite(box.height)||box.height<0)
                return std::unexpected("Invalid Sequence actor measurement");
            ids.emplace(a.id,i); actors[i].width=a.wrap?c.width:std::max(c.width,box.width+2*c.wrap_padding);
            actors[i].visible=!c.hide_unused || std::ranges::any_of(doc.messages,[&](const auto& m){return m.from==a.id||m.to==a.id;});
            if(a.wrap) c.height=std::max(c.height,box.height);
        }
        for(std::size_t i=0;i<doc.messages.size();++i) {
            const auto& m=doc.messages[i]; const auto& box=measured.messages[i];
            if(m.type==SequenceKind::active_start || m.type==SequenceKind::active_end || m.type==SequenceKind::autonumber) continue;
            if(sequence_region_start(m.type)||sequence_region_end(m.type)||sequence_region_section(m.type)||
                m.type==SequenceKind::rect_start||m.type==SequenceKind::rect_end) continue;
            if(!(m.type==SequenceKind::note||m.type==SequenceKind::solid||m.type==SequenceKind::dotted||open(m.type)||both(m.type)||cross(m.type)||point(m.type)))
                return std::unexpected("Sequence extended arrows require the next SVG adapter");
            if(!std::isfinite(box.width)||box.width<0||!std::isfinite(box.height)||box.height<0)
                return std::unexpected("Invalid Sequence message measurement");
            auto from=ids.find(m.from),to=ids.find(m.to);
            if(from==ids.end()||to==ids.end()) return std::unexpected("Sequence message actor is missing");
            const auto a=from->second,b=to->second; const auto width=box.width+2*c.wrap_padding;
            if(m.type==SequenceKind::note) {
                if(m.placement==0 && b>0) max_message[b-1]=std::max(max_message[b-1],width);
                else if(m.placement==1 && b+1<actors.size()) max_message[a]=std::max(max_message[a],width);
                else if(m.placement==2) {
                    if(b>0) max_message[b-1]=std::max(max_message[b-1],width/2);
                    if(b+1<actors.size()) max_message[a]=std::max(max_message[a],width/2);
                }
            } else if(a==b) max_message[a]=std::max(max_message[a],width/2);
            else if(a+1==b||b+1==a) max_message[std::min(a,b)]=std::max(max_message[std::min(a,b)],width);
        }
        for(std::size_t i=0;i<actors.size();++i) {
            auto& a=actors[i];
            const auto next_half=i+1<actors.size()?actors[i+1].width/2:0;
            a.margin=std::max(c.actor_margin,max_message[i]+c.actor_margin-a.width/2-next_half);
        }
        struct Group { double x=0,width=0,margin=0; bool visible=false; SequenceTextBox label; };
        std::vector<Group> groups(doc.boxes.size());
        double group_text_height=0;
        for(std::size_t i=0;i<groups.size();++i) {
            auto& g=groups[i];const auto& source=doc.boxes[i];
            double total=8*c.box_margin-2*c.box_text_margin;
            for(const auto& id:source.actors) {
                const auto index=ids.at(id);const auto& actor=actors[index];
                total+=actor.width+(max_message[index]>0?actor.margin:0);
            }
            g.label=source.wrap?wrap(source.name,total-2*c.wrap_padding):measured.boxes[i];
            g.margin=c.box_text_margin+std::max(0.0,g.label.width+2*c.wrap_padding-total)/2;
            group_text_height=std::max(group_text_height,g.label.height);
        }
        double previous_right=0,previous_margin=0,actor_right=0;
        std::optional<std::size_t> previous_box;
        for(std::size_t i=0;i<actors.size();++i) {
            auto& a=actors[i];if(!a.visible) continue;
            const auto box=doc.actors[i].box;
            if(previous_box && previous_box!=box) previous_margin+=c.box_margin+groups[*previous_box].margin;
            if(box && previous_box!=box) {
                auto& g=groups[*box];g.x=previous_right+previous_margin;g.visible=true;
                previous_margin+=g.margin;
            }
            // The original margin adjustment treats event index zero as false.
            if(const auto created=doc.created.find(doc.actors[i].id);created!=doc.created.end() && created->second)
                previous_margin+=a.width/2;
            a.x=previous_right+previous_margin;previous_right=a.x+a.width;actor_right=previous_right;
            if(box) {auto& g=groups[*box];g.width=previous_right+g.margin-g.x;}
            previous_margin=a.margin;previous_box=box;
        }
        const auto header_y=groups.empty()?0:c.box_margin+group_text_height;
        for(auto& actor:actors) actor.start_y=header_y;
        double left=0,right=actor_right,y=header_y+(actor_right?c.height:0),bottom=y;
        std::string signals;
        // Anchors retain event order: nested bars overlay outer bars, while
        // messages are painted after the completed note/activation layer.
        std::vector<std::string> backgrounds;
        struct Activation { std::u16string actor; double x, y; std::size_t anchor; };
        std::vector<Activation> active;
        struct Section { double y; std::size_t message; };
        struct Region {
            double left=std::numeric_limits<double>::infinity(),right=-std::numeric_limits<double>::infinity();
            double top=0,bottom=0;
            std::size_t message=0;
            std::vector<Section> sections;
        };
        std::vector<Region> regions;
        std::string region_backgrounds;
        auto insert=[&](double x1,double y1,double x2,double y2) {
            left=std::min({left,x1,x2});right=std::max({right,x1,x2});bottom=std::max({bottom,y1,y2});
            // Original updateBounds applies decreasing margins through the
            // region stack, then updates active bars with the same counter.
            for(std::size_t j=0;j<regions.size();++j) {
                auto& r=regions[j]; const auto margin=(regions.size()-j)*c.box_margin;
                r.left=std::min(r.left,std::min(x1,x2)-margin);r.right=std::max(r.right,std::max(x1,x2)+margin);
                r.top=std::min(r.top,std::min(y1,y2)-margin);r.bottom=std::max(r.bottom,std::max(y1,y2)+margin);
                left=std::min(left,r.left);right=std::max(right,r.right);bottom=std::max(bottom,r.bottom);
            }
            for(std::size_t j=0;j<active.size();++j)
                active[j].y=std::min(active[j].y,std::min(y1,y2)+j*c.box_margin);
        };
        auto activation_bounds=[&](std::u16string_view id) {
            const auto& a=actors[ids.at(std::u16string{id})];
            std::pair<double,double> result{a.x+a.width/2-1,a.x+a.width/2+1};
            for(const auto& bar:active) if(bar.actor==id) {
                result.first=std::min(result.first,bar.x);
                result.second=std::max(result.second,bar.x+c.activation_width);
            }
            return result;
        };
        auto endpoints=[&](const SequenceMessage& m) {
            const auto [fl,fr]=activation_bounds(m.from);
            const auto [tl,tr]=activation_bounds(m.to);
            const bool forward=fl<=tl;
            auto start=forward?fr:fl,stop=forward?tl:tr;
            if(m.from==m.to) stop=start;
            else {
                if(m.activate && std::abs(tl-tr)<=2) stop+=(forward?-1:1)*(c.activation_width/2-1);
                if(!open(m.type)) stop+=forward?-3:3;
                if(both(m.type)) start+=forward?3:-3;
            }
            return std::pair{start,stop};
        };
        // calculateLoopBounds builds note/message models before laying out the
        // vertical events. Its widths drive control-label wrapping, independently
        // of the nested margins used by the eventual painted region bounds.
        struct LoopWidth {
            double from=std::numeric_limits<double>::max(),to=std::numeric_limits<double>::lowest(),width=0;
            std::vector<std::size_t> labels;
        };
        std::vector<LoopWidth> loops;
        std::vector<std::pair<double,double>> notes(doc.messages.size());
        for(std::size_t i=0;i<doc.messages.size();++i) {
            const auto& m=doc.messages[i];auto& box=messages[i];
            if(m.type==SequenceKind::autonumber) continue;
            if(sequence_region_start(m.type)) {loops.push_back(LoopWidth{.labels={i}});continue;}
            if(sequence_region_section(m.type)) {if(!m.message.empty()) loops.back().labels.push_back(i);continue;}
            if(sequence_region_end(m.type)) {
                const auto loop=loops.back();loops.pop_back();
                for(auto label:loop.labels) if(!doc.messages[label].message.empty())
                    messages[label]=wrap(u"["+doc.messages[label].message+u"]",loop.width-2*c.wrap_padding);
                continue;
            }
            if(m.type==SequenceKind::active_start) {
                const auto& a=actors[ids.at(m.from)];
                const auto depth=std::ranges::count(active,m.from,&Activation::actor);
                active.push_back({m.from,a.x+a.width/2+(static_cast<double>(depth)-1)*c.activation_width/2,0,0});
                continue;
            }
            if(m.type==SequenceKind::active_end) {
                auto it=std::find_if(active.rbegin(),active.rend(),[&](const auto& bar){return bar.actor==m.from;});
                if(it!=active.rend()) active.erase(std::next(it).base());
                continue;
            }
            if(m.type==SequenceKind::rect_start||m.type==SequenceKind::rect_end) continue;
            const auto& a=actors[ids.at(m.from)];const auto& b=actors[ids.at(m.to)];
            if(m.type==SequenceKind::note) {
                if(m.wrap) box=wrap(m.message,c.width);
                auto width=m.wrap?c.width:std::max(c.width,box.width+2*c.note_margin),x=a.x;
                if(m.placement==0||m.placement==1) {
                    width=m.wrap?std::max(c.width,box.width+(m.placement==0?2*c.note_margin:0)):
                        std::max((a.width+b.width)/2,box.width+2*c.note_margin);
                    x=m.placement==0?a.x-width+(a.width-c.actor_margin)/2:a.x+(a.width+c.actor_margin)/2;
                } else if(m.from==m.to) {
                    width=m.wrap?std::max(c.width,a.width):std::max(width,a.width);x=a.x+(a.width-width)/2;
                } else {
                    width=std::abs(a.x+a.width/2-b.x-b.width/2)+c.actor_margin;
                    x=std::min(a.x+a.width/2,b.x+b.width/2)-c.actor_margin/2;
                }
                notes[i]={x,width};
                if(m.wrap) box=wrap(m.message,width-2*c.wrap_padding);
                for(auto& loop:loops) {
                    loop.from=std::min(loop.from,x);loop.to=std::max(loop.to,x+width);
                    loop.width=std::max(loop.width,std::abs(loop.from-loop.to))-c.label_box_width;
                }
            } else {
                const auto [start,stop]=endpoints(m);
                const auto bounded=std::abs(start-stop)+2*c.wrap_padding;
                if(m.wrap) box=wrap(m.message,std::max(bounded,c.width));
                const auto width=std::max({m.wrap?0:box.width+2*c.wrap_padding,bounded,c.width});
                for(auto& loop:loops) {
                    if(start==stop) {
                        loop.from=std::min({loop.from,a.x-width/2,a.x-a.width/2});
                        loop.to=std::max({loop.to,b.x+width/2,b.x+a.width/2});
                        loop.width=std::max(loop.width,std::abs(loop.to-loop.from))-c.label_box_width;
                    } else {
                        loop.from=std::min(loop.from,start);loop.to=std::max(loop.to,stop);
                        loop.width=std::max(loop.width,width)-c.label_box_width;
                    }
                }
            }
        }
        active.clear();
        const auto font=" font-family=\""+xml(t.font_family)+"\" font-size=\""+num(t.font_size)+"\" font-weight=\""+std::to_string(t.font_weight)+"\"";
        double sequence_index=1,sequence_step=1;
        bool sequence_visible=false;
        for(std::size_t i=0;i<doc.messages.size();++i) {
            const auto& m=doc.messages[i]; const auto& box=messages[i];
            if(m.type==SequenceKind::autonumber) {
                if(m.number_start.value_or(0)!=0) sequence_index=*m.number_start;
                if(m.number_step.value_or(0)!=0) sequence_step=*m.number_step;
                sequence_visible=m.number_visible;
                continue;
            }
            if(sequence_region_start(m.type)||sequence_region_section(m.type)||m.type==SequenceKind::rect_start) {
                const bool section=sequence_region_section(m.type);
                const bool background=m.type==SequenceKind::rect_start;
                y+=c.box_margin+(section?c.box_text_margin:0);
                if(section) {
                    if(regions.empty()) return std::unexpected("Sequence section has no region");
                    regions.back().sections.push_back({y,i});
                } else regions.push_back(Region{.top=y,.message=i});
                y+=c.box_margin+(section||background?0:c.box_text_margin)+
                    (background||box.display.empty()?0:std::max(box.height,c.label_box_height));
                bottom=std::max(bottom,y);
                continue;
            }
            if(sequence_region_end(m.type)||m.type==SequenceKind::rect_end) {
                if(regions.empty()) return std::unexpected("Sequence region end has no start");
                const auto r=regions.back();regions.pop_back();
                if(!std::isfinite(r.left)||!std::isfinite(r.right)) return std::unexpected("Sequence region needs rendered content");
                if(m.type==SequenceKind::rect_end) {
                    const auto& fill=doc.messages[r.message].message;
                    // Original drawBackgroundRect lowers each completed rect;
                    // outer backgrounds therefore precede nested backgrounds.
                    region_backgrounds="<rect class=\"rect\" x=\""+num(r.left)+"\" y=\""+num(r.top)+"\" width=\""+num(r.right-r.left)+
                        "\" height=\""+num(r.bottom-r.top)+"\" fill=\""+(fill.empty()?paint(t.rect_fill):xml(fill))+"\"/>"+region_backgrounds;
                    y=r.bottom;bottom=std::max(bottom,y);
                    continue;
                }
                auto line=[&](double x1,double y1,double x2,double y2,bool section=false) {
                    signals+="<line class=\"loopLine\" x1=\""+num(x1)+"\" y1=\""+num(y1)+"\" x2=\""+num(x2)+"\" y2=\""+num(y2)+
                        "\" stroke-width=\"2\" stroke-dasharray=\""+(section?"3 3":"2 2")+"\" stroke=\""+paint(t.label_border)+"\" fill=\""+paint(t.label_border)+"\"/>";
                };
                line(r.left,r.top,r.right,r.top);line(r.right,r.top,r.right,r.bottom);
                line(r.left,r.bottom,r.right,r.bottom);line(r.left,r.top,r.left,r.bottom);
                for(const auto& s:r.sections) line(r.left,s.y,r.right,s.y,true);
                const auto w=std::max(c.label_box_width,50.0),h=c.label_box_height?c.label_box_height:20;
                signals+="<polygon class=\"labelBox\" points=\""+num(r.left)+","+num(r.top)+" "+num(r.left+w)+","+num(r.top)+" "+
                    num(r.left+w)+","+num(r.top+h-7)+" "+num(r.left+w-8.4)+","+num(r.top+h)+" "+num(r.left)+","+num(r.top+h)+
                    "\" stroke=\""+paint(t.label_border)+"\" fill=\""+paint(t.label_fill)+"\"/>";
                auto text=[&](std::string_view cls,std::u16string_view value,double x,double ty,bool label) {
                    signals+="<text class=\""+std::string(cls)+"\" x=\""+num(label?std::floor(x+0.5):x)+"\" y=\""+
                        num(std::floor(ty+c.box_text_margin/2+0.5))+"\" text-anchor=\"middle\" fill=\""+paint(label?t.label_text:t.loop_text)+"\""+font;
                    if(label) signals+=" dominant-baseline=\"middle\" alignment-baseline=\"middle\"";
                    signals+=">"+xml(value)+"</text>";
                };
                text("labelText",region_name(doc.messages[r.message].type),r.left+w/2,r.top+h/2,true);
                const auto center=(r.left+r.right)/2;
                auto label_lines=[&](std::string_view cls,const SequenceTextBox& label,double x,double ty) {
                    for(std::size_t j=0;j<label.lines.size();++j)
                        text(cls,label.lines[j].empty()?u"\u200b":std::u16string_view{label.lines[j]},x,ty+j*label.line_height,false);
                };
                label_lines("loopText",messages[r.message],center+c.label_box_width/2,r.top+c.box_margin+c.box_text_margin);
                for(const auto& s:r.sections) if(!messages[s.message].display.empty())
                    label_lines("sectionTitle",messages[s.message],center,s.y+c.box_margin+c.box_text_margin);
                y=r.bottom;bottom=std::max(bottom,y);
                continue;
            }
            if(m.type==SequenceKind::active_start) {
                const auto& a=actors[ids.at(m.from)];
                const auto depth=std::ranges::count(active,m.from,&Activation::actor);
                active.push_back({m.from,a.x+a.width/2+(static_cast<double>(depth)-1)*c.activation_width/2,y+2,backgrounds.size()});
                backgrounds.emplace_back();
                continue;
            }
            if(m.type==SequenceKind::active_end) {
                auto it=std::find_if(active.rbegin(),active.rend(),[&](const auto& bar){return bar.actor==m.from;});
                if(it==active.rend()) return std::unexpected("Sequence activation end has no start");
                auto bar=*it;active.erase(std::next(it).base());
                auto end=y;
                if(bar.y+18>end) {bar.y=end-6;end+=12;}
                backgrounds[bar.anchor]="<rect class=\"activation"+std::to_string(std::ranges::count(active,m.from,&Activation::actor)%3)+
                    "\" x=\""+num(bar.x)+"\" y=\""+num(bar.y)+"\" width=\""+num(c.activation_width)+"\" height=\""+num(end-bar.y)+
                    "\" fill=\""+paint(t.activation_fill)+"\" stroke=\""+paint(t.activation_border)+"\"/>";
                insert(bar.x,end-10,bar.x+c.activation_width,end);
                continue;
            }
            const auto& a=actors[ids.at(m.from)];const auto& b=actors[ids.at(m.to)];
            if(m.type==SequenceKind::note) {
                const auto [x,width]=notes[i];
                y+=c.box_margin;
                const auto h=box.height+2*c.note_margin;
                const auto anchor=c.note_align==u"left"?"start":c.note_align==u"right"?"end":"middle";
                const auto tx=std::floor((c.note_align==u"left"?x+c.note_margin:c.note_align==u"right"?x+width-c.note_margin:x+width/2)+0.5);
                auto note="<rect class=\"note\" x=\""+num(x)+"\" y=\""+num(y)+"\" width=\""+num(width)+"\" height=\""+num(h)+
                    "\" fill=\""+paint(t.note_fill)+"\" stroke=\""+paint(t.note_border)+"\"/>";
                for(std::size_t j=0;j<box.lines.size();++j)
                    note+="<text class=\"noteText\" x=\""+num(tx)+"\" y=\""+num(std::floor(y+c.note_margin/2+j*box.line_height+0.5))+"\" text-anchor=\""+anchor+
                    "\" dominant-baseline=\"middle\" alignment-baseline=\"middle\" dy=\"1em\" fill=\""+paint(t.note_text)+"\""+font+">"+xml(box.lines[j].empty()?u"\u200b":std::u16string_view{box.lines[j]})+"</text>";
                backgrounds.push_back(std::move(note));
                insert(x,y,x+width,y+h);y+=h;
                continue;
            }
            const auto [from_left,from_right]=activation_bounds(m.from);
            const auto [to_left,to_right]=activation_bounds(m.to);
            const bool self=m.from==m.to;
            auto [start,stop]=endpoints(m);
            const auto start_y=y;
            const auto text_y=std::floor(y+10+c.wrap_padding/2+0.5);
            // Original boundMessage: one measured line, then the remaining
            // text offset and the family-specific self-loop/box margin.
            y+=10+box.line_height;
            auto offset=box.height-10;
            if(!self||!c.right_angles) offset+=c.box_margin;
            const auto line_y=y+offset;
            if(self) {
                offset+=30;
                const auto dx=std::max(box.width/2,c.width/2);
                insert(start-dx,y-10+offset,stop+dx,y+30+offset);
            } else insert(start,line_y-10,stop,line_y);
            y+=offset;bottom=std::max(bottom,y);
            insert(std::min(from_left,to_left),start_y,std::max(from_right,to_right),y);
            // Lifecycle adjustment follows boundMessage and precedes final text/line drawing.
            auto at_event=[&](const auto& events,const auto& id) {
                const auto found=events.find(id);return found!=events.end() && found->second==i;
            };
            const bool creation=at_event(doc.created,m.to);
            const bool destroy_sender=!creation && at_event(doc.destroyed,m.from);
            const bool destruction=destroy_sender || (!creation && at_event(doc.destroyed,m.to));
            if(creation || destruction) {
                const auto id=destroy_sender?m.from:m.to;
                auto& actor=actors[ids.at(id)];
                const auto& type=doc.actors[ids.at(id)].type;
                const bool icon=type==u"actor"||type==u"control"||type==u"entity"||type==u"database";
                if(creation || c.mirror_actors) {
                    const auto adjustment=(icon?18:actor.width/2)+(destroy_sender?0:3);
                    // These bounds retain the original pre-adjustment message model.
                    if(destroy_sender) {
                        if(actor.x < b.x) {insert(start-adjustment,start_y,stop,y-10+c.height/2+c.note_margin);start+=adjustment;}
                        else {insert(stop,start_y,start+adjustment,y-10+c.height/2+c.note_margin);start-=adjustment;}
                    } else {
                        if(actor.x < a.x) {insert(stop-adjustment,start_y,start,y-10+c.height/2+c.note_margin);stop+=adjustment;}
                        else {insert(start,start_y,stop+adjustment,y-10+c.height/2+c.note_margin);stop-=adjustment;}
                    }
                }
                if(creation) actor.start_y=line_y-c.height/2;
                else actor.stop_y=line_y-c.height/2;
                y+=c.height/2;bottom=std::max(bottom,y);
            }
            for(std::size_t j=0;j<box.lines.size();++j)
                signals+="<text class=\"messageText\" x=\""+num(std::floor((start+stop)/2+0.5))+"\" y=\""+num(text_y+j*box.line_height)+
                "\" text-anchor=\"middle\" dominant-baseline=\"middle\" alignment-baseline=\"middle\" dy=\"1em\" fill=\""+paint(t.signal_text)+"\""+font+">"+xml(box.lines[j].empty()?u"\u200b":std::u16string_view{box.lines[j]})+"</text>";
            const auto cls=dotted(m.type)?"messageLine1":"messageLine0";
            const bool numbered=sequence_visible||c.show_numbers;
            if(start==stop) {
                const auto line_start=start+(numbered&&both(m.type)?10:0);
                std::string d="M "+num(line_start)+","+num(line_y);
                if(c.right_angles) d+=" H "+num(start+std::max(c.width/2,box.width/2))+" V "+num(line_y+25)+" H "+num(start);
                else d+=" C "+num(line_start+60)+","+num(line_y-10)+" "+num(start+60)+","+num(line_y+30)+" "+num(start)+","+num(line_y+20);
                signals+="<path d=\""+d+"\"";
            } else signals+="<line x1=\""+num(start+(numbered?(both(m.type)?(start<stop?12:-6):6):0))+"\" y1=\""+num(line_y)+"\" x2=\""+num(stop)+"\" y2=\""+num(line_y)+"\"";
            signals+=" class=\""+std::string(cls)+"\" stroke=\""+paint(t.signal)+"\" stroke-width=\"1.5\" fill=\"none\"";
            if(dotted(m.type)) signals+=" stroke-dasharray=\"3 3\"";
            if(!open(m.type)) signals+=" marker-end=\"url(#sequence-"+std::string(cross(m.type)?"crosshead":point(m.type)?"filled-head":"arrowhead")+")\"";
            if(both(m.type)) signals+=" marker-start=\"url(#sequence-arrowhead)\"";
            signals+="/>";
            if(numbered) {
                const auto x=self||start<=stop?std::min(from_left,to_left)+1:std::max(from_right,to_right)-1;
                const auto value=num(sequence_index);
                signals+="<circle class=\"sequence-number-disc\" cx=\""+num(x)+"\" cy=\""+num(line_y)+"\" r=\"6\" fill=\""+paint(t.signal)+"\"/>";
                signals+="<text class=\"sequenceNumber\" x=\""+num(x)+"\" y=\""+num(line_y+4)+"\" font-family=\"Arial\" font-size=\""+
                    (value.size()>5?"7":value.size()>3?"9":"12")+"\" text-anchor=\"middle\" fill=\""+paint(t.sequence_number)+"\">"+value+"</text>";
            }
            sequence_index=std::floor((sequence_index+sequence_step)*100+0.5)/100;
        }
        const auto footer_y=y+2*c.box_margin;
        double footer_height=0;
        for(std::size_t i=0;i<actors.size();++i) if(actors[i].visible)
            footer_height=std::max(footer_height,participant_height(doc.actors[i].type,actors[i].width,c.height,c.label_box_height));
        if(c.mirror_actors) bottom=std::max(bottom,footer_y+footer_height+c.box_margin);
        std::string group_backgrounds;
        // drawBox lowers each complete group behind the diagram, in reverse order.
        for(std::size_t i=0;i<groups.size();++i) {
            const auto& g=groups[i];if(!g.visible) continue;
            right=std::max(right,g.x+g.width);left=std::min(left,g.x);
            std::string layer="<rect class=\"rect\" x=\""+num(g.x-2*c.box_margin)+"\" y=\""+num(-c.box_margin/2)+
                "\" width=\""+num(g.width+4*c.box_margin)+"\" height=\""+num((c.mirror_actors?footer_y+footer_height+c.box_margin:y)+1.5*c.box_margin)+
                "\" fill=\""+xml(doc.boxes[i].fill)+"\" stroke=\""+paint(t.actor_border)+"\"/>";
            if(!g.label.display.empty()) for(std::size_t j=0;j<g.label.lines.size();++j)
                layer+="<text class=\"text\" x=\""+num(g.x+g.width/2)+"\" y=\""+
                    num(c.box_text_margin+group_text_height/2+t.font_size*(static_cast<double>(j)-(g.label.lines.size()-1)/2.0))+
                    "\" text-anchor=\"middle\" dominant-baseline=\"central\" alignment-baseline=\"central\" fill=\""+paint(t.title_text)+"\""+font+">"+xml(g.label.lines[j])+"</text>";
            group_backgrounds=layer+group_backgrounds;
        }
        if(!groups.empty()) bottom+=c.box_margin;
        const auto width=right-left+2*c.margin_x;
        const auto title_height=doc.title.empty()?0:40;
        const auto height=bottom+2*c.margin_y+(c.mirror_actors?-c.box_margin+c.bottom_margin:0)+title_height;
        std::string svg="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\""+num(width)+"\" height=\""+num(height)+"\" viewBox=\""+
            num(left-c.margin_x)+" "+num(-c.margin_y-title_height)+" "+num(width)+" "+num(height)+"\" role=\"img\">";
        if(!doc.acc_title.empty()) svg+="<title>"+xml(doc.acc_title)+"</title>";
        if(!doc.acc_description.empty()) svg+="<desc>"+xml(doc.acc_description)+"</desc>";
        svg+="<defs><marker id=\"sequence-arrowhead\" refX=\"7.9\" refY=\"5\" markerUnits=\"userSpaceOnUse\" markerWidth=\"12\" markerHeight=\"12\" orient=\"auto-start-reverse\"><path d=\"M -1 0 L 10 5 L 0 10 z\" fill=\""+paint(t.signal)+"\" stroke=\""+paint(t.signal)+"\"/></marker></defs>";
        svg+="<defs><marker id=\"sequence-crosshead\" markerWidth=\"15\" markerHeight=\"8\" orient=\"auto\" refX=\"4\" refY=\"4.5\"><path d=\"M 1,2 L 6,7 M 6,2 L 1,7\" fill=\""+paint(t.signal)+"\" stroke=\""+paint(t.signal)+"\" stroke-width=\"1pt\" stroke-dasharray=\"0, 0\"/></marker>";
        svg+="<marker id=\"sequence-filled-head\" refX=\"15.5\" refY=\"7\" markerWidth=\"20\" markerHeight=\"28\" orient=\"auto\"><path d=\"M 18,7 L9,13 L14,7 L9,1 Z\" fill=\""+paint(t.title_text)+"\"/></marker></defs>";
        svg+=group_backgrounds+region_backgrounds;
        for(std::size_t i=0;i<actors.size();++i) {
            const auto& a=actors[i];const auto center=a.x+a.width/2;
            if(!a.visible) continue;
            const auto& type=doc.actors[i].type;
            const auto start=type==u"actor"||type==u"boundary"?80:type==u"control"||type==u"entity"?75:
                type==u"database"?c.height+2*c.box_text_margin:c.height;
            svg+="<line class=\"actor-line\" x1=\""+num(center)+"\" x2=\""+num(center)+"\" y1=\""+num(a.start_y+start)+"\" y2=\""+num(c.mirror_actors?a.stop_y.value_or(footer_y):a.stop_y?*a.stop_y+participant_height(type,a.width,c.height,c.label_box_height)/2:2000)+"\" stroke=\""+paint(t.actor_line)+"\" fill=\""+paint(t.actor_fill)+"\" stroke-width=\"2\"/>";
            svg+=participant_svg(doc.actors[i],measured.actors[i],t,c,a.x,a.width,a.start_y,false,font);
            if(c.mirror_actors) svg+=participant_svg(doc.actors[i],measured.actors[i],t,c,a.x,a.width,a.stop_y.value_or(footer_y),true,font);
        }

        for(const auto& layer:backgrounds) svg+=layer;
        svg+=signals;
        if(!doc.title.empty()) svg+="<text x=\""+num((right-left)/2-2*c.margin_x)+"\" y=\"-25\" fill=\""+paint(t.title_text)+"\""+font+">"+xml(doc.title)+"</text>";
        svg+="</svg>";
        return SequenceSvg{std::move(svg),width,height};
    } catch(const std::exception& error) { return std::unexpected(std::string{error.what()}); }
}
}
