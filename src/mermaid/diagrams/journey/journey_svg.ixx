// Mermaid 12.2.0 Journey native SVG projection. MIT; see ../LICENSE.
export module folia.mermaid.journey_svg;
export import folia.mermaid.journey;
import std;
export namespace folia::mermaid {
struct JourneyConfig {double margin_x=50,margin_y=10,left_margin=150,max_label_width=360,width=150,height=50,task_margin=50,text_margin=5,task_font_size=14;std::u16string text_placement=u"fo",title_font_size=u"4ex";};
struct JourneyTheme {std::u16string font_family,task_family,title_family,text_color,title_color,face_color;std::vector<std::u16string> actor_colors,section_fills,section_colors;};
struct JourneyRenderInput {JourneyDocument document;JourneyConfig config;JourneyTheme theme;};
struct JourneyTextBox {double width=0,height=0,baseline=0,line_height=0,middle_offset=0;};
using JourneyMeasure=std::function<std::expected<JourneyTextBox,std::string>(std::u16string_view,double,std::u16string_view,unsigned)>;
struct JourneySvg {std::string svg;double width=0,height=0;};
std::expected<JourneyRenderInput,std::string> prepare_journey(std::u16string_view,const ConfigSession&);
std::expected<JourneySvg,std::string> render_journey_svg(const JourneyRenderInput&,const JourneyMeasure&);
}
