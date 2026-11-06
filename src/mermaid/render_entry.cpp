module folia.mermaid.render_entry;
import std;
import folia.mermaid.svg_text;
import folia.mermaid.number;

namespace folia::mermaid {
namespace {
std::string narrow(std::u16string_view s) {
    std::string out;
    for (auto c : s) {
        if (c > 127) throw std::runtime_error("Expected ASCII style value");
        out += static_cast<char>(c);
    }
    return out;
}
const ConfigValue& member(const ConfigValue& value, std::u16string_view key) {
    const auto found = value.find(key);
    if (!found) throw std::runtime_error("Missing renderer configuration: " + narrow(key));
    return *found;
}
std::u16string text(const ConfigValue& value) {
    if (value.kind != ConfigKind::string) throw std::runtime_error("Expected renderer string");
    return value.string;
}
double number(const ConfigValue& value) {
    if (value.kind == ConfigKind::number && std::isfinite(value.number)) return value.number;
    throw std::runtime_error("Expected finite renderer number");
}
double size(const ConfigValue& value) {
    if (value.kind == ConfigKind::number) return number(value);
    auto s = narrow(text(value));
    if (s.ends_with("px")) s.resize(s.size()-2);
    double out{};
    const auto parsed = std::from_chars(s.data(),s.data()+s.size(),out);
    if (parsed.ec != std::errc{} || parsed.ptr != s.data()+s.size() || !std::isfinite(out))
        throw std::runtime_error("Renderer sizes require numbers or px units");
    return out;
}
bool boolean(const ConfigValue& value) {
    if (value.kind != ConfigKind::boolean) throw std::runtime_error("Expected renderer boolean");
    return value.boolean;
}
std::string error_text(std::u16string_view message) {
    auto escaped = escape_svg_text(message, u"Renderer error");
    return escaped ? *escaped : "Invalid diagram text";
}
void plain_metadata(std::u16string_view value) {
    // Plain text is emitted through the shared XML encoder. HTML metadata
    // requires a future explicit host policy; do not silently treat it as DOM.
    for (std::size_t i=0; i+1<value.size(); ++i) if (value[i]==u'<') {
        auto c=value[i+1];
        if ((c>=u'a'&&c<=u'z')||(c>=u'A'&&c<=u'Z')||c==u'/'||c==u'!')
            throw std::runtime_error("Native diagram metadata must be plain text");
    }
}
template<class Document> void metadata(Document& document, const PreparedDiagram& prepared) {
    if (document.title.empty() && prepared.title) document.title = *prepared.title;
    plain_metadata(document.title); plain_metadata(document.acc_title); plain_metadata(document.acc_description);
}
std::u16string entity(std::u16string_view name) {
    if (std::ranges::all_of(name,[](char16_t c){ return c>=u'0'&&c<=u'9'; })) {
        auto digits=narrow(name); std::uint32_t cp{};
        auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),cp);
        if (parsed.ec!=std::errc{} || cp==0 || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff))
            throw std::runtime_error("Invalid diagram character entity");
        if (cp<0x10000) return std::u16string(1,static_cast<char16_t>(cp));
        cp-=0x10000;
        return {static_cast<char16_t>(0xd800+(cp>>10)),static_cast<char16_t>(0xdc00+(cp&1023))};
    }
    for (auto [key,value] : {std::pair{u"quot",u"\""}, {u"amp",u"&"}, {u"apos",u"'"},
        {u"lt",u"<"}, {u"gt",u">"}, {u"nbsp",u"\u00a0"}, {u"copy",u"\u00a9"}, {u"reg",u"\u00ae"}})
        if (name==key) return value;
    throw std::runtime_error("Unsupported named diagram entity; use its decimal code point");
}
// Only generated text-node content is decoded. Styles, colors and attributes
// must never be interpreted as Mermaid source escapes.
void decode_svg_text(std::string& svg) {
    std::size_t pos=0;
    while ((pos=svg.find('<',pos))!=std::string::npos) {
        const bool is_text=svg.compare(pos,6,"<text ")==0 || svg.compare(pos,7,"<title>")==0 ||
            svg.compare(pos,6,"<desc>")==0;
        pos=svg.find('>',pos);
        if (pos==std::string::npos) break;
        ++pos;
        if (!is_text) continue;
        auto end=svg.find('<',pos);
        if (end==std::string::npos) break;
        while (pos<end) {
            constexpr std::string_view prefix="\xef\xac\x82\xc2\xb0", suffix="\xc2\xb6\xc3\x9f";
            if(svg.compare(pos,prefix.size(),prefix)!=0) { ++pos; continue; }
            auto start=pos+prefix.size();
            if(svg.compare(start,2,"\xc2\xb0")==0) start+=2;
            const auto stop=svg.find(suffix,start);
            if(stop==std::string::npos || stop>=end) throw std::runtime_error("Incomplete diagram entity");
            const auto name=svg.substr(start,stop-start);
            auto value=entity(std::u16string(name.begin(),name.end()));
            auto escaped=escape_svg_text(value,u"Diagram entity");
            if (!escaped) throw std::runtime_error(error_text(escaped.error()));
            auto length=stop+suffix.size()-pos;
            svg.replace(pos,length,*escaped); end=end-length+escaped->size(); pos+=escaped->size();
        }
    }
}
}

std::expected<std::u16string,std::string> diagram_display_text(std::u16string_view value) {
    try {
        std::u16string out;
        for (std::size_t i=0; i<value.size();) {
            if(value.substr(i,2)==u"\ufb02\u00b0") {
                auto start=i+2;
                if(start<value.size() && value[start]==u'\u00b0') ++start;
                auto end=value.find(u"\u00b6\u00df",start);
                if(end==value.npos) throw std::runtime_error("Incomplete diagram entity");
                out+=entity(value.substr(start,end-start)); i=end+2; continue;
            }
            out+=value[i++];
        }
        return out;
    } catch(const std::runtime_error& e) { return std::unexpected(e.what()); }
}

std::expected<NativeDiagram,std::string> prepare_native_diagram(
    std::u16string_view source,const ConfigSession& session,FrontendLimits limits) {
    auto prepared=prepare_diagram(source,session,limits);
    if (!prepared) return std::unexpected(prepared.error());
    try {
        const auto& effective=prepared->effective_config;
        const auto& theme=member(effective,u"themeVariables");
        if(prepared->type==u"architecture") {
            auto input=prepare_architecture_render(source,session,limits);
            if(!input)return std::unexpected(input.error());
            return NativeDiagram{std::move(*prepared),std::move(*input)};
        }
        if(prepared->type==u"mindmap") {
            auto input=prepare_mindmap_render(source,session,limits);
            if(!input)return std::unexpected(input.error());
            return NativeDiagram{std::move(*prepared),std::move(*input)};
        }
        if(prepared->type==u"stateDiagram") {
            auto input=prepare_state(source,session);
            if(!input)return std::unexpected(input.error());
            return NativeDiagram{std::move(*prepared),std::move(*input)};
        }
        if(prepared->type==u"c4") {
            auto input=prepare_c4(source,session);
            if(!input)return std::unexpected(input.error());
            return NativeDiagram{std::move(*prepared),std::move(*input)};
        }
        if(prepared->type==u"kanban") {
            auto input=prepare_kanban(source,session);
            if(!input)return std::unexpected(input.error());
            return NativeDiagram{std::move(*prepared),std::move(*input)};
        }
        if(prepared->type==u"block") {
            auto input=prepare_block(source,session);
            if(!input)return std::unexpected(input.error());
            return NativeDiagram{std::move(*prepared),std::move(*input)};
        }
        if(prepared->type==u"gitGraph") {
            auto input=prepare_git_graph(source,session);
            if(!input) return std::unexpected(input.error());
            return NativeDiagram{std::move(*prepared),std::move(*input)};
        }
        if(prepared->type==u"gantt") {
            auto input=prepare_gantt(source,session);
            if(!input) return std::unexpected(input.error());
            return NativeDiagram{std::move(*prepared),std::move(*input)};
        }
        if(prepared->type==u"timeline") {
            auto input=prepare_timeline(source,session);
            if(!input) return std::unexpected(input.error());
            return NativeDiagram{std::move(*prepared),std::move(*input)};
        }
        if(prepared->type==u"journey") {
            auto input=prepare_journey(source,session);
            if(!input) return std::unexpected(input.error());
            return NativeDiagram{std::move(*prepared),std::move(*input)};
        }
        if(prepared->type==u"treemap") {
            auto input=prepare_treemap(source,session);
            if(!input) return std::unexpected(input.error());
            return NativeDiagram{std::move(*prepared),std::move(*input)};
        }
        if(prepared->type==u"sankey") {
            auto input=prepare_sankey(source,session);
            if(!input) return std::unexpected(input.error());
            return NativeDiagram{std::move(*prepared),std::move(*input)};
        }
        if(prepared->type==u"xychart") {
            auto input=prepare_xy(source,session);
            if(!input) return std::unexpected(input.error());
            return NativeDiagram{std::move(*prepared),std::move(*input)};
        }
        if(prepared->type==u"radar") {
            auto input=prepare_radar(source,session);
            if(!input) return std::unexpected(input.error());
            metadata(input->document,*prepared);
            return NativeDiagram{std::move(*prepared),std::move(*input)};
        }
        if(prepared->type==u"quadrantChart") {
            auto input=prepare_quadrant(source,session);
            if(!input) return std::unexpected(input.error());
            metadata(input->document,*prepared);
            return NativeDiagram{std::move(*prepared),std::move(*input)};
        }
        if(prepared->type==u"classDiagram" || prepared->type==u"class") {
            if(text(member(effective,u"layout"))!=u"dagre" || text(member(effective,u"look"))!=u"classic")
                return std::unexpected("Native Class SVG currently requires explicit Dagre layout and classic look; ELK is not yet ported");
            auto doc=prepare_class(source,session,ClassLimits{.frontend=limits});
            if(!doc) return std::unexpected(doc.error());
            metadata(*doc,*prepared);
            const auto& cfg=member(effective,u"class");const auto& state=member(effective,u"state");
            // ClassDB.getData projects compact namespaces before measurement:
            // implicit ancestors disappear and explicit groups become siblings.
            if(auto value=cfg.find(u"hierarchicalNamespaces");value&&!boolean(*value)) {
                auto explicit_parent=[&](std::u16string parent) {
                    while(!parent.empty()) {
                        const auto found=std::ranges::find(doc->namespaces,parent,&ClassNamespace::id);
                        if(found==doc->namespaces.end()) return std::u16string{};
                        if(found->explicit_declaration) return parent;
                        parent=found->parent;
                    }
                    return parent;
                };
                for(auto& node:doc->classes) node.parent=explicit_parent(node.parent);
                for(auto& note:doc->notes) note.parent=explicit_parent(note.parent);
                std::erase_if(doc->namespaces,[](const auto& group){return !group.explicit_declaration;});
                for(auto& group:doc->namespaces) {
                    group.label=group.id;group.parent.clear();group.children.clear();
                }
            }
            ClassSvgConfig c;
            if(auto html=effective.find(u"htmlLabels");html&&html->kind!=ConfigKind::null) c.html_labels=boolean(*html);
            if(auto value=state.find(u"titleTopMargin")) c.title_top_margin=number(*value);
            if(auto value=cfg.find(u"padding")) c.padding=c.note_padding=c.group_padding=number(*value);
            const auto& margins=member(member(effective,u"flowchart"),u"subGraphTitleMargin");
            c.group_title_top=number(member(margins,u"top"));c.group_title_bottom=number(member(margins,u"bottom"));
            c.hide_empty_members=boolean(member(cfg,u"hideEmptyMembersBox"));
            for(auto [key,target]:{std::pair{u"nodeSpacing",&c.node_spacing},{u"rankSpacing",&c.rank_spacing}}) {
                if(auto value=state.find(key);value&&number(*value)!=0) *target=number(*value);
                if(auto value=effective.find(key);value&&number(*value)!=0) *target=number(*value);
            }
            if(c.padding<0||c.node_spacing<0||c.rank_spacing<0) return std::unexpected("Class spacing must be nonnegative");
            ClassSvgTheme t;t.font_family=text(member(effective,u"fontFamily"));t.font_size=size(member(theme,u"fontSize"));
            t.stroke_width=size(member(theme,u"strokeWidth"));
            if(t.font_size<=0||t.stroke_width<0) return std::unexpected("Invalid Class font or stroke size");
            for(auto [key,target]:{std::pair{u"mainBkg",&t.fill},{u"nodeBorder",&t.border},{u"classText",&t.text},
                {u"lineColor",&t.line},{u"edgeLabelBackground",&t.label_background},{u"textColor",&t.title_color},
                {u"noteBkgColor",&t.note_fill},{u"noteBorderColor",&t.note_border},{u"noteTextColor",&t.note_text},
                {u"clusterBkg",&t.cluster_fill},{u"clusterBorder",&t.cluster_border},{u"titleColor",&t.cluster_text}}) *target=narrow(text(member(theme,key)));
            return NativeDiagram{std::move(*prepared),ClassRenderInput{std::move(*doc),std::move(t),c}};
        }
        if(prepared->type==u"sequence") {
            auto document=prepare_sequence(source,session,SequenceLimits{.frontend=limits});
            if(!document) return std::unexpected(document.error());
            const auto& config=member(effective,u"sequence");
            if(text(member(effective,u"look"))!=u"classic")
                return std::unexpected("Native Sequence SVG currently requires classic look");
            SequenceSvgConfig c;
            c.message_align=text(member(config,u"messageAlign"));
            c.mirror_actors=boolean(member(config,u"mirrorActors"));
            c.hide_unused=boolean(member(config,u"hideUnusedParticipants"));
            c.show_numbers=boolean(member(config,u"showSequenceNumbers"));
            for(auto [key,target]:{std::pair{u"width",&c.width},{u"height",&c.height},{u"actorMargin",&c.actor_margin},
                {u"boxMargin",&c.box_margin},{u"wrapPadding",&c.wrap_padding},{u"diagramMarginX",&c.margin_x},
                {u"diagramMarginY",&c.margin_y},{u"bottomMarginAdj",&c.bottom_margin},
                {u"noteMargin",&c.note_margin},{u"activationWidth",&c.activation_width},
                {u"boxTextMargin",&c.box_text_margin},{u"labelBoxWidth",&c.label_box_width},{u"labelBoxHeight",&c.label_box_height}}) {
                *target=number(member(config,key));
                if(*target<0) return std::unexpected("Sequence spacing must be nonnegative");
            }
            c.right_angles=boolean(member(config,u"rightAngles"));
            c.note_align=text(member(config,u"noteAlign"));
            SequenceSvgTheme t;
            // Original setConf assigns root fontFamily/fontSize to all three
            // Sequence text categories before measurement and drawing.
            t.font_family=text(member(effective,u"fontFamily"));
            t.font_size=size(member(effective,u"fontSize"));
            if(t.font_size<=0) return std::unexpected("Sequence font size must be positive");
            if(auto weight=effective.find(u"fontWeight")) {
                const auto value=number(*weight);
                if(value<100||value>900||std::floor(value)!=value) return std::unexpected("Invalid Sequence font weight");
                t.font_weight=static_cast<unsigned>(value);
                t.actor_font_weight=t.message_font_weight=t.note_font_weight=t.font_weight;
            } else {
                for(auto [key,target]:{std::pair{u"actorFontWeight",&t.actor_font_weight},
                    {u"messageFontWeight",&t.message_font_weight},{u"noteFontWeight",&t.note_font_weight}}) {
                    const auto value=number(member(config,key));
                    if(value<100||value>900||std::floor(value)!=value) return std::unexpected("Invalid Sequence font weight");
                    *target=static_cast<unsigned>(value);
                }
            }
            for(auto [key,target]:{std::pair{u"actorBkg",&t.actor_fill},{u"actorBorder",&t.actor_border},
                {u"actorTextColor",&t.actor_text},{u"actorLineColor",&t.actor_line},
                {u"signalColor",&t.signal},{u"signalTextColor",&t.signal_text},
                {u"noteBkgColor",&t.note_fill},{u"noteBorderColor",&t.note_border},{u"noteTextColor",&t.note_text},
                {u"activationBkgColor",&t.activation_fill},{u"activationBorderColor",&t.activation_border},
                {u"labelBoxBkgColor",&t.label_fill},{u"labelBoxBorderColor",&t.label_border},
                {u"labelTextColor",&t.label_text},{u"loopTextColor",&t.loop_text},
                {u"textColor",&t.title_text},{u"sequenceNumberColor",&t.sequence_number}}) *target=narrow(text(member(theme,key)));
            t.rect_fill=t.actor_fill;
            if(auto fill=theme.find(u"rectBkgColor");fill&&fill->kind==ConfigKind::string&&!fill->string.empty())
                t.rect_fill=narrow(fill->string);
            return NativeDiagram{std::move(*prepared),SequenceRenderInput{std::move(*document),std::move(t),c}};
        }
        if(prepared->type==u"flowchart" || prepared->type==u"flowchart-v2") {
            auto graph=prepare_flowchart(source,session,FlowchartLimits{.frontend=limits});
            if(!graph) return std::unexpected(graph.error());
            plain_metadata(graph->title);
            FlowchartSvgTheme style;
            const auto& config=member(effective,u"flowchart");
            const auto look=text(member(effective,u"look"));
            if(look!=u"classic") return std::unexpected("Native Flowchart SVG currently requires classic look");
            const auto layout=text(member(effective,u"layout"));
            if(layout!=u"dagre"&&layout!=u"elk") return std::unexpected("Native Flowchart requires Dagre or ELK layout");
            style.font_family=text(member(theme,u"fontFamily"));style.font_size=size(member(theme,u"fontSize"));
            style.minimum_label_width=number(member(config,u"minNodeWidth"));
            style.wrapping_width=number(member(config,u"wrappingWidth"));style.padding=number(member(config,u"diagramPadding"));
            if(auto value=theme.find(u"radius")) style.radius=size(*value);
            // getEffectiveHtmlLabels: root override, family override, then true.
            auto html=effective.find(u"htmlLabels");
            if(!html || html->kind==ConfigKind::null) html=config.find(u"htmlLabels");
            style.html_label_spacing=!html || html->kind==ConfigKind::null || boolean(*html);
            style.fill=narrow(text(member(theme,u"mainBkg")));style.border=narrow(text(member(theme,u"nodeBorder")));
            const auto node_text=theme.find(u"nodeTextColor");
            style.text=narrow(text(node_text && node_text->kind==ConfigKind::string && !node_text->string.empty()?*node_text:member(theme,u"textColor")));
            style.line=narrow(text(member(theme,u"lineColor")));
            style.title_color=narrow(text(member(theme,u"textColor")));
            style.title_top_margin=number(member(config,u"titleTopMargin"));
            style.label_background=narrow(text(member(theme,u"edgeLabelBackground")));
            style.cluster_fill=narrow(text(member(theme,u"clusterBkg")));
            style.cluster_border=narrow(text(member(theme,u"clusterBorder")));
            style.cluster_text=narrow(text(member(theme,u"titleColor")));
            const auto& margins=member(config,u"subGraphTitleMargin");
            style.group_title_top=number(member(margins,u"top"));
            style.group_title_bottom=number(member(margins,u"bottom"));
            return NativeDiagram{std::move(*prepared),FlowchartRenderInput{std::move(*graph),std::move(style)}};
        }
        if (prepared->type==u"packet") {
            auto document=parse_packet(encode_diagram_source(prepared->code.cleaned));
            if (!document) return std::unexpected(error_text(document.error().message));
            metadata(*document,*prepared);
            const auto& config=member(effective,u"packet");
            PacketConfig c;
            c.row_height=number(member(config,u"rowHeight")); c.bit_width=number(member(config,u"bitWidth"));
            c.padding_x=number(member(config,u"paddingX")); c.padding_y=number(member(config,u"paddingY"));
            auto bits=number(member(config,u"bitsPerRow"));
            if (bits<1 || bits>std::numeric_limits<std::uint32_t>::max() || std::trunc(bits)!=bits)
                throw std::runtime_error("Packet bitsPerRow must be a positive integer");
            c.bits_per_row=static_cast<std::uint32_t>(bits);
            c.show_bits=boolean(member(config,u"showBits")); c.use_max_width=boolean(member(config,u"useMaxWidth"));
            c.descending=text(member(config,u"bitOrder"))==u"descending";
            auto layout=layout_packet(*document,c);
            if (!layout) return std::unexpected(error_text(layout.error().message));
            PacketSvgTheme style; style.font_family=text(member(effective,u"fontFamily"));
            if (const auto packet=theme.find(u"packet")) {
                auto paint=[&](auto key,std::string& target) { if(auto v=packet->find(key)) target=narrow(text(*v)); };
                auto dimension=[&](auto key,double& target) { if(auto v=packet->find(key)) target=size(*v); };
                paint(u"startByteColor",style.start_color); paint(u"endByteColor",style.end_color);
                paint(u"labelColor",style.label_color); paint(u"titleColor",style.title_color);
                paint(u"blockStrokeColor",style.stroke_color); paint(u"blockFillColor",style.fill_color);
                dimension(u"byteFontSize",style.byte_size); dimension(u"labelFontSize",style.label_size);
                dimension(u"titleFontSize",style.title_size); dimension(u"blockStrokeWidth",style.stroke_width);
            }
            return NativeDiagram{std::move(*prepared),PacketRenderInput{std::move(*layout),std::move(style)}};
        }
        if (prepared->type!=u"pie") return std::unexpected("Native renderer is not available for this diagram family");
        auto parsed=parse_pie(encode_diagram_source(prepared->code.cleaned));
        if (!parsed) return std::unexpected(error_text(parsed.error().message));
        metadata(*parsed,*prepared);
        auto document=populate_pie(*parsed);
        if (!document) return std::unexpected(error_text(document.error().message));
        const auto& config=member(effective,u"pie");
        PieSvgConfig c;
        c.layout.geometry.donut_hole=number(member(config,u"donutHole"));
        c.layout.geometry.text_position=number(member(config,u"textPosition"));
        c.use_max_width=boolean(member(config,u"useMaxWidth"));
        c.highlight_slice=text(member(config,u"highlightSlice"));
        const auto position=text(member(config,u"legendPosition"));
        bool known=false;
        for(auto [name,value] : {std::pair{u"right",PieLegendPosition::right},{u"left",PieLegendPosition::left},
            {u"top",PieLegendPosition::top},{u"bottom",PieLegendPosition::bottom},{u"center",PieLegendPosition::center}})
            if(position==name) { c.layout.legend_position=value; known=true; }
        if(!known) throw std::runtime_error("Unknown Pie legend position");
        PieSvgTheme t;
        for(std::size_t i=0; i<12; ++i) {
            const auto suffix=std::to_string(i+1);
            if(auto paint=theme.find(u"pie"+std::u16string(suffix.begin(),suffix.end())))
                t.colors[i]=narrow(text(*paint));
        }
        t.font_family=text(member(theme,u"fontFamily"));
        t.stroke_color=narrow(text(member(theme,u"pieStrokeColor")));
        t.outer_stroke_color=narrow(text(member(theme,u"pieOuterStrokeColor")));
        t.title_color=narrow(text(member(theme,u"pieTitleTextColor")));
        t.section_color=narrow(text(member(theme,u"pieSectionTextColor")));
        t.legend_color=narrow(text(member(theme,u"pieLegendTextColor")));
        t.stroke_width=size(member(theme,u"pieStrokeWidth")); t.outer_stroke_width=size(member(theme,u"pieOuterStrokeWidth"));
        t.opacity=size(member(theme,u"pieOpacity")); t.title_size=size(member(theme,u"pieTitleTextSize"));
        t.section_size=size(member(theme,u"pieSectionTextSize")); t.legend_size=size(member(theme,u"pieLegendTextSize"));
        return NativeDiagram{std::move(*prepared),PieRenderInput{std::move(*document),std::move(t),std::move(c)}};
    } catch(const std::runtime_error& e) { return std::unexpected(e.what()); }
}

std::expected<DiagramSvg,std::string> render_native_diagram(const NativeDiagram& input,const PieMeasurements& widths,const FlowchartMeasurements& flowchart_measurements,const SequenceMeasurements& sequence_measurements,const ClassMeasurements& class_measurements,const XYMeasurements& xy_measurements,const SankeyMeasurements& sankey_measurements,const TreemapMeasure& treemap_measure,const JourneyMeasure& journey_measure,const TimelineMeasure& timeline_measure,const GanttMeasure& gantt_measure,const GitGraphMeasure& git_graph_measure,const MindmapMeasurements& mindmap_measurements,const BlockMeasure& block_measure,const KanbanMeasure& kanban_measure,const C4Measurements& c4_measurements,const ArchitectureMeasurements& architecture_measurements,const StateMeasure& state_measure) {
    return std::visit([&](const auto& content)->std::expected<DiagramSvg,std::string> {
        if constexpr(std::is_same_v<std::decay_t<decltype(content)>,ArchitectureRenderInput>) {
            auto result=render_architecture_svg(content,architecture_measurements);
            if(!result)return std::unexpected(result.error());
            return DiagramSvg{std::move(result->svg),result->width,result->height};
        } else if constexpr(std::is_same_v<std::decay_t<decltype(content)>,MindmapRenderInput>) {
            auto result=render_mindmap_svg(content,mindmap_measurements);
            if(!result)return std::unexpected(result.error());
            return DiagramSvg{std::move(result->svg),result->width,result->height};
        } else if constexpr(std::is_same_v<std::decay_t<decltype(content)>,StateRenderInput>) {
            auto result=render_state_svg(content,state_measure);
            if(!result)return std::unexpected(result.error());
            return DiagramSvg{std::move(result->svg),result->width,result->height};
        } else if constexpr(std::is_same_v<std::decay_t<decltype(content)>,C4RenderInput>) {
            auto result=render_c4_svg(content,c4_measurements);
            if(!result)return std::unexpected(result.error());
            return DiagramSvg{std::move(result->svg),result->width,result->height};
        } else if constexpr(std::is_same_v<std::decay_t<decltype(content)>,KanbanRenderInput>) {
            auto result=render_kanban_svg(content,kanban_measure);
            if(!result)return std::unexpected(result.error());
            return DiagramSvg{std::move(result->svg),result->width,result->height};
        } else if constexpr(std::is_same_v<std::decay_t<decltype(content)>,BlockRenderInput>) {
            auto result=render_block_svg(content,block_measure);
            if(!result)return std::unexpected(result.error());
            return DiagramSvg{std::move(result->svg),result->width,result->height};
        } else if constexpr(std::is_same_v<std::decay_t<decltype(content)>,GitGraphRenderInput>) {
            auto result=render_git_graph_svg(content,git_graph_measure);
            if(!result) return std::unexpected(result.error());
            return DiagramSvg{std::move(result->svg),result->width,result->height};
        } else if constexpr(std::is_same_v<std::decay_t<decltype(content)>,GanttRenderInput>) {
            auto result=render_gantt_svg(content,gantt_measure);
            if(!result) return std::unexpected(result.error());
            return DiagramSvg{std::move(result->svg),result->width,result->height};
        } else if constexpr(std::is_same_v<std::decay_t<decltype(content)>,TimelineRenderInput>) {
            auto result=render_timeline_svg(content,timeline_measure);
            if(!result) return std::unexpected(result.error());
            return DiagramSvg{std::move(result->svg),result->width,result->height};
        } else if constexpr(std::is_same_v<std::decay_t<decltype(content)>,JourneyRenderInput>) {
            auto result=render_journey_svg(content,journey_measure);
            if(!result) return std::unexpected(result.error());
            return DiagramSvg{std::move(result->svg),result->width,result->height};
        } else if constexpr(std::is_same_v<std::decay_t<decltype(content)>,TreemapRenderInput>) {
            auto result=render_treemap_svg(content,treemap_measure);
            if(!result) return std::unexpected(result.error());
            return DiagramSvg{std::move(result->svg),result->width,result->height};
        } else if constexpr(std::is_same_v<std::decay_t<decltype(content)>,SankeyRenderInput>) {
            auto result=render_sankey_svg(content,sankey_measurements);
            if(!result) return std::unexpected(result.error());
            return DiagramSvg{std::move(result->svg),result->width,result->height};
        } else if constexpr(std::is_same_v<std::decay_t<decltype(content)>,XYRenderInput>) {
            auto result=render_xy_svg(content,xy_measurements);
            if(!result) return std::unexpected(result.error());
            return DiagramSvg{std::move(result->svg),result->width,result->height};
        } else if constexpr(std::is_same_v<std::decay_t<decltype(content)>,RadarRenderInput>) {
            auto result=render_radar_svg(content);
            if(!result) return std::unexpected(result.error());
            try { decode_svg_text(result->svg); }
            catch(const std::runtime_error& e) { return std::unexpected(e.what()); }
            return DiagramSvg{std::move(result->svg),result->width,result->height};
        } else if constexpr(std::is_same_v<std::decay_t<decltype(content)>,QuadrantRenderInput>) {
            auto result=render_quadrant_svg(content);
            if(!result) return std::unexpected(result.error());
            try { decode_svg_text(result->svg); }
            catch(const std::runtime_error& e) { return std::unexpected(e.what()); }
            return DiagramSvg{std::move(result->svg),result->width,result->height};
        } else if constexpr(std::is_same_v<std::decay_t<decltype(content)>,ClassRenderInput>) {
            auto result=render_class_svg(content,class_measurements);
            if(!result) return std::unexpected(result.error());
            return DiagramSvg{std::move(result->svg),result->width,result->height};
        } else if constexpr(std::is_same_v<std::decay_t<decltype(content)>,SequenceRenderInput>) {
            auto result=render_sequence_svg(content,sequence_measurements);
            if(!result) return std::unexpected(result.error());
            try { decode_svg_text(result->svg); }
            catch(const std::runtime_error& e) { return std::unexpected(e.what()); }
            return DiagramSvg{std::move(result->svg),result->width,result->height};
        } else if constexpr(std::is_same_v<std::decay_t<decltype(content)>,FlowchartRenderInput>) {
            auto result=render_flowchart_svg(content,flowchart_measurements);
            if(!result) return std::unexpected(result.error());
            return DiagramSvg{std::move(result->svg),result->width,result->height};
        } else {
        auto result=[&] {
            if constexpr(std::is_same_v<std::decay_t<decltype(content)>,PacketRenderInput>)
                return render_packet_svg(content.layout,content.theme);
            else return render_pie_svg(content.document,widths,content.theme,content.config);
        }();
        if (!result) return std::unexpected(error_text(result.error().message));
        const auto& doc=[&]() -> const auto& {
            if constexpr(std::is_same_v<std::decay_t<decltype(content)>,PacketRenderInput>) return content.layout.document;
            else return content.document;
        }();
        try {
            std::string metadata;
            for(auto [tag,value] : {std::pair{"title",std::u16string_view(doc.acc_title)}, {"desc",std::u16string_view(doc.acc_description)}}) {
                if(value.empty()) continue;
                auto escaped=escape_svg_text(value,u"Accessibility metadata");
                if(!escaped) return std::unexpected(error_text(escaped.error()));
                metadata+="<"+std::string(tag)+">"+*escaped+"</"+tag+">";
            }
            result->svg.insert(result->svg.find('>')+1,metadata);
            decode_svg_text(result->svg);
            return DiagramSvg{std::move(result->svg),result->width,result->height};
        } catch(const std::runtime_error& e) { return std::unexpected(e.what()); }
        }
    },input.content);
}
}
