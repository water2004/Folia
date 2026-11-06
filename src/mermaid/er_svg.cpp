module folia.mermaid.er_svg;
import std;
import folia.mermaid.dagre_rank;
import folia.mermaid.flowchart_elk;
import folia.mermaid.number;
import folia.mermaid.svg_text;
import folia.mermaid.theme_color;
import folia.mermaid.rough_rectangle;

namespace folia::mermaid {
namespace {
const ConfigValue* member(const ConfigValue& value,std::u16string_view key){return value.find(key);}
std::u16string string(const ConfigValue& value,std::u16string_view key,std::u16string_view fallback={}){if(auto* v=member(value,key);v&&v->kind==ConfigKind::string)return v->string;return std::u16string{fallback};}
std::string color(const ConfigValue& value,std::u16string_view key,std::string fallback){if(auto* v=member(value,key);v&&v->kind==ConfigKind::string&&!v->string.empty())return std::string(v->string.begin(),v->string.end());return fallback;}
double dimension(const ConfigValue& value,std::u16string_view key,double fallback){if(auto* v=member(value,key);v&&v->kind==ConfigKind::number&&std::isfinite(v->number)&&v->number>=0)return v->number;return fallback;}
std::string number(double value){auto out=format_number(value);if(!out)throw std::runtime_error("Invalid ER geometry");return *out;}
std::string xml(std::u16string_view text){auto out=escape_svg_text(text,u"ER text");if(!out)throw std::runtime_error("Invalid ER text");return *out;}
std::string attr(std::string_view value){std::string out;for(char c:value){if(c=='&')out+="&amp;";else if(c=='<')out+="&lt;";else if(c=='\"')out+="&quot;";else out+=c;}return out;}
std::string point(FlowchartPoint p){return number(p.x)+","+number(p.y);}
FlowchartPoint mix(FlowchartPoint a,FlowchartPoint b,double wa,double wb,double divisor){return{(wa*a.x+wb*b.x)/divisor,(wa*a.y+wb*b.y)/divisor};}
std::string path(const std::vector<FlowchartPoint>& points){if(points.empty())return {};std::string d="M"+point(points.front());if(points.size()<3){for(std::size_t i=1;i<points.size();++i)d+="L"+point(points[i]);return d;}d+="L"+point(mix(points[0],points[1],5,1,6));auto segment=[&](FlowchartPoint a,FlowchartPoint b,FlowchartPoint c){d+="C"+point(mix(a,b,2,1,3))+" "+point(mix(a,b,1,2,3))+" "+point({(a.x+4*b.x+c.x)/6,(a.y+4*b.y+c.y)/6});};for(std::size_t i=2;i<points.size();++i)segment(points[i-2],points[i-1],points[i]);segment(points[points.size()-2],points.back(),points.back());return d+"L"+point(points.back());}
std::string marker_id(std::u16string_view card,bool start){std::string name;if(card==u"ONLY_ONE")name="onlyOne";else if(card==u"ZERO_OR_ONE")name="zeroOrOne";else if(card==u"ONE_OR_MORE")name="oneOrMore";else if(card==u"ZERO_OR_MORE")name="zeroOrMore";else return {};return "url(#mermaid-er_er-"+name+(start?"Start":"End")+")";}
std::string markers(std::string_view stroke){std::string s="<defs>";auto simple=[&](std::string_view id,std::string_view d){s+="<marker id=\"mermaid-er_er-"+std::string(id)+"\" refX=\""+(id.ends_with("Start")?"0":"18")+"\" refY=\"9\" markerWidth=\"18\" markerHeight=\"18\" orient=\"auto\"><path stroke=\""+attr(stroke)+"\" fill=\"none\" d=\""+std::string(d)+"\"/></marker>";};simple("onlyOneStart","M9,0 L9,18 M15,0 L15,18");simple("onlyOneEnd","M3,0 L3,18 M9,0 L9,18");s+="<marker id=\"mermaid-er_er-zeroOrOneStart\" refX=\"0\" refY=\"9\" markerWidth=\"30\" markerHeight=\"18\" orient=\"auto\"><circle stroke=\""+attr(stroke)+"\" fill=\"white\" cx=\"21\" cy=\"9\" r=\"6\"/><path stroke=\""+attr(stroke)+"\" fill=\"none\" d=\"M9,0 L9,18\"/></marker><marker id=\"mermaid-er_er-zeroOrOneEnd\" refX=\"30\" refY=\"9\" markerWidth=\"30\" markerHeight=\"18\" orient=\"auto\"><circle stroke=\""+attr(stroke)+"\" fill=\"white\" cx=\"9\" cy=\"9\" r=\"6\"/><path stroke=\""+attr(stroke)+"\" fill=\"none\" d=\"M21,0 L21,18\"/></marker>";s+="<marker id=\"mermaid-er_er-oneOrMoreStart\" refX=\"18\" refY=\"18\" markerWidth=\"45\" markerHeight=\"36\" orient=\"auto\"><path stroke=\""+attr(stroke)+"\" fill=\"none\" d=\"M0,18 Q 18,0 36,18 Q 18,36 0,18 M42,9 L42,27\"/></marker><marker id=\"mermaid-er_er-oneOrMoreEnd\" refX=\"27\" refY=\"18\" markerWidth=\"45\" markerHeight=\"36\" orient=\"auto\"><path stroke=\""+attr(stroke)+"\" fill=\"none\" d=\"M3,9 L3,27 M9,18 Q27,0 45,18 Q27,36 9,18\"/></marker>";s+="<marker id=\"mermaid-er_er-zeroOrMoreStart\" refX=\"18\" refY=\"18\" markerWidth=\"57\" markerHeight=\"36\" orient=\"auto\"><circle stroke=\""+attr(stroke)+"\" fill=\"white\" cx=\"48\" cy=\"18\" r=\"6\"/><path stroke=\""+attr(stroke)+"\" fill=\"none\" d=\"M0,18 Q18,0 36,18 Q18,36 0,18\"/></marker><marker id=\"mermaid-er_er-zeroOrMoreEnd\" refX=\"39\" refY=\"18\" markerWidth=\"57\" markerHeight=\"36\" orient=\"auto\"><circle stroke=\""+attr(stroke)+"\" fill=\"white\" cx=\"9\" cy=\"18\" r=\"6\"/><path stroke=\""+attr(stroke)+"\" fill=\"none\" d=\"M21,18 Q39,0 57,18 Q39,36 21,18\"/></marker></defs>";return s;}
FlowchartPoint intersect(const FlowchartPlacedNode& node,std::u16string_view,FlowchartPoint p) {
    const double dx=p.x-node.x,dy=p.y-node.y;
    const double scale=std::max(2*std::abs(dx)/node.width,2*std::abs(dy)/node.height);
    return scale>0?FlowchartPoint{node.x+dx/scale,node.y+dy/scale}:p;
}
#include "flowchart_elk_paint.inc"

struct Box {
    double left=std::numeric_limits<double>::infinity(),top=left,right=-left,bottom=-left;
    double width() const { return right-left; }
    double height() const { return bottom-top; }
    void include(double x, double y) {
        left=std::min(left,x);top=std::min(top,y);
        right=std::max(right,x);bottom=std::max(bottom,y);
    }
    void include(Box other) {
        include(other.left,other.top);include(other.right,other.bottom);
    }
};

Box centered(FlowchartPoint p, double width, double height) {
    return {p.x-width/2,p.y-height/2,p.x+width/2,p.y+height/2};
}

struct Label {
    std::u16string text;
    double width=0,height=0,baseline=0;
};

Label measure_label(const ErSvgMeasure& measure,std::u16string_view text,double size,bool html) {
    if(text.empty()) return {};
    auto result=measure(text,size,false);
    if(!result) throw std::runtime_error(result.error());
    for(double value:{result->advance,result->width,result->height,result->baseline})
        if(!std::isfinite(value)||value<0) throw std::runtime_error("Invalid ER text measurement");
    return {std::u16string(text),html?result->advance:result->width,
        html?size*1.5:result->height,result->baseline};
}

std::u16string joined_keys(std::span<const std::u16string> keys) {
    std::u16string text;
    for(const auto& key:keys) {
        if(!text.empty()) text+=u",";
        text+=key;
    }
    return text;
}

struct Row {
    std::array<Label,4> cells;
    double height=0;
};
struct EntityBox {
    Label title;
    std::vector<Row> rows;
    std::array<double,4> columns{};
    double width=0,height=0,header_height=0;
};

EntityBox measure_entity(const ErEntity& entity,const ErSvgMeasure& measure,double font_size,
    double padding,double text_padding,double minimum_width,bool html,bool raw_html) {
    EntityBox box;
    box.title=measure_label(measure,entity.alias.empty()?entity.name:entity.alias,font_size,html);
    if(entity.attributes.empty()) {
        box.width=std::max(minimum_width,box.title.width+2*padding);
        box.height=box.title.height+3*padding;
        return box;
    }
    // erBox tests the raw root flag for padding, while createText defaults
    // to HTML when that flag was omitted.
    if(!raw_html) {padding*=1.25;text_padding*=1.25;}
    box.header_height=box.title.height+text_padding;
    box.height=box.header_height;
    double widest_label=box.title.width;
    for(const auto& attribute:entity.attributes) {
        Row row;
        const auto keys=joined_keys(attribute.keys);
        const std::array<std::u16string_view,4> text{
            attribute.type,attribute.name,keys,attribute.comment};
        for(std::size_t i=0;i<text.size();++i) {
            auto& label=row.cells[i];
            label=measure_label(measure,text[i],font_size,html);
            box.columns[i]=std::max(box.columns[i],label.width+padding);
            row.height=std::max(row.height,label.height);
            widest_label=std::max(widest_label,label.width);
        }
        row.height+=text_padding;
        box.height+=row.height;
        box.rows.push_back(std::move(row));
    }
    std::size_t sections=4;
    for(std::size_t i=2;i<box.columns.size();++i) {
        if(box.columns[i]<=padding) {box.columns[i]=0;--sections;}
    }
    const auto columns=std::accumulate(box.columns.begin(),box.columns.end(),0.0);
    const auto extra=std::max(0.0,box.title.width+2*padding-columns);
    for(std::size_t i=0;i<box.columns.size();++i) {
        if(i<2 || box.columns[i]>0) box.columns[i]+=extra/static_cast<double>(sections);
    }
    box.width=std::max(widest_label+2*padding,columns+extra);
    return box;
}

struct Paint {
    const ErSvgTheme& theme;
    std::string family;
    std::string body;

    void plain_rect(Box box) {
        body+="<rect class=\"basic label-container\" x=\""+number(box.left)+
            "\" y=\""+number(box.top)+"\" width=\""+number(box.width())+
            "\" height=\""+number(box.height())+"\" fill=\""+attr(theme.background)+
            "\" stroke=\""+attr(theme.border)+"\" stroke-width=\"1\"/>";
    }

    void rough_rect(Box box,std::string_view fill,std::string_view css_class) {
        rough_shape(rough_rectangle_paths(box.left,box.top,box.width(),box.height()),fill,css_class);
    }

    void divider(double x1,double y1,double x2,double y2) {
        constexpr double half_thickness=.0001/2;
        const std::array<RoughPoint,4> points=x1==x2 ?
            std::array<RoughPoint,4>{{{x1-half_thickness,y1},{x1+half_thickness,y1},
                {x2+half_thickness,y2},{x2-half_thickness,y2}}} :
            std::array<RoughPoint,4>{{{x1,y1-half_thickness},{x1,y1+half_thickness},
                {x2,y2+half_thickness},{x2,y2-half_thickness}}};
        rough_shape(rough_polygon_paths(points),theme.background,"divider");
    }

    void rough_shape(const RoughRectanglePaths& paths,std::string_view fill,std::string_view css_class) {
        body+="<g class=\""+std::string(css_class)+"\"><path d=\""+paths.fill_path+
            "\" stroke=\"none\" stroke-width=\"0\" fill=\""+attr(fill)+
            "\"/><path d=\""+paths.stroke_path+"\" stroke=\""+attr(theme.border)+
            "\" stroke-width=\"1.3\" fill=\"none\" stroke-dasharray=\"0 0\"/></g>";
    }

    void label(const Label& value,double x,double y,double size,bool html,
        bool middle,std::string_view css_class,bool edge=false) {
        if(value.text.empty()) return;
        body+="<g class=\""+std::string(css_class)+"\">";
        if(html) {
            const auto left=middle?x-value.width/2:x;
            body+="<foreignObject x=\""+number(left)+"\" y=\""+number(y)+
                "\" width=\""+number(value.width)+"\" height=\""+number(value.height)+
                "\"><div xmlns=\"http://www.w3.org/1999/xhtml\"";
            if(edge) body+=" class=\"labelBkg\"";
            body+=" style=\"display:table-cell;white-space:nowrap;line-height:1.5;"+
                std::string(middle?"text-align:center;":"text-align:start;")+
                "font-family:"+family+";font-size:"+number(size)+"px;color:"+attr(theme.text);
            if(edge) body+=";background-color:"+attr(theme.label_background);
            body+="\"><span class=\""+std::string(edge?"edgeLabel":"nodeLabel")+"\"";
            if(edge) body+=" style=\"background-color:"+attr(theme.edge_background)+"\"";
            body+="><p style=\"margin:0\">"+xml(value.text)+"</p></span></div></foreignObject>";
        } else {
            text(value.text,x,y+size,size,middle,edge?theme.title_color:theme.text);
        }
        body+="</g>";
    }

    void text(std::u16string_view value,double x,double baseline,double size,
        bool middle,std::string_view color,std::string_view css_class={}) {
        body+="<text x=\""+number(x)+"\" y=\""+number(baseline)+
            "\" font-family=\""+family+"\" font-size=\""+number(size)+
            "px\" fill=\""+attr(color)+"\"";
        if(middle) body+=" text-anchor=\"middle\"";
        if(!css_class.empty()) body+=" class=\""+std::string(css_class)+"\"";
        body+=">"+xml(value)+"</text>";
    }
};

void paint_entity(Paint& paint,const EntityBox& entity,FlowchartPoint center,
    double padding,double text_padding,bool html,bool raw_html) {
    const auto box=centered(center,entity.width,entity.height);
    const auto& theme=paint.theme;
    paint.body+="<g class=\"node default\">";
    if(entity.rows.empty()) {
        paint.plain_rect(box);
        const double y=center.y-entity.title.height/2;
        paint.label(entity.title,center.x,y,theme.font_size,html,true,"label");
    } else {
        if(!raw_html) {padding*=1.25;text_padding*=1.25;}
        paint.rough_rect(box,theme.background,"outer-path");
        const auto header_bottom=box.top+entity.header_height;
        double row_y=header_bottom;
        for(std::size_t i=0;i<entity.rows.size();++i) {
            const auto row_bottom=row_y+entity.rows[i].height;
            const bool even=i%2==1;
            paint.rough_rect({box.left,row_y,box.right,row_bottom},
                even?theme.attribute_even:theme.attribute_odd,even?"row-rect-even":"row-rect-odd");
            row_y=row_bottom;
        }
        paint.label(entity.title,center.x,box.top+text_padding/2,theme.font_size,html,true,"label name");
        row_y=header_bottom+text_padding/2;
        for(const auto& row:entity.rows) {
            double x=box.left+padding/2;
            constexpr std::array classes{"label attribute-type","label attribute-name",
                "label attribute-keys","label attribute-comment"};
            for(std::size_t i=0;i<row.cells.size();++i) {
                paint.label(row.cells[i],x,row_y,theme.font_size,html,false,classes[i]);
                x+=entity.columns[i];
            }
            row_y+=row.height;
        }
        paint.divider(box.left,header_bottom,box.right,header_bottom);
        double x=box.left;
        for(std::size_t i=0;i<entity.columns.size()-1;++i) {
            x+=entity.columns[i];
            if(i==0 || entity.columns[i+1]>0)
                paint.divider(x,header_bottom,x,box.bottom);
        }
        // erBox inserts both a name divider and the first attribute divider
        // at this boundary.
        paint.divider(box.left,header_bottom,box.right,header_bottom);
    }
    paint.body+="</g>";
}

bool html_label_setting(const ConfigValue* value,bool fallback) {
    return value && value->kind==ConfigKind::boolean?value->boolean:fallback;
}

std::string alpha_half(std::u16string_view color) {
    const auto opaque=theme_detail::color_adjust(color,{{'a',1}});
    const auto faded=theme_detail::color_adjust(opaque,{{'a',-.5}});
    return {faded.begin(),faded.end()};
}

std::string font_attribute(std::u16string_view family) {
    std::string out;
    for(char c:xml(family)) {
        if(c=='"') out+="&quot;";
        else out+=c;
    }
    return out;
}
} // namespace

std::expected<ErRenderInput,std::string> prepare_er_render(
    std::u16string_view source,const ConfigSession& session,ErLimits limits) {
    auto document=prepare_er(source,session,std::move(limits));
    if(!document) return std::unexpected(document.error());
    ErRenderInput out;
    out.document=std::move(*document);
    const auto& config=out.document.effective_config;
    const auto layout=string(config,u"layout",u"elk");
    if((layout!=u"dagre"&&layout!=u"elk")||string(config,u"look",u"classic")!=u"classic")
        return std::unexpected("Native ER SVG requires Dagre or ELK layout and classic look");
    const auto* variables=member(config,u"themeVariables");
    if(!variables) return std::unexpected("ER configuration has no theme variables");
    auto& theme=out.theme;
    theme.font_family=string(config,u"fontFamily",u"Arial");
    theme.font_size=dimension(*variables,u"fontSize",16);
    theme.background=color(*variables,u"mainBkg",theme.background);
    theme.border=color(*variables,u"nodeBorder",theme.border);
    theme.title_color=color(*variables,u"textColor",theme.title_color);
    theme.text=color(*variables,u"nodeTextColor",theme.title_color);
    theme.line=color(*variables,u"lineColor",theme.line);
    const auto tertiary=string(*variables,u"tertiaryColor");
    if(!tertiary.empty()) theme.label_background=alpha_half(tertiary);
    theme.edge_background=color(*variables,u"edgeLabelBackground",theme.edge_background);
    theme.attribute_odd=color(*variables,u"rowOdd",theme.attribute_odd);
    theme.attribute_even=color(*variables,u"rowEven",theme.attribute_even);
    if(const auto* er=config.find(u"er"))
        theme.title_top_margin=dimension(*er,u"titleTopMargin",theme.title_top_margin);
    return out;
}

std::expected<ErSvg,std::string> render_er_svg(const ErRenderInput& render_input,const ErSvgMeasure& measure) {
    try {
        if(!measure) return std::unexpected("ER renderer requires a text measurement callback");
        const auto& input=render_input.document;
        const auto& theme=render_input.theme;
        const auto& config=input.effective_config;
        const ConfigValue empty;
        const auto* er=config.find(u"er");
        const auto& options=er?*er:empty;
        const auto padding=dimension(options,u"diagramPadding",20);
        const auto text_padding=dimension(options,u"entityPadding",15);
        const auto minimum_width=dimension(options,u"minEntityWidth",100);
        const auto* html_setting=config.find(u"htmlLabels");
        const bool raw_html=html_label_setting(html_setting,false);
        const bool node_html=html_label_setting(html_setting,true);
        const auto* flowchart=config.find(u"flowchart");
        const bool edge_html=html_label_setting(html_setting,
            html_label_setting(flowchart?flowchart->find(u"htmlLabels"):nullptr,true));
        const bool elk=string(config,u"layout",u"elk")==u"elk";
        const double layout_margin=elk?0:8;
        constexpr double label_size=14,viewport_padding=8;

        FlowchartInput graph;
        graph.direction=input.direction;
        graph.node_spacing=dimension(options,u"nodeSpacing",140);
        graph.rank_spacing=dimension(options,u"rankSpacing",80);
        if(graph.node_spacing==0) graph.node_spacing=140;
        if(graph.rank_spacing==0) graph.rank_spacing=80;
        std::vector<EntityBox> entities;
        std::vector<FlowchartNodeBox> node_sizes;
        for(const auto& entity:input.entities) {
            auto box=measure_entity(entity,measure,theme.font_size,padding,text_padding,
                minimum_width,node_html,raw_html);
            graph.nodes.push_back({.id=entity.id,.shape=u"squareRect"});
            node_sizes.push_back({box.width,box.height});
            entities.push_back(std::move(box));
        }

        std::vector<Label> labels;
        std::vector<FlowchartEdgeLabelSize> label_sizes;
        for(const auto& relationship:input.relationships) {
            const auto id=std::to_string(graph.edges.size());
            graph.edges.push_back({.id=u"er-edge-"+std::u16string(id.begin(),id.end()),
                .start=relationship.entity_a,.end=relationship.entity_b,.label=relationship.label,
                .type=u"arrow_open",.arrow_start=u"none",.arrow_end=u"none"});
            auto label=measure_label(measure,relationship.label,label_size,edge_html);
            label_sizes.push_back({label.width,label.height+(edge_html || label.text.empty()?0:2)});
            labels.push_back(std::move(label));
        }
        auto layout=[&]()->std::expected<FlowchartLayout,std::string> {
            if(!elk) return layout_flowchart(graph,node_sizes,label_sizes);
            if(graph.nodes.empty()) return FlowchartLayout{};
            FlowchartMeasurements measured;
            measured.nodes.resize(node_sizes.size());
            auto scene=layout_measured_elk_scene(graph,node_sizes,label_sizes,measured,{});
            if(!scene) return std::unexpected(scene.error());
            std::unordered_map<std::u16string_view,std::size_t> ids;
            for(std::size_t i=0;i<graph.nodes.size();++i) ids.emplace(graph.nodes[i].id,i);
            for(std::size_t i=0;i<graph.edges.size();++i) {
                auto& points=scene->layout.edges[i].points;
                if(points.empty()) continue;
                const auto& edge=graph.edges[i];
                const auto& from=scene->layout.nodes[ids.at(edge.start)];
                const auto& to=scene->layout.nodes[ids.at(edge.end)];
                clip_elk_node_endpoints(points,from,to,u"rect",u"rect");
                elk_remove_short_marker_segment(points,to,false);
                elk_remove_short_marker_segment(points,from,true);
            }
            return std::move(scene->layout);
        }();
        if(!layout) return std::unexpected(layout.error());
        Paint paint{theme,font_attribute(theme.font_family)};
        paint.body="<g class=\"root\"><g class=\"edgePaths\">";
        const auto place=[=](FlowchartPoint p) {
            return FlowchartPoint{p.x+layout_margin,p.y+layout_margin};
        };
        Box bounds;
        for(std::size_t i=0;i<input.relationships.size();++i) {
            const auto& relationship=input.relationships[i];
            auto points=layout->edges[i].points;
            for(auto& p:points) {p=place(p);bounds.include(p.x,p.y);}
            if(points.size()<2) continue;
            const bool dashed=relationship.rel_type==u"NON_IDENTIFYING";
            paint.body+="<path class=\"edge-thickness-normal edge-pattern-"+
                std::string(dashed?"dashed":"solid")+" relationshipLine\" d=\""+
                (elk?rounded_elk_path(points):path(points))+
                "\" fill=\"none\" stroke=\""+attr(theme.line)+"\" stroke-width=\"1\" stroke-dasharray=\""+
                (dashed?"8 8":"0")+"\" marker-start=\""+marker_id(relationship.card_b,true)+
                "\" marker-end=\""+marker_id(relationship.card_a,false)+"\"/>";
        }
        paint.body+="</g><g class=\"edgeLabels\">";
        for(std::size_t i=0;i<labels.size();++i) {
            if(const auto& label=layout->edges[i].label) {
                const auto center=place(*label);
                bounds.include(centered(center,label_sizes[i].width,label_sizes[i].height));
                const double y=center.y-(edge_html?labels[i].height/2:
                    label_size-labels[i].baseline+labels[i].height/2);
                paint.label(labels[i],center.x,y,label_size,edge_html,true,"edgeLabel",true);
            }
        }
        paint.body+="</g><g class=\"nodes\">";
        for(std::size_t i=0;i<entities.size();++i) {
            const auto& node=layout->nodes[i];
            const auto center=place({node.x,node.y});
            bounds.include(centered(center,node.width,node.height));
            paint_entity(paint,entities[i],center,padding,text_padding,node_html,raw_html);
        }
        paint.body+="</g></g>";

        if(!std::isfinite(bounds.left)) bounds={0,0,0,0};
        if(!input.title.empty()) {
            const auto title=measure_label(measure,input.title,theme.font_size,false);
            const auto x=(bounds.left+bounds.right)/2;
            const auto y=-theme.title_top_margin;
            paint.text(title.text,x,y,theme.font_size,true,theme.title_color,"erDiagramTitleText");
            bounds.include({x-title.width/2,y-title.baseline,x+title.width/2,y+title.height-title.baseline});
        }
        const auto width=bounds.width()+2*viewport_padding;
        const auto height=bounds.height()+2*viewport_padding;
        const auto view=number(bounds.left-viewport_padding)+" "+number(bounds.top-viewport_padding)+
            " "+number(width)+" "+number(height);
        std::string svg="<svg xmlns=\"http://www.w3.org/2000/svg\" class=\"erDiagram\" width=\""+
            number(width)+"\" height=\""+number(height)+"\" viewBox=\""+view+"\">";
        svg+=markers(theme.line)+paint.body+"</svg>";
        return ErSvg{std::move(svg),width,height};
    } catch(const std::exception& e) {
        return std::unexpected(e.what());
    }
}
} // namespace folia::mermaid
