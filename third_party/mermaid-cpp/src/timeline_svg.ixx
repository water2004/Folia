// Mermaid 12.2.0 Timeline native measured SVG. MIT; see ../LICENSE.
export module folia.mermaid.timeline_svg;
export import folia.mermaid.timeline;
import std;
export namespace folia::mermaid {
struct TimelineConfig {double left_margin=150,padding=50,font_size=16;bool disable_multicolor=false;};
struct TimelineTheme {std::u16string font_family,text_color;std::vector<std::u16string> fills,labels,lines;};
struct TimelineRenderInput {TimelineDocument document;TimelineConfig config;TimelineTheme theme;};
struct TimelineTextBox {double advance=0,width=0,height=0,baseline=0,middle_offset=0;};
using TimelineMeasure=std::function<std::expected<TimelineTextBox,std::string>(std::u16string_view,double,unsigned)>;
struct TimelineSvg {std::string svg;double width=0,height=0;};
std::expected<TimelineRenderInput,std::string> prepare_timeline(std::u16string_view,const ConfigSession&);
std::expected<TimelineSvg,std::string> render_timeline_svg(const TimelineRenderInput&,const TimelineMeasure&);
}
