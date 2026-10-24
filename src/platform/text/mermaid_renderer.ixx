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
            auto lines=flowchart_label_lines(text,source.label_type);
            if(!lines) return std::unexpected(lines.error());
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
                auto wrapped_lines=measurer.wrap(line,font,wrapping_width);
                if(!wrapped_lines) return std::unexpected(wrapped_lines.error());
                wrapped=wrapped||wrapped_lines->size()>1;
                for(auto& value:*wrapped_lines) box.lines.push_back(std::move(value));
            }
            for(const auto& line:box.lines) {
                auto metrics=measurer.measure(line,font,true);
                if(!metrics) return std::unexpected(metrics.error());
                box.width=std::max(box.width,metrics->bounds_width);
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
    return render_native_diagram(*input,widths,flowchart_measurements);
}
}
