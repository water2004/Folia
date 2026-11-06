// Mermaid 12.2.0 Event Modeling SVG renderer. MIT; see THIRD_PARTY_NOTICES.md.
export module folia.mermaid.eventmodeling_svg;
import std;
export import folia.mermaid.eventmodeling;
export import folia.mermaid.frontend;
import folia.mermaid.svg_text_measure;

export namespace folia::mermaid {

struct EventmodelingTheme {
    std::u16string font_family = u"Arial";
    std::u16string text = u"#333333";
    std::u16string lane_fill = u"rgb(250,250,250)", lane_stroke = u"rgb(240,240,240)";
    std::u16string relation_stroke = u"#000000", arrowhead = u"#000000";
    std::array<std::u16string, 5> fills{u"white", u"#edb3f6", u"#d3f1a2", u"#bcd6fe", u"#ffb778"};
    std::array<std::u16string, 5> strokes{u"#dbdada", u"#b88cbf", u"#a3b732", u"#679ac3", u"#c19a0f"};
};

struct EventmodelingRenderInput {
    EventmodelingDiagram document;
    EventmodelingTheme theme;
    double padding = 30;
    bool use_max_width = true;
};

struct EventmodelingSvg {
    std::string svg;
    double width{}, height{};
};

using EventmodelingTextMeasure = std::function<std::expected<SvgTextMetrics, std::string>(
    std::u16string_view text, std::u16string_view family, double size, unsigned weight)>;

std::expected<EventmodelingRenderInput, std::string> prepare_eventmodeling_render(const PreparedDiagram&);
std::expected<EventmodelingSvg, std::string> render_eventmodeling_svg(
    const EventmodelingRenderInput&, const EventmodelingTextMeasure&);

}
