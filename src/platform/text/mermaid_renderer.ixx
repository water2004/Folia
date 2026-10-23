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
    return render_native_diagram(*input,widths);
}
}
