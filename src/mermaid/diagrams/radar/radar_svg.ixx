// Mermaid 12.2.0 radar/renderer.ts and styles.ts. MIT; see ../LICENSE.
export module folia.mermaid.radar_svg;
export import folia.mermaid.radar;
import std;
export namespace folia::mermaid {
struct RadarConfig {
    double width=600,height=600,top=50,right=50,bottom=50,left=50;
    double axis_scale=1,label_scale=1.05,tension=.17;
    bool use_max_width=true;
};
struct RadarTheme {
    std::u16string font_family,text,title,axis,graticule;
    double font_size=16,axis_width=2,axis_size=12,curve_opacity=.5,curve_width=2;
    double graticule_opacity=.3,graticule_width=1,legend_size=12;
    std::vector<std::u16string> colors;
};
struct RadarRenderInput { RadarDocument document;RadarConfig config;RadarTheme theme; };
struct RadarPoint { double x{},y{}; };
struct RadarCurveGeometry { std::size_t index{};std::vector<RadarPoint> points;std::string path; };
struct RadarSvg { std::string svg;double width{},height{}; };
std::expected<RadarRenderInput,std::string> prepare_radar(std::u16string_view,const ConfigSession&);
std::expected<std::vector<RadarCurveGeometry>,std::string> radar_curve_geometry(const RadarRenderInput&);
// Like the original renderer: configured polar geometry and SVG baselines,
// without glyph-width estimates or an unrelated layout dependency.
std::expected<RadarSvg,std::string> render_radar_svg(const RadarRenderInput&);
}
