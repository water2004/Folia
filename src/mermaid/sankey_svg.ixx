// Mermaid 12.2.0 Sankey renderer. MIT; see ../LICENSE.
export module folia.mermaid.sankey_svg;
export import folia.mermaid.sankey;
import std;
export namespace folia::mermaid {
struct SankeyTheme {std::u16string font_family,text_color,background;};
struct SankeyRenderInput {SankeyGraph graph;SankeyConfig config;SankeyTheme theme;};
struct SankeyTextBox {double width=0,height=0,baseline=0;};
struct SankeyMeasurements {std::vector<SankeyTextBox> labels;};
struct SankeySvg {std::string svg;double width=0,height=0;};
std::expected<SankeyRenderInput,std::string> prepare_sankey(std::u16string_view,const ConfigSession&);
// One plain 14px label per node; newline is collapsed as in SVG text content.
std::vector<std::u16string> sankey_measurement_requests(const SankeyRenderInput&);
std::expected<SankeySvg,std::string> render_sankey_svg(const SankeyRenderInput&,const SankeyMeasurements&);
}
