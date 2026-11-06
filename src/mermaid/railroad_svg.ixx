// Mermaid 12.2.0 Railroad renderer. MIT; see THIRD_PARTY_NOTICES.md.
export module folia.mermaid.railroad_svg;
import std;
export import folia.mermaid.frontend;
export import folia.mermaid.railroad;
export import folia.mermaid.svg_text_measure;

export namespace folia::mermaid {
struct RailroadSvgStyle {
    std::u16string fill, stroke, text;
};

struct RailroadSvgTheme {
    std::u16string font_family = u"monospace";
    double font_size = 14;
    RailroadSvgStyle terminal{u"#FFFFC0", u"#000000", u"#000000"};
    RailroadSvgStyle nonterminal{u"#FFFFFF", u"#000000", u"#000000"};
    RailroadSvgStyle special{u"#F0E0FF", u"#8800CC", u"#000000"};
    std::u16string line = u"#000000", marker = u"#000000", rule_name = u"#000066";
};

struct RailroadRenderInput {
    RailroadDocument document;
    RailroadSvgTheme theme;
    std::u16string type = u"railroad";
    double padding = 10, vertical_separation = 8, horizontal_separation = 10;
    double arc_radius = 10, marker_radius = 5, stroke_width = 2;
    bool use_max_width = true;
};

struct RailroadSvg {
    std::string svg;
    double width{}, height{};
};

std::expected<RailroadRenderInput, std::string> prepare_railroad_render(const PreparedDiagram&);
std::expected<RailroadSvg, std::string> render_railroad_svg(
    const RailroadRenderInput&, const SvgTextMeasure&);
}
