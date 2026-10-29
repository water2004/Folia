module;
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

export module folia.platform.mermaid_renderer;
import std;
import folia.platform.mermaid_measurer;
export import folia.mermaid.render_entry;

export namespace folia::platform {
// Opt-in native source-to-SVG API for the currently implemented families.
// Resolve fonts, measure final display text and emit using the same font.
// Failures are explicit and never invoke another renderer or estimate widths.
std::expected<folia::mermaid::DiagramSvg,std::string> render_mermaid(
    std::u16string_view source, const folia::mermaid::ConfigSession& session,
    folia::mermaid::FrontendLimits limits = {}, std::optional<double> available_width = {}) {
    using namespace folia::mermaid;
    auto input=prepare_native_diagram(source,session,limits);
    if(!input) return std::unexpected(input.error());
    MermaidMeasurer measurer;
    auto font=std::visit([&](auto& content) -> std::expected<void,std::string> {
        auto resolved=measurer.resolve_family(content.theme.font_family);
        if(!resolved) return std::unexpected(resolved.error());
        content.theme.font_family=std::move(*resolved);
        return {};
    },input->content);
    if(!font) return std::unexpected(font.error());
    PieMeasurements widths;
    if(auto pie=std::get_if<PieRenderInput>(&input->content)) {
        auto measure=[&](std::u16string_view text,double size) -> std::expected<double,std::string> {
            auto display=diagram_display_text(text);
            if(!display) return std::unexpected(display.error());
            if(!std::isfinite(size) || size<=0 || size>std::numeric_limits<float>::max())
                return std::unexpected("Invalid Mermaid font size");
            auto metrics=measurer.measure(*display,{pie->theme.font_family,static_cast<float>(size)});
            if(!metrics) return std::unexpected(metrics.error());
            return metrics->bounds_width;
        };
        auto labels=pie_legend_text(pie->document);
        if(!labels) return std::unexpected("Invalid Pie legend data");
        for(const auto& label:*labels) {
            auto width=measure(label,pie->theme.legend_size);
            if(!width) return std::unexpected(width.error());
            widths.legend_widths.push_back(*width);
        }
        auto title=measure(pie->document.title,pie->theme.title_size);
        if(!title) return std::unexpected(title.error());
        widths.title_width=*title;
    }
    auto measure_label=[&measurer](const FlowchartSvgTheme& label_theme,const ConfigValue& effective_config,const auto& source,double wrapping_width,bool group=false)->std::expected<FlowchartTextBox,std::string> {
            const auto& text=source.label;
            const bool svg_label=!label_theme.html_label_spacing;
            const auto* auto_wrap=effective_config.find(u"markdownAutoWrap");
            const bool nowrap=source.label_type==u"markdown" && !svg_label && auto_wrap &&
                auto_wrap->kind==ConfigKind::boolean && !auto_wrap->boolean;
            if(nowrap)wrapping_width=std::numeric_limits<double>::infinity();
            // Class CSS is inherited during sizing; direct cluster styles are
            // applied to the finished span by clusters.js after its size is fixed.
            auto label_font=flowchart_label_font(source.compiled_styles,group?std::vector<std::u16string>{}:source.styles,label_theme);
            if(!label_font) return std::unexpected(label_font.error());
            auto family=measurer.resolve_family(label_font->family);
            if(!family) return std::unexpected(family.error());
            label_font->family=std::move(*family);
            const auto size=label_font->size;
            if(!std::isfinite(size)||size<=0||size>std::numeric_limits<float>::max())
                return std::unexpected("Invalid Mermaid font size");
            const MermaidFont font{label_font->family,static_cast<float>(size),label_font->weight,label_font->italic};
            auto lines=svg_label?flowchart_svg_label_runs(text,source.label_type):flowchart_label_runs(text,source.label_type);
            if(!lines) return std::unexpected(lines.error());
            // markdownToHTML preserves spaces as NBSP when auto wrapping is off.
            if(nowrap)for(auto& line:*lines)for(auto& run:line)
                std::ranges::replace(run.text,u' ',u'\u00a0');
            FlowchartTextBox box;
            box.font=*label_font;box.svg_label=svg_label;
            box.centered_group=group && source.label_type==u"markdown";
            // Empty lines still occupy a font line box, measured with a glyph's
            // font metrics rather than an estimated glyph height.
            auto font_metrics=measurer.measure(u"M",font);
            if(!font_metrics) return std::unexpected(font_metrics.error());
            box.line_step=size*(svg_label?1.1:1.5);
            box.baseline=svg_label?size:font_metrics->baseline+(box.line_step-font_metrics->line_height)/2;
            bool wrapped=false;
            for(const auto& line:*lines) {
                std::u16string display;
                std::vector<MermaidTextStyle> styles;
                for(const auto& run:line) {
                    const auto weight=svg_label?(run.strong?700u:400u):run.strong?(font.weight<350?400u:font.weight<550?700u:900u):font.weight;
                    styles.push_back({display.size(),run.text.size(),weight,svg_label?run.emphasis:font.italic||run.emphasis});
                    display+=run.text;
                }
                auto wrapped_lines=measurer.wrap(display,font,wrapping_width,styles,!svg_label);
                if(!wrapped_lines) return std::unexpected(wrapped_lines.error());
                wrapped=wrapped||wrapped_lines->size()>1;
                std::size_t start=0;
                for(auto& value:*wrapped_lines) {
                    const auto raw_end=start+value.size();
                    if(svg_label) {
                        while(!value.empty() && value.front()==u' ') {value.erase(0,1);++start;}
                        while(!value.empty() && value.back()==u' ') value.pop_back();
                    }
                    const auto end=start+value.size();
                    FlowchartLabelLine runs;
                    std::vector<MermaidTextStyle> line_styles;
                    for(std::size_t i=0;i<styles.size();++i) {
                        const auto& style=styles[i];
                        const auto first=std::max(start,style.offset),last=std::min(end,style.offset+style.length);
                        if(first>=last) continue;
                        runs.push_back({display.substr(first,last-first),line[i].strong,line[i].emphasis});
                        line_styles.push_back({first-start,last-first,style.weight,style.italic});
                    }
                    auto metrics=measurer.measure(value,font,true,line_styles,svg_label);
                    if(!metrics) return std::unexpected(metrics.error());
                    box.width=std::max(box.width,metrics->bounds_width);
                    box.lines.push_back(std::move(value));box.runs.push_back(std::move(runs));start=raw_end;
                }
            }
            if(wrapped && label_theme.html_label_spacing) box.width=std::max(box.width,wrapping_width);
            box.height=text.empty()?0:box.line_step*box.lines.size();
            if(svg_label && !text.empty()) {
                auto metrics=measurer.svg_font_metrics(font);if(!metrics)return std::unexpected(metrics.error());
                box.height=metrics->height+box.line_step*(box.lines.size()-1);
            }
            if(text.empty()) box.baseline=0;
            if(group && !svg_label && !source.styles.empty()) {
                auto paint=flowchart_label_font(source.compiled_styles,source.styles,label_theme);
                if(!paint)return std::unexpected(paint.error());
                auto family=measurer.resolve_family(paint->family);
                if(!family)return std::unexpected(family.error());
                paint->family=std::move(*family);
                const MermaidFont font{paint->family,static_cast<float>(paint->size),paint->weight,paint->italic};
                auto metrics=measurer.measure(u"M",font);if(!metrics)return std::unexpected(metrics.error());
                box.paint_font=*paint;box.paint_line_step=paint->size*1.5;
                box.paint_baseline=metrics->baseline+(box.paint_line_step-metrics->line_height)/2;
            }
            return box;
        };
    FlowchartMeasurements flowchart_measurements;
    if(auto flow=std::get_if<FlowchartRenderInput>(&input->content)) {
        if(!flow->document.title.empty()) {
            auto title=measurer.measure(flow->document.title,{flow->theme.font_family,18});
            if(!title) return std::unexpected(title.error());
            flowchart_measurements.title=FlowchartTextBox{
                .width=title->bounds_width,.height=title->line_height,.baseline=title->baseline};
        }
        auto measure=[flow,&measure_label](const auto& source,double width,bool group=false) {
            return measure_label(flow->theme,flow->document.effective_config,source,width,group);
        };
        flowchart_measurements.measure_group=[flow,measure](std::size_t index,double width) {
            return measure(flow->document.nodes.at(index),width,true);
        };
        for(const auto& node:flow->document.nodes) {
            auto box=measure(node,node.group?std::numeric_limits<double>::infinity():flow->theme.wrapping_width,node.group);
            if(!box) return std::unexpected(box.error());
            flowchart_measurements.nodes.push_back(*box);
        }
        for(const auto& edge:flow->document.edges) {
            // createText uses its 200px default for edge labels; group titles
            // go through createLabel's unbounded plain-text width above.
            auto box=measure(edge,200);if(!box) return std::unexpected(box.error());
            if(box->svg_label && !edge.label.empty()) {
                const auto& f=*box->font;
                auto metrics=measurer.svg_font_metrics({f.family,static_cast<float>(f.size),f.weight,f.italic});
                if(!metrics)return std::unexpected(metrics.error());
                // createText's edge background is the glyph box plus two units per side.
                box->width+=4;box->height+=4;box->baseline=metrics->baseline+2;
            }
            flowchart_measurements.edges.push_back(*box);
        }
    }
    MindmapMeasurements mindmap_measurements;
    if(auto mindmap=std::get_if<MindmapRenderInput>(&input->content)) {
        for(const auto& node:mindmap->document.nodes) {
            FlowchartNode label_source;label_source.label=node.label;label_source.label_type=node.label_type;
            const double width=node.type==1||node.type==2||node.type==6?mindmap->theme.wrapping_width:node.width;
            auto box=measure_label(mindmap->theme,mindmap->document.effective_config,label_source,width);
            if(!box)return std::unexpected(box.error());
            // createText measures a CSS table-cell. Its inline-size is rounded
            // up to a 1/64px layout unit before it becomes an SVG shape input.
            if(mindmap->theme.html_label_spacing)
                box->width=std::ceil(box->width*64)/64;
            mindmap_measurements.nodes.push_back(std::move(*box));
        }
    }
    SequenceMeasurements sequence_measurements;
    if(auto seq=std::get_if<SequenceRenderInput>(&input->content)) {
        if(seq->theme.font_size>std::numeric_limits<float>::max()) return std::unexpected("Invalid Sequence font size");
        auto measure_font=[&,seq](std::u16string_view text,unsigned weight)->std::expected<SequenceTextBox,std::string> {
            const MermaidFont font{seq->theme.font_family,static_cast<float>(seq->theme.font_size),weight};
            auto height=measurer.svg_font_height(font);if(!height)return std::unexpected(height.error());
            const MermaidFont fallback{u"Arial",font.size,font.weight};
            auto fallback_height=measurer.svg_font_height(fallback);if(!fallback_height)return std::unexpected(fallback_height.error());
            // Sequence splits explicit HTML breaks before measuring; protected
            // Mermaid entities remain encoded until the final SVG text pass.
            std::vector<std::u16string> lines(1);
            for(std::size_t i=0;i<text.size();) {
                if(text[i]==u'<') {
                    const auto end=text.find(u'>',i);
                    if(end!=text.npos) {
                        auto tag=std::u16string{text.substr(i+1,end-i-1)};
                        std::erase_if(tag,[](char16_t c){return c==u'/'||c==u' '||c==u'\t';});
                        for(auto& c:tag) if(c>=u'A'&&c<=u'Z') c+=u'a'-u'A';
                        if(tag==u"br") {lines.emplace_back();i=end+1;continue;}
                    }
                    // Sequence drawText uses .text(); non-break markup is literal.
                    lines.back()+=text[i++];
                } else lines.back()+=text[i++];
            }
            SequenceTextBox box{.display=std::u16string{text},.lines=lines};
            double width=0,generic_width=0;
            for(const auto& line:lines) {
            auto metrics=measurer.measure(line.empty()?u"\u200b":std::u16string_view{line},font,false,{},true);
            if(!metrics) return std::unexpected(metrics.error());
            auto generic=measurer.measure(line.empty()?u"\u200b":std::u16string_view{line},fallback,false,{},true);
            if(!generic) return std::unexpected(generic.error());
            width=std::max(width,std::round(metrics->bounds_width));
            generic_width=std::max(generic_width,std::round(generic->bounds_width));
            }
            auto line_height=text.empty()?0:*height;
            // Original chooses sans-serif only when all dimensions are larger.
            if(*fallback_height>line_height && generic_width>width && !text.empty()) {
                width=generic_width;line_height=*fallback_height;
            }
            box.width=width;box.line_height=line_height;box.height=line_height*lines.size();
            return box;
        };
        auto wrap=[measure_font](std::u16string_view text,double max_width,unsigned weight)->std::expected<SequenceTextBox,std::string> {
            const auto measure=[&](std::u16string_view value){return measure_font(value,weight);};
            auto initial=measure(text);if(!initial) return initial;
            if(initial->lines.size()>1 || text.empty()) return initial;
            // Original utils.wrapLabel/breakString: rounded SVG widths, a
            // space measured with each word, and code-point hyphenation.
            std::vector<std::u16string> lines;
            std::u16string next;
            auto finish=[&](std::u16string value) {if(!value.empty()) lines.push_back(std::move(value));};
            for(std::size_t pos=0;pos<text.size();) {
                if(text[pos]==u' ') {++pos;continue;}
                const auto end=text.find(u' ',pos);
                const auto word=text.substr(pos,end==text.npos?text.size()-pos:end-pos);
                pos=end==text.npos?text.size():end;
                auto word_box=measure(std::u16string{word}+u" ");if(!word_box) return word_box;
                auto next_box=measure(next);if(!next_box) return next_box;
                if(word_box->width>max_width) {
                    finish(std::move(next));next.clear();
                    for(std::size_t i=0;i<word.size();) {
                        next+=word[i++];
                        if(next.back()>=0xd800&&next.back()<=0xdbff&&i<word.size()) next+=word[i++];
                        auto part=measure(next);if(!part) return part;
                        if(part->width>=max_width) {
                            if(i<word.size()) next+=u'-';
                            finish(std::move(next));next.clear();
                        }
                    }
                } else if(next_box->width+word_box->width>=max_width) {
                    finish(std::move(next));next=word;
                } else {if(!next.empty()) next+=u' ';next+=word;}
            }
            finish(std::move(next));
            std::u16string joined;
            for(const auto& line:lines) {if(!joined.empty()) joined+=u"<br/>";joined+=line;}
            return measure(joined);
        };
        sequence_measurements.wrap=wrap;
        for(const auto& group:seq->document.boxes) {
            auto box=measure_font(group.name,seq->theme.message_font_weight);
            if(!box) return std::unexpected(box.error());
            sequence_measurements.boxes.push_back(*box);
        }
        for(const auto& actor:seq->document.actors) {
            auto box=actor.wrap?wrap(actor.description,seq->config.width-2*seq->config.wrap_padding,seq->theme.actor_font_weight):measure_font(actor.description,seq->theme.actor_font_weight);
            if(!box) return std::unexpected(box.error());
            sequence_measurements.actors.push_back(*box);
        }
        if(auto title=measure_font(seq->document.title,seq->theme.font_weight);!title) return std::unexpected(title.error());
        for(const auto& message:seq->document.messages) {
            const bool bracket=sequence_region_start(message.type)||sequence_region_section(message.type);
            const auto text=bracket&&!message.message.empty()?u"["+message.message+u"]":message.message;
            const auto weight=message.type==SequenceKind::note?seq->theme.note_font_weight:seq->theme.message_font_weight;
            auto box=message.wrap&&!bracket?wrap(text,seq->config.width-2*seq->config.wrap_padding,weight):measure_font(text,weight);
            if(!box) return std::unexpected(box.error());
            sequence_measurements.messages.push_back(*box);
        }
    }
    ClassMeasurements class_measurements;
    if(auto cls=std::get_if<ClassRenderInput>(&input->content)) {
        const auto& theme=cls->theme;
        if(theme.font_size>std::numeric_limits<float>::max()) return std::unexpected("Invalid Class font size");
        if(!cls->document.title.empty()) {
            const MermaidFont font{theme.font_family,static_cast<float>(theme.font_size)};
            auto title=measurer.measure(cls->document.title,font,true);
            if(!title) return std::unexpected(title.error());
            auto metrics=measurer.svg_font_metrics(font);if(!metrics) return std::unexpected(metrics.error());
            class_measurements.title={cls->document.title,title->bounds_width,metrics->height,metrics->baseline};
        }
        auto measure=[&](std::u16string_view source,unsigned weight=400,bool italic=false,double size=0,bool markdown=true,
            double wrapping_width=std::numeric_limits<double>::infinity())->std::expected<ClassTextBox,std::string> {
            auto display=diagram_display_text(source);if(!display) return std::unexpected(display.error());
            if(display->empty()) return ClassTextBox{};
            if(size==0) size=theme.font_size;
            const bool svg_label=!cls->config.html_labels;
            // SVG createText puts an explicit normal/italic style on each run,
            // overriding the classifier's inherited italic presentation.
            const MermaidFont font{theme.font_family,static_cast<float>(size),weight,italic&&!svg_label};
            auto lines=flowchart_label_runs(*display,markdown?u"markdown":u"text");
            if(!lines) return std::unexpected(lines.error());
            ClassTextBox box;box.line_step=size*(svg_label?1.1:1.5);box.svg_label=svg_label;
            std::vector<FlowchartLabelLine> display_lines;
            bool wrapped=false;
            for(const auto& line:*lines) {
                std::u16string value;
                std::vector<MermaidTextStyle> styles;
                for(const auto& run:line) {
                    styles.push_back({value.size(),run.text.size(),run.strong?(weight<350?400u:weight<550?700u:900u):weight,font.italic||run.emphasis});
                    value+=run.text;
                }
                auto slices=measurer.wrap(value,font,wrapping_width,styles);
                if(!slices) return std::unexpected(slices.error());
                wrapped=wrapped||slices->size()>1;
                std::size_t start=0;
                for(const auto& slice:*slices) {
                    const auto end=start+slice.size();FlowchartLabelLine runs;
                    for(std::size_t i=0;i<styles.size();++i) {
                        const auto first=std::max(start,styles[i].offset),last=std::min(end,styles[i].offset+styles[i].length);
                        if(first<last) runs.push_back({value.substr(first,last-first),line[i].strong,line[i].emphasis});
                    }
                    display_lines.push_back(std::move(runs));start=end;
                }
            }
            for(const auto& line:display_lines) {
                std::u16string value;
                std::vector<MermaidTextStyle> styles;
                for(const auto& run:line) {
                    styles.push_back({value.size(),run.text.size(),run.strong?(weight<350?400u:weight<550?700u:900u):weight,font.italic||run.emphasis});
                    value+=run.text;
                }
                auto metrics=measurer.measure(value.empty()?std::u16string_view{u"M"}:std::u16string_view{value},font,true,styles,svg_label);
                if(!metrics) return std::unexpected(metrics.error());
                const double width=value.empty()?0:svg_label?metrics->bounds_width:metrics->advance;
                box.width=std::max(box.width,width);box.line_widths.push_back(width);
                // HTML inline boxes share a baseline but each font retains its
                // own half-leading. Include the parent strut as createText does.
                auto strut=measurer.svg_font_metrics(font);if(!strut) return std::unexpected(strut.error());
                if(svg_label) {
                    box.line_baselines.push_back(size+box.lines.size()*box.line_step);
                    box.height=strut->height+box.lines.size()*box.line_step;
                    box.baseline=size;box.runs.push_back(line);box.text+=value;box.lines.push_back(std::move(value));
                    continue;
                }
                double above=strut->baseline+(box.line_step-strut->height)/2,below=box.line_step-above;
                for(const auto& style:styles) {
                    auto run_metrics=measurer.svg_font_metrics({font.family,font.size,style.weight,style.italic});
                    if(!run_metrics) return std::unexpected(run_metrics.error());
                    const auto baseline=run_metrics->baseline+(box.line_step-run_metrics->height)/2;
                    above=std::max(above,baseline);below=std::max(below,box.line_step-baseline);
                }
                box.line_baselines.push_back(box.height+above);box.height+=above+below;
                box.baseline=box.line_baselines.front();box.runs.push_back(line);
                box.text+=value;box.lines.push_back(std::move(value));
            }
            if(wrapped) box.width=std::max(box.width,wrapping_width);
            return box;
        };
        for(const auto& group:cls->document.namespaces) {
            auto label=measure(group.label,400,false,0,false);if(!label) return std::unexpected(label.error());
            class_measurements.namespaces.push_back(std::move(*label));
        }
        for(const auto& node:cls->document.classes) {
            ClassNodeMeasurements m;
            const bool styled=cls->config.html_labels&&std::ranges::any_of(node.styles,[](const auto& style){return style.find(u"font-")!=style.npos;});
            auto apply_paint=[&](ClassTextBox& box,unsigned weight,bool italic)->std::expected<void,std::string> {
                if(!styled||box.text.empty()) return {};
                auto style=class_text_style(node,theme,weight,italic);if(!style)return std::unexpected(style.error());
                auto family=measurer.resolve_family(style->family);if(!family)return std::unexpected(family.error());
                style->family=std::move(*family);
                const MermaidFont font{style->family,static_cast<float>(style->size),style->weight,style->italic};
                auto strut=measurer.svg_font_metrics(font);if(!strut)return std::unexpected(strut.error());
                for(std::size_t i=0;i<box.lines.size();++i) {
                    double above=strut->baseline+(style->size*1.5-strut->height)/2;
                    for(const auto& run:box.runs[i]) {
                        auto metrics=measurer.svg_font_metrics({font.family,font.size,
                            run.strong?(font.weight<350?400u:font.weight<550?700u:900u):font.weight,font.italic||run.emphasis});
                        if(!metrics)return std::unexpected(metrics.error());
                        above=std::max(above,metrics->baseline+(style->size*1.5-metrics->height)/2);
                    }
                    box.paint_baselines.push_back(i*style->size*1.5+above);
                }
                box.paint_font=std::move(*style);return {};
            };
            auto label=measure(node.label+(node.type.empty()?std::u16string{}:u"&lt;"+node.type+u"&gt;"),700);
            if(!label) return std::unexpected(label.error());m.label=std::move(*label);
            if(auto painted=apply_paint(m.label,700,false);!painted)return std::unexpected(painted.error());
            if(!node.annotations.empty()) {
                auto annotation=measure(u"\u00ab"+node.annotations.front()+u"\u00bb");
                if(!annotation) return std::unexpected(annotation.error());m.annotation=std::move(*annotation);
                if(auto painted=apply_paint(m.annotation,400,false);!painted)return std::unexpected(painted.error());
            }
            for(auto [members,output]:{std::pair{&node.members,&m.members},{&node.methods,&m.methods}})
                for(const auto& member:*members) {
                    auto box=measure(member.text,400,member.classifier==u"*");
                    if(!box) return std::unexpected(box.error());
                    if(auto painted=apply_paint(*box,400,member.classifier==u"*");!painted)return std::unexpected(painted.error());
                    output->push_back(std::move(*box));
                }
            class_measurements.nodes.push_back(std::move(m));
        }
        for(const auto& note:cls->document.notes) {
            // ClassDB sets white-space: nowrap; only explicit lines split notes.
            auto box=measure(note.text);if(!box) return std::unexpected(box.error());
            ClassNoteMeasurements m;
            m.width=box->width;m.height=box->height;m.lines.push_back(std::move(*box));
            class_measurements.notes.push_back(std::move(m));
        }
        for(const auto& interface_node:cls->document.interfaces) {
            auto label=measure(interface_node.label,400,false,0,false);
            if(!label) return std::unexpected(label.error());
            class_measurements.interfaces.push_back(std::move(*label));
        }
        for(const auto& edge:cls->document.relations) {
            ClassEdgeMeasurements m;
            // The unified edge painter uses createText's default 200px width.
            auto label=measure(edge.title,400,false,0,true,200);if(!label) return std::unexpected(label.error());m.label=std::move(*label);
            if(!cls->config.html_labels&&!m.label.text.empty()) {
                // createText's centered edge background adds two units per side.
                m.label.width+=4;m.label.height+=4;
            }
            for(auto [value,target]:{std::pair{&edge.start_label,&m.start},{&edge.end_label,&m.end}}) {
                if(*value==u"none") continue;
                auto box=measure(*value,400,false,11);if(!box) return std::unexpected(box.error());*target=std::move(*box);
            }
            class_measurements.edges.push_back(std::move(m));
        }
    }
    SankeyMeasurements sankey_measurements;
    if(auto sankey=std::get_if<SankeyRenderInput>(&input->content)) {
        const MermaidFont font{sankey->theme.font_family,14};
        auto height=measurer.svg_font_metrics(font);
        if(!height) return std::unexpected(height.error());
        for(const auto& text:sankey_measurement_requests(*sankey)) {
            auto metrics=measurer.measure(text,font,false,{},true);
            if(!metrics) return std::unexpected(metrics.error());
            sankey_measurements.labels.push_back({metrics->bounds_width,height->height,height->baseline});
        }
    }
    XYMeasurements xy_measurements;
    if(auto xy=std::get_if<XYRenderInput>(&input->content)) {
        for(const auto& request:xy_measurement_requests(*xy)) {
            const MermaidFont font{xy->theme.font_family,static_cast<float>(request.font_size)};
            auto metrics=measurer.measure(request.text,font,true,{},true);
            if(!metrics) return std::unexpected(metrics.error());
            auto height=measurer.svg_font_height(font);
            if(!height) return std::unexpected(height.error());
            xy_measurements.text.push_back({metrics->bounds_width,request.text.empty()?0:*height});
        }
    }
    TreemapMeasure treemap_measure;
    if(auto treemap=std::get_if<TreemapRenderInput>(&input->content)) {
        treemap_measure=[treemap,&measurer](std::u16string_view text,double size,unsigned weight,bool italic)->std::expected<TreemapTextBox,std::string> {
            const MermaidFont font{treemap->theme.font_family,static_cast<float>(size),weight,italic};
            auto box=measurer.measure(text,font,false,{},true);
            if(!box)return std::unexpected(box.error());
            auto metrics=measurer.svg_font_metrics(font);
            if(!metrics)return std::unexpected(metrics.error());
            return TreemapTextBox{box->advance,box->bounds_width,metrics->height,metrics->baseline,metrics->middle_offset};
        };
    }
    JourneyMeasure journey_measure;
    if(std::holds_alternative<JourneyRenderInput>(input->content)) {
        journey_measure=[&measurer](std::u16string_view text,double size,std::u16string_view family,unsigned weight)->std::expected<JourneyTextBox,std::string> {
            auto resolved=measurer.resolve_family(family);
            if(!resolved)return std::unexpected(resolved.error());
            const MermaidFont font{*resolved,static_cast<float>(size),weight};
            auto box=measurer.measure(text,font,true,{},true);
            if(!box)return std::unexpected(box.error());
            auto metrics=measurer.svg_font_metrics(font);
            if(!metrics)return std::unexpected(metrics.error());
            return JourneyTextBox{box->bounds_width,metrics->height,metrics->baseline,box->line_height,metrics->middle_offset};
        };
    }
    TimelineMeasure timeline_measure;
    if(auto timeline=std::get_if<TimelineRenderInput>(&input->content)) {
        timeline_measure=[timeline,&measurer](std::u16string_view text,double size,unsigned weight)->std::expected<TimelineTextBox,std::string> {
            const MermaidFont font{timeline->theme.font_family,static_cast<float>(size),weight};
            auto box=measurer.measure(text,font,false,{},true);
            if(!box)return std::unexpected(box.error());
            auto metrics=measurer.svg_font_metrics(font);
            if(!metrics)return std::unexpected(metrics.error());
            return TimelineTextBox{box->advance,box->bounds_width,metrics->height,metrics->baseline,metrics->middle_offset};
        };
    }
    GanttMeasure gantt_measure;
    if(auto gantt=std::get_if<GanttRenderInput>(&input->content)) {
        gantt_measure=[gantt,&measurer](std::u16string_view text,double size)->std::expected<double,std::string> {
            auto box=measurer.measure(text,{gantt->theme.font_family,static_cast<float>(size)},false,{},true);
            if(!box)return std::unexpected(box.error());
            return box->bounds_width;
        };
    }
    GitGraphMeasure git_graph_measure;
    if(auto git=std::get_if<GitGraphRenderInput>(&input->content)) {
        git_graph_measure=[git,&measurer](std::u16string_view text,double size)->std::expected<GitGraphTextBox,std::string> {
            const MermaidFont font{git->theme.font_family,static_cast<float>(size)};
            auto box=measurer.measure(text,font,false,{},true);
            if(!box)return std::unexpected(box.error());
            auto metrics=measurer.svg_font_metrics(font);
            if(!metrics)return std::unexpected(metrics.error());
            return GitGraphTextBox{box->bounds_width,metrics->height,metrics->baseline};
        };
    }
    BlockMeasure block_measure;
    if(auto block=std::get_if<BlockRenderInput>(&input->content)) {
        block_measure=[block,&measurer](std::u16string_view text,double size)->std::expected<BlockTextBox,std::string> {
            const MermaidFont font{block->theme.font_family,static_cast<float>(size)};
            auto box=measurer.measure(text,font,false,{},true);
            if(!box)return std::unexpected(box.error());
            auto metrics=measurer.svg_font_metrics(font);
            if(!metrics)return std::unexpected(metrics.error());
            return BlockTextBox{box->advance,box->bounds_width,metrics->height,metrics->baseline,box->line_height};
        };
    }
    KanbanMeasure kanban_measure;
    if(auto kanban=std::get_if<KanbanRenderInput>(&input->content)) {
        kanban_measure=[kanban,&measurer](std::u16string_view text,double size)->std::expected<KanbanTextBox,std::string> {
            const MermaidFont font{kanban->theme.font_family,static_cast<float>(size)};
            auto box=measurer.measure(text,font,false,{},true);
            if(!box)return std::unexpected(box.error());
            auto metrics=measurer.svg_font_metrics(font);
            if(!metrics)return std::unexpected(metrics.error());
            return KanbanTextBox{box->advance,box->bounds_width,metrics->height,metrics->baseline,box->line_height};
        };
    }
    C4Measurements c4_measurements;
    if(std::holds_alternative<C4RenderInput>(input->content)) {
        if(available_width)c4_measurements.available_width=*available_width;
        else {RECT area{};if(!SystemParametersInfoW(SPI_GETWORKAREA,0,&area,0))return std::unexpected("Cannot obtain C4 available work area");c4_measurements.available_width=area.right-area.left;}
        c4_measurements.measure=[&measurer](std::u16string_view text,const C4Font& requested)->std::expected<C4TextBox,std::string> {
            auto family=measurer.resolve_family(requested.family);if(!family)return std::unexpected(family.error());
            const MermaidFont font{*family,static_cast<float>(requested.size),requested.weight,requested.italic};
            auto box=measurer.measure(text,font,false,{},true);if(!box)return std::unexpected(box.error());
            auto metrics=measurer.svg_font_metrics(font);if(!metrics)return std::unexpected(metrics.error());
            return C4TextBox{box->advance,box->bounds_width,metrics->height,metrics->baseline,metrics->middle_offset};
        };
    }
    ArchitectureMeasurements architecture_measurements;
    if(auto architecture=std::get_if<ArchitectureRenderInput>(&input->content)) {
        const auto icon_size=architecture->document.config.find(u"iconSize")->number;
        const auto node_size=architecture->document.config.find(u"fontSize")->number;
        auto metrics=measurer.svg_font_metrics({architecture->theme.font_family,static_cast<float>(architecture->theme.font_size)});
        if(!metrics)return std::unexpected(metrics.error());
        architecture_measurements.svg_ascent=metrics->baseline;
        architecture_measurements.svg_middle=metrics->middle_offset;
        architecture_measurements.measure_label=[architecture,&measure_label](std::u16string_view text,double width) {
            FlowchartNode label;label.label=text;label.label_type=u"markdown";
            return measure_label(architecture->theme,architecture->document.effective_config,label,width);
        };
        // Cytoscape's canvas labels retain its own default font, independently
        // of the theme font used by createText in the visible SVG.
        auto cy_family=measurer.resolve_family(u"Helvetica Neue, Helvetica, sans-serif");
        if(!cy_family)return std::unexpected(cy_family.error());
        auto cy_label=[&measurer,&cy_family](std::u16string_view value,double size)->std::expected<ArchitectureCyLabel,std::string> {
            if(value.empty())return ArchitectureCyLabel{};
            ArchitectureCyLabel out;
            std::size_t start=0;
            while(start<=value.size()) {
                const auto end=value.find(u'\n',start);const auto line=value.substr(start,end==value.npos?value.size()-start:end-start);
                auto measured=measurer.measure(line,{*cy_family,static_cast<float>(size)},true);
                if(!measured)return std::unexpected(measured.error());
                out.width=std::max(out.width,std::ceil(measured->advance));out.height+=size;
                if(end==value.npos)break;start=end+1;
            }
            return out;
        };
        for(const auto& node:architecture->document.nodes) {
            auto label=architecture_measurements.measure_label(node.title,icon_size*1.5);
            if(!label)return std::unexpected(label.error());architecture_measurements.services.push_back(std::move(*label));
            auto icon_text=architecture_measurements.measure_label(node.icon_text,icon_size-2);
            if(!icon_text)return std::unexpected(icon_text.error());architecture_measurements.icon_text.push_back(std::move(*icon_text));
            auto cy=cy_label(node.title,node_size);if(!cy)return std::unexpected(cy.error());architecture_measurements.cy_labels.nodes.push_back(*cy);
        }
        for(const auto& edge:architecture->document.edges) {
            auto cy=cy_label(edge.title,16);if(!cy)return std::unexpected(cy.error());architecture_measurements.cy_labels.edges.push_back(*cy);
        }
    }
    StateMeasure state_measure;
    if(auto state=std::get_if<StateRenderInput>(&input->content)) {
        state_measure=[state,&measurer](std::u16string_view text,double size)->std::expected<StateTextBox,std::string> {
            const MermaidFont font{state->theme.font_family,static_cast<float>(size)};
            auto box=measurer.measure(text,font,false,{},true);if(!box)return std::unexpected(box.error());
            auto metrics=measurer.svg_font_metrics(font);if(!metrics)return std::unexpected(metrics.error());
            return StateTextBox{box->advance,box->bounds_width,metrics->height,metrics->baseline,box->line_height};
        };
    }
    return render_native_diagram(*input,widths,flowchart_measurements,sequence_measurements,class_measurements,xy_measurements,sankey_measurements,treemap_measure,journey_measure,timeline_measure,gantt_measure,git_graph_measure,mindmap_measurements,block_measure,kanban_measure,c4_measurements,architecture_measurements,state_measure);
}
}
