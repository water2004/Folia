export module folia.platform.mermaid_pie;
import std;
import folia.platform.mermaid_measurer;
export import folia.mermaid.pie_svg;

export namespace folia::platform {
// The resolved theme is shared by measurement and SVG emission. family must
// name one installed font; CSS fallback lists must be resolved by the caller.
// This is an opt-in native API, not a switch of Folia's application renderer.
std::expected<folia::mermaid::PieSvg, folia::mermaid::PieError> render_measured_pie(
    folia::mermaid::PieDocument const& document,
    folia::mermaid::PieSvgTheme const& theme,
    folia::mermaid::PieSvgConfig const& config = {}) {
    using namespace folia::mermaid;
    auto labels = pie_legend_text(document);
    if (!labels) return std::unexpected(labels.error());
    MermaidMeasurer measurer;
    PieMeasurements widths;
    auto measure = [&](std::u16string_view text, double size) -> std::expected<double, PieError> {
        if (!std::isfinite(size) || size <= 0 || size > std::numeric_limits<float>::max())
            return std::unexpected(PieError{0,u"Invalid resolved Pie font size."});
        auto result = measurer.measure(text, {theme.font_family, static_cast<float>(size)});
        if (!result) return std::unexpected(PieError{0,
            std::u16string(result.error().begin(), result.error().end())});
        return result->bounds_width;
    };
    for (auto const& text : *labels) {
        auto width = measure(text, theme.legend_size);
        if (!width) return std::unexpected(width.error());
        widths.legend_widths.push_back(*width);
    }
    auto title = measure(document.title, theme.title_size);
    if (!title) return std::unexpected(title.error());
    widths.title_width = *title;
    return render_pie_svg(document, widths, theme, config);
}
}
