// Mermaid 12.2.0 Gantt measured SVG and d3-time scheduling axis. MIT / ISC; see ../THIRD_PARTY_NOTICES.md.
export module folia.mermaid.gantt_svg;
export import folia.mermaid.gantt;
import std;
export namespace folia::mermaid {
struct GanttConfig {double width=1200,title_margin=25,bar_height=20,bar_gap=4,top_padding=50,left_padding=75,right_padding=75,grid_padding=35,font_size=11,section_font_size=11;std::size_t section_styles=4;bool top_axis=false,compact=false;std::u16string axis_format=u"%Y-%m-%d",tick_interval;};
struct GanttTheme {std::u16string font_family;ConfigValue colors;};
struct GanttRenderInput {GanttDocument document;GanttConfig config;GanttTheme theme;GanttTime current_time=0;};
using GanttMeasure=std::function<std::expected<double,std::string>(std::u16string_view,double)>;
struct GanttSvg {std::string svg;double width=0,height=0;};
std::expected<GanttRenderInput,std::string> prepare_gantt(std::u16string_view,const ConfigSession&);
std::expected<GanttSvg,std::string> render_gantt_svg(const GanttRenderInput&,const GanttMeasure&);
}
