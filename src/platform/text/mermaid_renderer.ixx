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
        auto measure=[&](std::u16string_view text,std::u16string_view type,double wrapping_width)->std::expected<FlowchartTextBox,std::string> {
            const auto size=flow->theme.font_size;
            if(!std::isfinite(size)||size<=0||size>std::numeric_limits<float>::max())
                return std::unexpected("Invalid Mermaid font size");
            auto lines=flowchart_label_lines(text,type);
            if(!lines) return std::unexpected(lines.error());
            FlowchartTextBox box;
            // Empty lines still occupy a font line box, measured with a glyph's
            // font metrics rather than an estimated glyph height.
            auto font_metrics=measurer.measure(u"M",{flow->theme.font_family,static_cast<float>(size)});
            if(!font_metrics) return std::unexpected(font_metrics.error());
            box.line_step=flow->theme.html_label_spacing?size*1.5:font_metrics->line_height;
            box.baseline=font_metrics->baseline+(box.line_step-font_metrics->line_height)/2;
            bool wrapped=false;
            for(const auto& line:*lines) {
                auto wrapped_lines=measurer.wrap(line,{flow->theme.font_family,static_cast<float>(size)},wrapping_width);
                if(!wrapped_lines) return std::unexpected(wrapped_lines.error());
                wrapped=wrapped||wrapped_lines->size()>1;
                for(auto& value:*wrapped_lines) box.lines.push_back(std::move(value));
            }
            for(const auto& line:box.lines) {
                auto metrics=measurer.measure(line,{flow->theme.font_family,static_cast<float>(size)},true);
                if(!metrics) return std::unexpected(metrics.error());
                box.width=std::max(box.width,metrics->bounds_width);
            }
            if(wrapped && flow->theme.html_label_spacing) box.width=std::max(box.width,wrapping_width);
            box.height=text.empty()?0:box.line_step*box.lines.size();
            if(text.empty()) box.baseline=0;
            return box;
        };
        for(const auto& node:flow->document.nodes) {
            auto box=measure(node.label,node.label_type,node.group?std::numeric_limits<double>::infinity():flow->theme.wrapping_width);
            if(!box) return std::unexpected(box.error());
            flowchart_measurements.nodes.push_back(*box);
        }
        for(const auto& edge:flow->document.edges) {
            // createText uses its 200px default for edge labels; group titles
            // go through createLabel's unbounded plain-text width above.
            auto box=measure(edge.label,edge.label_type,200);if(!box) return std::unexpected(box.error());
            flowchart_measurements.edges.push_back(*box);
        }
    }
    return render_native_diagram(*input,widths,flowchart_measurements);
}
}
