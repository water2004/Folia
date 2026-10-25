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
    folia::mermaid::FrontendLimits limits = {}) {
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
    FlowchartMeasurements flowchart_measurements;
    if(auto flow=std::get_if<FlowchartRenderInput>(&input->content)) {
        if(!flow->document.title.empty()) {
            auto title=measurer.measure(flow->document.title,{flow->theme.font_family,18});
            if(!title) return std::unexpected(title.error());
            flowchart_measurements.title=FlowchartTextBox{
                .width=title->bounds_width,.height=title->line_height,.baseline=title->baseline};
        }
        auto measure=[&](const auto& source,double wrapping_width,bool group=false)->std::expected<FlowchartTextBox,std::string> {
            const auto& text=source.label;
            auto label_font=flowchart_label_font(source.compiled_styles,source.styles,flow->theme);
            if(!label_font) return std::unexpected(label_font.error());
            auto family=measurer.resolve_family(label_font->family);
            if(!family) return std::unexpected(family.error());
            label_font->family=std::move(*family);
            // Original clusters apply font styles after sizing their HTML
            // container. That separate group-title adapter is still pending.
            if(group && (label_font->family!=flow->theme.font_family || label_font->size!=flow->theme.font_size ||
                label_font->weight!=400 || label_font->italic))
                return std::unexpected("Flowchart group title font styles require the styled cluster-label adapter");
            const auto size=label_font->size;
            if(!std::isfinite(size)||size<=0||size>std::numeric_limits<float>::max())
                return std::unexpected("Invalid Mermaid font size");
            const MermaidFont font{label_font->family,static_cast<float>(size),label_font->weight,label_font->italic};
            auto lines=flowchart_label_runs(text,source.label_type);
            if(!lines) return std::unexpected(lines.error());
            const bool styled=std::ranges::any_of(*lines,[](const auto& line) {
                return std::ranges::any_of(line,[](const auto& run){return run.strong||run.emphasis;});
            });
            if(styled && (group||!flow->theme.html_label_spacing))
                return std::unexpected("Styled Flowchart labels currently require node/edge HTML label mode");
            FlowchartTextBox box;
            box.font=*label_font;
            // Empty lines still occupy a font line box, measured with a glyph's
            // font metrics rather than an estimated glyph height.
            auto font_metrics=measurer.measure(u"M",font);
            if(!font_metrics) return std::unexpected(font_metrics.error());
            box.line_step=flow->theme.html_label_spacing?size*1.5:font_metrics->line_height;
            box.baseline=font_metrics->baseline+(box.line_step-font_metrics->line_height)/2;
            bool wrapped=false;
            for(const auto& line:*lines) {
                std::u16string display;
                std::vector<MermaidTextStyle> styles;
                for(const auto& run:line) {
                    const auto weight=run.strong?(font.weight<350?400u:font.weight<550?700u:900u):font.weight;
                    styles.push_back({display.size(),run.text.size(),weight,font.italic||run.emphasis});
                    display+=run.text;
                }
                auto wrapped_lines=measurer.wrap(display,font,wrapping_width,styles);
                if(!wrapped_lines) return std::unexpected(wrapped_lines.error());
                wrapped=wrapped||wrapped_lines->size()>1;
                std::size_t start=0;
                for(auto& value:*wrapped_lines) {
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
                    auto metrics=measurer.measure(value,font,true,line_styles);
                    if(!metrics) return std::unexpected(metrics.error());
                    box.width=std::max(box.width,metrics->bounds_width);
                    box.lines.push_back(std::move(value));box.runs.push_back(std::move(runs));start=end;
                }
            }
            if(wrapped && flow->theme.html_label_spacing) box.width=std::max(box.width,wrapping_width);
            box.height=text.empty()?0:box.line_step*box.lines.size();
            if(text.empty()) box.baseline=0;
            return box;
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
            flowchart_measurements.edges.push_back(*box);
        }
    }
    SequenceMeasurements sequence_measurements;
    if(auto seq=std::get_if<SequenceRenderInput>(&input->content)) {
        if(seq->theme.font_size>std::numeric_limits<float>::max()) return std::unexpected("Invalid Sequence font size");
        const MermaidFont font{seq->theme.font_family,static_cast<float>(seq->theme.font_size),seq->theme.font_weight};
        auto height=measurer.svg_font_height(font);
        if(!height) return std::unexpected(height.error());
        const MermaidFont fallback{u"Arial",font.size,font.weight};
        auto fallback_height=measurer.svg_font_height(fallback);
        if(!fallback_height) return std::unexpected(fallback_height.error());
        auto measure=[&,font,fallback,height,fallback_height](std::u16string_view text)->std::expected<SequenceTextBox,std::string> {
            // Sequence splits explicit HTML breaks before measuring; protected
            // Mermaid entities remain encoded until the final SVG text pass.
            std::vector<std::u16string> lines(1);
            for(std::size_t i=0;i<text.size();) {
                if(text[i]==u'<') {
                    const auto end=text.find(u'>',i);
                    if(end==text.npos) return std::unexpected("Unsupported Sequence text markup");
                    auto tag=std::u16string{text.substr(i+1,end-i-1)};
                    std::erase_if(tag,[](char16_t c){return c==u'/'||c==u' '||c==u'\t';});
                    for(auto& c:tag) if(c>=u'A'&&c<=u'Z') c+=u'a'-u'A';
                    if(tag!=u"br") return std::unexpected("Sequence rich text requires its display adapter");
                    lines.emplace_back();i=end+1;
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
        auto wrap=[measure](std::u16string_view text,double max_width)->std::expected<SequenceTextBox,std::string> {
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
            auto box=measure(group.name);
            if(!box) return std::unexpected(box.error());
            sequence_measurements.boxes.push_back(*box);
        }
        for(const auto& actor:seq->document.actors) {
            auto box=actor.wrap?wrap(actor.description,seq->config.width-2*seq->config.wrap_padding):measure(actor.description);
            if(!box) return std::unexpected(box.error());
            sequence_measurements.actors.push_back(*box);
        }
        if(auto title=measure(seq->document.title);!title) return std::unexpected(title.error());
        for(const auto& message:seq->document.messages) {
            const bool bracket=sequence_region_start(message.type)||sequence_region_section(message.type);
            const auto text=bracket&&!message.message.empty()?u"["+message.message+u"]":message.message;
            auto box=message.wrap&&!bracket?wrap(text,seq->config.width-2*seq->config.wrap_padding):measure(text);
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
        auto measure=[&](std::u16string_view source,unsigned weight=400,bool italic=false,double size=0)->std::expected<ClassTextBox,std::string> {
            auto display=diagram_display_text(source);if(!display) return std::unexpected(display.error());
            if(display->find_first_of(u"\r\n")!=display->npos) return std::unexpected("Multiline Class labels require the line-breaking adapter");
            if(display->empty()) return ClassTextBox{};
            if(size==0) size=theme.font_size;
            const MermaidFont font{theme.font_family,static_cast<float>(size),weight,italic};
            auto metrics=measurer.measure(*display,font,true);if(!metrics) return std::unexpected(metrics.error());
            const auto height=size*1.5;
            return ClassTextBox{std::move(*display),metrics->advance,height,metrics->baseline+(height-metrics->line_height)/2};
        };
        for(const auto& node:cls->document.classes) {
            ClassNodeMeasurements m;
            auto label=measure(node.label+(node.type.empty()?std::u16string{}:u"<"+node.type+u">"),700);
            if(!label) return std::unexpected(label.error());m.label=std::move(*label);
            if(!node.annotations.empty()) {
                auto annotation=measure(u"\u00ab"+node.annotations.front()+u"\u00bb");
                if(!annotation) return std::unexpected(annotation.error());m.annotation=std::move(*annotation);
            }
            for(auto [members,output]:{std::pair{&node.members,&m.members},{&node.methods,&m.methods}})
                for(const auto& member:*members) {
                    auto box=measure(member.display_text,400,member.classifier==u"*");
                    if(!box) return std::unexpected(box.error());output->push_back(std::move(*box));
                }
            class_measurements.nodes.push_back(std::move(m));
        }
        for(const auto& note:cls->document.notes) {
            auto lines=flowchart_label_runs(note.text,u"markdown");
            if(!lines) return std::unexpected(lines.error());
            ClassNoteMeasurements m;
            for(const auto& line:*lines) {
                std::u16string display;
                for(const auto& run:line) {
                    if(run.strong||run.emphasis) return std::unexpected("Styled Class notes require the rich-label adapter");
                    display+=run.text;
                }
                const MermaidFont font{theme.font_family,static_cast<float>(theme.font_size)};
                auto metrics=measurer.measure(display.empty()?u"M":display,font,true);
                if(!metrics) return std::unexpected(metrics.error());
                const auto height=theme.font_size*1.5;
                ClassTextBox box{display,display.empty()?0:metrics->advance,height,metrics->baseline+(height-metrics->line_height)/2};
                m.width=std::max(m.width,box.width);m.height+=height;m.lines.push_back(std::move(box));
            }
            class_measurements.notes.push_back(std::move(m));
        }
        for(const auto& edge:cls->document.relations) {
            ClassEdgeMeasurements m;
            auto label=measure(edge.title);if(!label) return std::unexpected(label.error());m.label=std::move(*label);
            for(auto [value,target]:{std::pair{&edge.start_label,&m.start},{&edge.end_label,&m.end}}) {
                if(*value==u"none") continue;
                auto box=measure(*value,400,false,11);if(!box) return std::unexpected(box.error());*target=std::move(*box);
            }
            class_measurements.edges.push_back(std::move(m));
        }
    }
    return render_native_diagram(*input,widths,flowchart_measurements,sequence_measurements,class_measurements);
}
}
