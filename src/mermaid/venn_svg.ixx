// Mermaid 12.2.0 Venn SVG. MIT; see THIRD_PARTY_NOTICES.md.
export module folia.mermaid.venn_svg;
import std;
export import folia.mermaid.venn;
export import folia.mermaid.frontend;

export namespace folia::mermaid {
struct VennTheme {
    std::u16string font_family, primary, intersection_text, title, root_fill;
    std::vector<std::u16string> palette;
    bool dark = false;
};
struct VennRenderInput {
    VennDiagram document;
    VennTheme theme;
    double width = 800, height = 450, padding = 8;
    bool use_max_width = true, debug = false, hand_drawn = false;
    std::uint32_t hand_drawn_seed = 0;
};
struct VennSvg { std::string svg; double width, height; };
std::expected<VennRenderInput, std::string> prepare_venn_render(const PreparedDiagram&);
std::expected<VennSvg, std::string> render_venn_svg(const VennRenderInput&);
}
