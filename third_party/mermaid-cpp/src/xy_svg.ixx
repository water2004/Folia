// Mermaid 12.2.0 XY chartBuilder and renderer. MIT; see ../LICENSE.
export module folia.mermaid.xy_svg;
export import folia.mermaid.xy;
import std;
export namespace folia::mermaid {
struct XYAxisConfig {
    bool show_label=true,show_title=true,show_tick=true,show_line=true;
    double label_size=14,label_padding=5,title_size=16,title_padding=5,tick_length=5,tick_width=2,line_width=2,rotation=0;
};
struct XYConfig {
    double width=700,height=500,title_size=20,title_padding=10,legend_size=14,legend_padding=10,reserved=50;
    bool horizontal=false,show_title=true,show_legend=true,data_labels=false,outside_labels=false;
    XYAxisConfig x,y;
};
struct XYAxisTheme {std::u16string title,label,tick,line;};
struct XYTheme {std::u16string font_family,title,background,legend,data_label;XYAxisTheme x,y;std::vector<std::u16string> colors;};
struct XYRenderInput {XYDocument document;XYConfig config;XYTheme theme;};
struct XYTextRequest {std::u16string text;double font_size=14;};
struct XYTextBox {double width=0,height=0;};
struct XYMeasurements {std::vector<XYTextBox> text;};
struct XYSvg {std::string svg;double width=0,height=0;};
std::expected<XYRenderInput,std::string> prepare_xy(std::u16string_view,const ConfigSession&);
std::vector<XYTextRequest> xy_measurement_requests(const XYRenderInput&);
std::expected<XYSvg,std::string> render_xy_svg(const XYRenderInput&,const XYMeasurements&);
}
