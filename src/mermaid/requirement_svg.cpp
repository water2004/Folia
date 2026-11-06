module folia.mermaid.requirement_svg;
import std;
import folia.mermaid.flowchart_elk;
import folia.mermaid.number;
import folia.mermaid.svg_text;

namespace folia::mermaid {
namespace {
std::string num(double v) {
    auto n=format_number(v);
    if(!n) throw std::runtime_error("Invalid Requirement geometry");
    return *n;
}
std::string xml(std::u16string_view s) {
    auto escaped=escape_svg_text(s,u"Requirement text");
    if(!escaped) throw std::runtime_error("Invalid Requirement SVG text");
    std::string out;
    for(char c:*escaped) {
        if(c=='"') out+="&quot;";
        else if(c=='\'') out+="&apos;";
        else out+=c;
    }
    return out;
}
std::string attr(std::string_view key,std::string_view value) {
    std::string escaped;
    for(char c:value) {
        if(c=='&') escaped+="&amp;";
        else if(c=='<') escaped+="&lt;";
        else if(c=='"') escaped+="&quot;";
        else if(c=='\'') escaped+="&apos;";
        else escaped+=c;
    }
    return " "+std::string(key)+"=\""+escaped+"\"";
}
std::string attr(std::string_view key,std::u16string_view value) {
    return " "+std::string(key)+"=\""+xml(value)+"\"";
}
FlowchartPoint mix(FlowchartPoint a,FlowchartPoint b,double wa,double wb,double d) {
    return {(wa*a.x+wb*b.x)/d,(wa*a.y+wb*b.y)/d};
}
std::string point(FlowchartPoint p) { return num(p.x)+","+num(p.y); }
// The unified Mermaid renderer uses d3-shape curveBasis for its Dagre routes.
std::string curve(std::span<const FlowchartPoint> points) {
    if(points.empty()) return {};
    std::string path="M"+point(points.front());
    if(points.size()<3) {
        for(std::size_t i=1;i<points.size();++i) path+="L"+point(points[i]);
        return path;
    }
    path+="L"+point(mix(points[0],points[1],5,1,6));
    const auto segment=[&](FlowchartPoint a,FlowchartPoint b,FlowchartPoint c) {
        path+="C"+point(mix(a,b,2,1,3))+" "+point(mix(a,b,1,2,3))+" "+
            point({(a.x+4*b.x+c.x)/6,(a.y+4*b.y+c.y)/6});
    };
    for(std::size_t i=2;i<points.size();++i) segment(points[i-2],points[i-1],points[i]);
    segment(points[points.size()-2],points.back(),points.back());
    return path+"L"+point(points.back());
}
FlowchartPoint intersect(const FlowchartPlacedNode& node,std::u16string_view,FlowchartPoint p) {
    const double dx=p.x-node.x,dy=p.y-node.y;
    const double scale=std::max(2*std::abs(dx)/node.width,2*std::abs(dy)/node.height);
    return scale>0?FlowchartPoint{node.x+dx/scale,node.y+dy/scale}:p;
}
#include "flowchart_elk_paint.inc"

struct FieldLine {
    FlowchartLabelLine runs;
    RequirementTextMetrics metrics;
};
struct Field {
    std::u16string text;
    std::vector<FieldLine> lines;
    FlowchartLabelFont font;
    RequirementTextMetrics metrics;
    double width=0,height=0,offset=0,line_step=0;
    bool center_rows=false;
};
struct NodePaint {
    std::vector<Field> fields;
    std::u16string classes=u"node";
    std::map<std::u16string,std::u16string,std::less<>> styles;
    double width=0,height=0,divider=0;
    bool has_divider=false;
};
struct Bounds {
    double left=std::numeric_limits<double>::infinity(),top=left,right=-left,bottom=-left;

    void include(double x,double y,double width=0,double height=0) {
        left=std::min(left,x);top=std::min(top,y);
        right=std::max(right,x+width);bottom=std::max(bottom,y+height);
    }
};
// createFormattedText's SVG edge-label background extends past the glyph box.
constexpr double svg_edge_label_padding=2;
bool finite_metrics(const RequirementTextMetrics& m) {
    return std::isfinite(m.advance)&&std::isfinite(m.ink_width)&&
        std::isfinite(m.bounds_width)&&std::isfinite(m.svg_height)&&
        std::isfinite(m.svg_baseline)&&m.advance>=0&&m.ink_width>=0&&
        m.bounds_width>=0&&m.svg_height>=0&&m.svg_baseline>=0;
}
std::u16string_view trim(std::u16string_view s) {
    const auto start=s.find_first_not_of(u" \t\r\n");
    return start==s.npos?std::u16string_view{}:s.substr(start,s.find_last_not_of(u" \t\r\n")-start+1);
}
std::u16string field_text(std::u16string_view prefix,std::u16string_view value) {
    if(value.empty()) return {};
    return std::u16string(prefix)+std::u16string(value);
}
RequirementTextMetrics measure_line(const RequirementSvgMeasure& measure,
    std::u16string_view text,const FlowchartLabelFont& font,bool svg,
    std::span<const RequirementTextRun> runs={}) {
    auto metrics=measure(text,font.family,font.size,font.weight,font.italic,runs,svg);
    if(!metrics) throw std::runtime_error(metrics.error());
    if(!finite_metrics(*metrics)) throw std::runtime_error("Requirement text measurement is invalid");
    return *metrics;
}
unsigned run_weight(const FlowchartLabelFont& font,bool strong,bool svg) {
    if(!strong) return font.weight;
    if(svg) return std::max(font.weight,700u);
    return font.weight<350?400u:font.weight<550?700u:900u;
}
RequirementTextMetrics measure_runs(const RequirementSvgMeasure& measure,
    const FlowchartLabelLine& runs,const FlowchartLabelFont& font,bool svg) {
    std::u16string text;
    std::vector<RequirementTextRun> styles;
    for(const auto& run:runs) {
        styles.push_back({text.size(),run.text.size(),run_weight(font,run.strong,svg),
            font.italic||run.emphasis});
        text+=run.text;
    }
    return measure_line(measure,text,font,svg,styles);
}
// createText uses HTML tokens as literal SVG words. Keep explicit breaks
// separate, and encode tag brackets before the shared inline parser runs.
std::u16string label_source(std::u16string_view text,bool html) {
    std::u16string source;
    for(std::size_t i=0;i<text.size();) {
        const auto tail=text.substr(i);
        const bool starts_tag=tail.size()>1&&tail.front()==u'<'&&
            (tail[1]==u'/'||(tail[1]>=u'a'&&tail[1]<=u'z')||(tail[1]>=u'A'&&tail[1]<=u'Z'));
        if(starts_tag&&!html) {
            const auto end=tail.find(u'>');
            if(end!=tail.npos) {
                auto tag=tail.substr(1,end-1);
                auto name=trim(tag);
                if(name.ends_with(u"/")) name=trim(name.substr(0,name.size()-1));
                if(name.size()==2&&(name[0]==u'b'||name[0]==u'B')&&
                    (name[1]==u'r'||name[1]==u'R')) {
                    if(source.empty()||source.back()!=u'\n') source+=u'\n';
                } else {
                    source+=u" &lt;";
                    source+=tag;
                    source+=u"&gt; ";
                }
                i+=end+1;
                continue;
            }
        }
        if(tail.front()==u'<'&&!starts_tag) source+=u"&lt;";
        else source+=tail.front();
        ++i;
    }
    return source;
}
void append_run(FlowchartLabelLine& line,std::u16string_view text,const FlowchartLabelRun& style) {
    if(text.empty()) return;
    if(line.empty()||line.back().strong!=style.strong||line.back().emphasis!=style.emphasis)
        line.push_back({{},style.strong,style.emphasis});
    line.back().text+=text;
}
double wrapping_limit(const RequirementSvgMeasure& measure,std::u16string_view raw,
    const FlowchartLabelFont& font) {
    struct Dimensions {double width=0,height=0,line_height=0;};
    auto dimensions=[&](const FlowchartLabelFont& selected) {
        Dimensions result;
        std::size_t start=0;
        auto include=[&](std::u16string_view line) {
            const auto metrics=measure_line(measure,line.empty()?u"\u200b":line,selected,true);
            result.width=std::max(result.width,std::round(metrics.bounds_width));
            result.height+=std::round(metrics.svg_height);
            result.line_height=std::max(result.line_height,std::round(metrics.svg_height));
        };
        for(auto i=raw.find(u'<');i!=raw.npos;i=raw.find(u'<',i+1)) {
            const auto end=raw.find(u'>',i+1);
            if(end==raw.npos) break;
            auto tag=trim(raw.substr(i+1,end-i-1));
            if(tag.starts_with(u"/")) tag.remove_prefix(1);
            if(tag.ends_with(u"/")) tag=trim(tag.substr(0,tag.size()-1));
            if(tag.size()!=2||(tag[0]!=u'b'&&tag[0]!=u'B')||
                (tag[1]!=u'r'&&tag[1]!=u'R')) continue;
            include(raw.substr(start,i-start));
            start=end+1;
            i=end;
        }
        include(raw.substr(start));
        return result;
    };
    auto fallback=font;
    fallback.family=u"sans-serif";
    const auto generic=dimensions(fallback);
    const auto preferred=dimensions(font);
    const bool use_generic=generic.width>preferred.width&&generic.height>preferred.height&&
        generic.line_height>preferred.line_height;
    return (use_generic?generic.width:preferred.width)+50;
}
std::vector<FieldLine> wrap_runs(const RequirementSvgMeasure& measure,FlowchartLabelLine runs,
    const FlowchartLabelFont& font,bool svg,double limit) {
    auto natural=measure_runs(measure,runs,font,svg);
    if(natural.advance<=limit) return {{std::move(runs),natural}};
    struct Word {FlowchartLabelLine runs; bool preceded=false; FlowchartLabelRun space_style;};
    std::vector<Word> words(1);
    for(const auto& run:runs) for(const auto ch:run.text) {
        if(ch==u' ') {
            if(!words.back().runs.empty()) words.emplace_back();
            words.back().preceded=true;
            words.back().space_style={u"",run.strong,run.emphasis};
        } else append_run(words.back().runs,std::u16string_view(&ch,1),run);
    }
    std::vector<FieldLine> result;
    FlowchartLabelLine line;
    auto flush=[&](const FlowchartLabelRun* trailing_space=nullptr) {
        if(line.empty()) return;
        auto metrics=measure_runs(measure,line,font,svg);
        // The HTML label uses break-spaces and keeps the wrapping space on the
        // preceding line. Its measured wrapping width still excludes that space.
        if(trailing_space) append_run(line,u" ",*trailing_space);
        result.push_back({std::move(line),metrics});
        line.clear();
    };
    for(const auto& word:words) {
        if(word.runs.empty()) continue;
        auto candidate=line;
        if(!candidate.empty()&&word.preceded) append_run(candidate,u" ",word.space_style);
        for(const auto& run:word.runs) append_run(candidate,run.text,run);
        if(!line.empty()&&measure_runs(measure,candidate,font,svg).advance>limit) {
            flush(!svg&&word.preceded?&word.space_style:nullptr);
            candidate=word.runs;
        }
        if(svg&&measure_runs(measure,candidate,font,svg).advance>limit) {
            // A single overlong SVG word is split by measured character spans.
            for(const auto& run:candidate) for(std::size_t start=0;start<run.text.size();) {
                const auto first=run.text[start];
                const auto count=first>=0xd800&&first<=0xdbff&&start+1<run.text.size()?2u:1u;
                const auto scalar=std::u16string_view(run.text).substr(start,count);
                auto attempt=line;
                append_run(attempt,scalar,run);
                if(!line.empty()&&measure_runs(measure,attempt,font,svg).advance>limit) {
                    flush();
                    append_run(line,scalar,run);
                } else line=std::move(attempt);
                start+=count;
            }
        } else line=std::move(candidate);
    }
    flush();
    return result;
}
Field measure_field(const RequirementSvgMeasure& measure,std::u16string text,
    FlowchartLabelFont font,const FlowchartLabelFont& base_font,bool html,bool center_rows,
    bool rich) {
    Field result;
    result.text=std::move(text);
    result.font=std::move(font);
    result.center_rows=html&&center_rows;
    const auto& base=result.font;
    result.line_step=base.size*(html?1.5:1.1);
    result.metrics=measure_line(measure,result.text,base,!html);
    if(!rich) {
        result.width=html?result.metrics.advance:result.metrics.bounds_width;
        result.height=html?result.line_step:result.metrics.svg_height;
        result.lines.push_back({{{result.text,false,false}},result.metrics});
        return result;
    }
    auto normalized=label_source(result.text,html);
    auto parsed=html?flowchart_label_runs(normalized,u"markdown"):
        flowchart_svg_label_runs(normalized,u"markdown");
    if(!parsed) throw std::runtime_error(parsed.error());
    // requirementBox computes the generous cap from the original unformatted
    // input at the base font before createText applies label styles.
    const auto limit=wrapping_limit(measure,result.text,base_font);
    bool wrapped=false;
    for(auto& line:*parsed) {
        auto parts=wrap_runs(measure,std::move(line),base,!html,limit);
        wrapped=wrapped||parts.size()>1;
        for(auto& part:parts) {
            result.width=std::max(result.width,html?part.metrics.advance:part.metrics.bounds_width);
            result.lines.push_back(std::move(part));
        }
    }
    if(html&&wrapped) result.width=std::max(result.width,limit);
    const auto line_count=std::max<std::size_t>(1,result.lines.size());
    result.height=html?result.line_step*line_count:
        result.metrics.svg_height+result.line_step*(line_count-1);
    return result;
}
std::string paint_attr(const NodePaint& paint,std::u16string_view key,std::string_view attribute,
    std::string_view fallback) {
    const auto style=paint.styles.find(key);
    return style==paint.styles.end()?attr(attribute,fallback):attr(attribute,style->second);
}
std::string paint_text(const Field& field,FlowchartPoint at,std::string_view color,
    bool html,std::string_view background={}) {
    const auto weight=std::to_string(field.font.weight);
    const auto font_style=field.font.italic?"italic":"normal";
    std::string out;
    if(!background.empty()) {
        const auto pad=html?0:svg_edge_label_padding;
        out="<rect class=\"background\""+attr("x",num(at.x-pad))+attr("y",num(at.y-pad))+
            attr("width",num(field.width+2*pad))+attr("height",num(field.height+2*pad))+
            attr("fill",background)+"/>";
    }
    const auto x=at.x+(field.center_rows?field.width/2:0);
    const auto baseline=at.y+field.metrics.svg_baseline+
        (html?(field.line_step-field.metrics.svg_height)/2:0);
    out+="<text xml:space=\"preserve\""+attr("x",num(x))+attr("y",num(baseline))+
        attr("font-family",field.font.family)+attr("font-size",num(field.font.size))+
        attr("font-weight",weight)+attr("font-style",font_style)+attr("fill",color)+
        (field.center_rows?" text-anchor=\"middle\"":"")+(html?" data-html-label=\"true\"":"")+">";
    for(std::size_t i=0;i<field.lines.size();++i) {
        out+="<tspan"+attr("x",num(x))+attr("y",num(baseline+i*field.line_step))+">";
        for(const auto& run:field.lines[i].runs) {
            out+="<tspan"+attr("font-weight",std::to_string(run_weight(field.font,run.strong,!html)))+
                attr("font-style",field.font.italic||run.emphasis?"italic":"normal")+
                ">"+xml(run.text)+"</tspan>";
        }
        out+="</tspan>";
    }
    return out+"</text>";
}
}

std::expected<RequirementSvg,std::string> render_requirement_svg(
    const RequirementRenderInput& input,const RequirementSvgMeasure& measure) {
    if(!measure) return std::unexpected("Requirement rendering requires real text measurements");
    const bool elk=input.config.layout==u"elk";
    if(!elk&&input.config.layout!=u"dagre")
        return std::unexpected("Requirement SVG requires Dagre or ELK layout");
    try {
        const auto& doc=input.document;
        const auto& theme=input.theme;
        const auto& config=input.config;
        const auto node_count=doc.requirements.size()+doc.elements.size();
        if(node_count>10000 || doc.relationships.size()>500)
            return std::unexpected("Requirement diagram exceeds layout limits");

        FlowchartInput graph;
        graph.direction=doc.direction;
        graph.node_spacing=config.node_spacing;
        graph.rank_spacing=config.rank_spacing;
        std::vector<NodePaint> paints;
        paints.reserve(node_count);
        std::vector<FlowchartNodeBox> boxes;
        boxes.reserve(node_count);
        const FlowchartLabelFont base_font{theme.font_family,theme.font_size};
        const FlowchartSvgTheme label_theme{.font_family=theme.font_family,.font_size=theme.font_size};
        const auto add_node=[&](const auto& node,std::u16string type,std::initializer_list<std::u16string> body) {
            NodePaint paint;
            for(const auto& name:node.classes) paint.classes+=u" "+name;
            for(std::u16string_view declaration:node.css_styles) {
                const auto colon=declaration.find(u':');
                if(colon!=declaration.npos)
                    paint.styles[std::u16string(trim(declaration.substr(0,colon)))]=trim(declaration.substr(colon+1));
            }
            double offset=0;
            Bounds unplaced;
            auto font=flowchart_label_font({},node.css_styles,label_theme);
            if(!font) throw std::runtime_error(font.error());
            const auto add_field=[&](std::u16string text,bool bold=false,bool heading=false,bool rich=true) {
                if(text.empty()) return;
                auto field_font=*font;
                if(bold) field_font.weight=std::max(field_font.weight,700u);
                auto field=measure_field(measure,std::move(text),std::move(field_font),base_font,
                    config.html_labels,heading||!elk,rich);
                const auto line_height=field.height+(config.html_labels?0:6);
                field.offset=offset-line_height/2;
                const double glyph_top=config.html_labels?0:field.font.size-field.metrics.svg_baseline;
                unplaced.include(-field.width/2,field.offset+glyph_top,field.width,field.height);
                offset+=line_height;
                paint.fields.push_back(std::move(field));
            };
            add_field(u"<<"+type+u">>",false,true,false);
            add_field(node.name,true,true);
            offset+=theme.gap;
            paint.divider=offset;
            for(auto value=body.begin();value!=body.end();++value) {
                // The final field is positioned without advancing the upstream divider cursor.
                if(value+1==body.end()) paint.has_divider=offset>paint.divider;
                add_field(*value);
            }
            paint.width=unplaced.right-unplaced.left+theme.padding;
            paint.height=unplaced.bottom-unplaced.top+theme.padding;
            graph.nodes.push_back({.id=node.name,.shape=u"rect"});
            boxes.push_back({paint.width,paint.height});
            paints.push_back(std::move(paint));
        };
        for(const auto& r:doc.requirements)
            add_node(r,r.type,{field_text(u"ID: ",r.requirement_id),field_text(u"Text: ",r.text),
                field_text(u"Risk: ",r.risk),field_text(u"Verification: ",r.verify_method)});
        for(const auto& e:doc.elements)
            add_node(e,u"Element",{field_text(u"Type: ",e.type),field_text(u"Doc Ref: ",e.doc_ref)});

        std::vector<FlowchartEdgeLabelSize> edge_boxes;
        std::vector<Field> edge_texts;
        edge_boxes.reserve(doc.relationships.size());
        edge_texts.reserve(doc.relationships.size());
        const double edge_padding=config.html_labels?0:svg_edge_label_padding;
        for(std::size_t i=0;i<doc.relationships.size();++i) {
            const auto& relation=doc.relationships[i];
            auto label=measure_field(measure,u"<<"+relation.type+u">>",base_font,base_font,
                config.html_labels,true,false);
            const auto edge_id=std::to_string(i);
            graph.edges.push_back({.id=relation.source+u"-"+relation.target+u"-"+
                std::u16string(edge_id.begin(),edge_id.end()),.start=relation.source,.end=relation.target,
                .label=label.text,.arrow_start=u"none",.arrow_end=u"none"});
            edge_boxes.push_back({label.width+2*edge_padding,label.height+2*edge_padding});
            edge_texts.push_back(std::move(label));
        }

        FlowchartLayout layout;
        if(!graph.nodes.empty()||!graph.edges.empty()) {
            FlowchartMeasurements measured;
            measured.nodes.resize(node_count);
            if(elk) {
                auto scene=layout_measured_elk_scene(graph,boxes,edge_boxes,measured,{});
                if(!scene) return std::unexpected(scene.error());
                layout=std::move(scene->layout);
                std::unordered_map<std::u16string_view,std::size_t> ids;
                for(std::size_t i=0;i<graph.nodes.size();++i) ids.emplace(graph.nodes[i].id,i);
                for(std::size_t i=0;i<graph.edges.size();++i) {
                    const auto& edge=graph.edges[i];
                    clip_elk_node_endpoints(layout.edges[i].points,layout.nodes[ids.at(edge.start)],
                        layout.nodes[ids.at(edge.end)],u"rect",u"rect");
                }
            } else {
                auto scene=layout_measured_flowchart(graph,boxes,edge_boxes,measured,{});
                if(!scene) return std::unexpected(scene.error());
                layout=std::move(scene->layout);
            }
        }

        const double pad=config.diagram_padding;
        const double offset=elk?0:8; // Dagre's unified graph margin, before viewport padding.
        Bounds bounds;
        std::string paths,labels,nodes;
        for(std::size_t i=0;i<graph.edges.size();++i) {
            const auto& edge=graph.edges[i];
            auto& placed=layout.edges[i];
            for(auto& p:placed.points) {
                p.x+=offset;p.y+=offset;
                bounds.include(p.x,p.y);
            }
            const auto path=elk?rounded_elk_path(placed.points):curve(placed.points);
            if(path.empty()) continue;
            const bool contains=doc.relationships[i].type==u"contains";
            paths+="<path"+attr("class",contains?"edge-thickness-normal edge-pattern-solid relationshipLine":
                "edge-thickness-normal edge-pattern-dashed relationshipLine")+
                attr("data-id",edge.id)+attr("d",path)+attr("fill","none")+attr("stroke",theme.line)+
                attr("stroke-width","1")+attr("stroke-dasharray",contains?"none":"10,7")+
                (contains?attr("marker-start","url(#requirement-contains-start)"):attr("marker-end","url(#requirement-arrow-end)"))+"/>";
            if(placed.label) {
                const auto& label=edge_texts[i];
                const FlowchartPoint at{placed.label->x+offset-label.width/2,placed.label->y+offset-label.height/2};
                bounds.include(at.x-edge_padding,at.y-edge_padding,edge_boxes[i].width,edge_boxes[i].height);
                const auto& color=config.html_labels?theme.text:theme.relation_label;
                labels+="<g class=\"edgeLabel\"><g class=\"label\""+attr("data-id",edge.id)+">"+
                    paint_text(label,at,color,config.html_labels,theme.label_background)+"</g></g>";
            }
        }
        for(std::size_t i=0;i<graph.nodes.size();++i) {
            const auto& node=layout.nodes[i];
            const auto& paint=paints[i];
            const double x=node.x+offset,y=node.y+offset,w=node.width,h=node.height;
            bounds.include(x-w/2,y-h/2,w,h);
            const auto outline=paint_attr(paint,u"stroke","stroke",theme.border)+
                paint_attr(paint,u"stroke-width","stroke-width",num(theme.stroke_width))+
                paint_attr(paint,u"stroke-dasharray","stroke-dasharray","none");
            nodes+="<g"+attr("class",paint.classes)+attr("id",u"mermaid-requirement-"+node.id)+
                attr("data-look","classic")+attr("transform","translate("+num(x)+", "+num(y)+")")+">";
            nodes+="<rect class=\"basic label-container outer-path\""+attr("x",num(-w/2))+attr("y",num(-h/2))+
                attr("width",num(w))+attr("height",num(h))+paint_attr(paint,u"fill","fill",theme.fill)+outline+"/>";
            const auto color=paint.styles.find(u"color");
            const auto text_color=color==paint.styles.end()?theme.text:xml(color->second);
            for(std::size_t j=0;j<paint.fields.size();++j) {
                const auto& field=paint.fields[j];
                const double glyph_top=config.html_labels?0:field.font.size-field.metrics.svg_baseline;
                const FlowchartPoint at{j<2?-field.width/2:-w/2+theme.padding/2,
                    field.offset-h/2+theme.padding+glyph_top};
                bounds.include(x+at.x,y+at.y,field.width,field.height);
                nodes+="<g class=\"label\">"+paint_text(field,at,text_color,config.html_labels)+"</g>";
            }
            if(paint.has_divider) {
                const double y=paint.divider-h/2;
                nodes+="<path class=\"divider\""+attr("d","M"+point({-w/2,y})+"L"+point({w/2,y}))+
                    attr("fill","none")+outline+"/>";
            }
            nodes+="</g>";
        }
        if(!std::isfinite(bounds.left)) bounds.include(0,0);
        std::string title;
        if(!doc.title.empty()) {
            const auto label=measure_field(measure,doc.title,base_font,base_font,false,true,false);
            const auto x=(bounds.left+bounds.right)/2,y=-config.title_top_margin;
            bounds.include(x-label.metrics.bounds_width/2,y-label.metrics.svg_baseline,
                label.metrics.bounds_width,label.metrics.svg_height);
            title="<text class=\"requirementDiagramTitleText\" text-anchor=\"middle\""+attr("x",num(x))+
                attr("y",num(y))+attr("fill",theme.title_color)+">"+xml(doc.title)+"</text>";
        }
        const double width=bounds.right-bounds.left+2*pad,height=bounds.bottom-bounds.top+2*pad;
        auto svg="<svg xmlns=\"http://www.w3.org/2000/svg\" class=\"requirementDiagram\" role=\"graphics-document document\""+
            attr("viewBox",num(bounds.left-pad)+" "+num(bounds.top-pad)+" "+num(width)+" "+num(height))+
            attr("font-family",theme.font_family)+attr("font-size",num(theme.font_size));
        svg+=config.use_max_width?attr("width","100%")+attr("style","max-width:"+num(width)+"px"):
            attr("width",num(width))+attr("height",num(height));
        if(!doc.acc_title.empty()) svg+=" aria-labelledby=\"requirement-title\"";
        if(!doc.acc_description.empty()) svg+=" aria-describedby=\"requirement-description\"";
        svg+=">";
        if(!doc.acc_title.empty()) svg+="<title id=\"requirement-title\">"+xml(doc.acc_title)+"</title>";
        if(!doc.acc_description.empty()) svg+="<desc id=\"requirement-description\">"+xml(doc.acc_description)+"</desc>";
        const auto marker_stroke=attr("stroke",theme.line);
        svg+="<defs><marker id=\"requirement-contains-start\" refX=\"0\" refY=\"10\" markerWidth=\"20\" markerHeight=\"20\" orient=\"auto\""+
            marker_stroke+"><circle cx=\"10\" cy=\"10\" r=\"9\" fill=\"none\"/><path d=\"M1 10H19M10 1V19\" fill=\"none\"/></marker>"
            "<marker id=\"requirement-arrow-end\" refX=\"20\" refY=\"10\" markerWidth=\"20\" markerHeight=\"20\" orient=\"auto\""+
            marker_stroke+"><path d=\"M0 0L20 10M20 10L0 20\" fill=\"none\"/></marker></defs>";
        svg+="<g class=\"root\"><g class=\"edgePaths\">"+paths+"</g><g class=\"edgeLabels\">"+labels+
            "</g><g class=\"nodes\">"+nodes+"</g></g>"+title+"</svg>";
        return RequirementSvg{std::move(svg),width,height};
    } catch(const std::exception& e) {
        return std::unexpected(e.what());
    }
}
}
