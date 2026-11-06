export module folia.mermaid.state_svg;
export import folia.mermaid.state;
export import folia.mermaid.flowchart_svg;
import std;
export namespace folia::mermaid {
struct StateTheme {std::u16string font_family,fill,border,text,line,special,composite_title,composite,alternate,edge_text,edge_background,note_fill,note_border,note_text,fork_join;double font_size=16;};
struct StateRenderInput {FlowchartInput document;std::vector<StateAnnotation> annotations;StateTheme theme;double minimum_width=120,wrapping_width=120,title_margin=25,note_margin=10,note_node_padding=15;};
struct StateTextBox {double advance=0,width=0,height=0,baseline=0,line_height=0;};
using StateMeasure=std::function<std::expected<StateTextBox,std::string>(std::u16string_view,double)>;
struct StateSvg {std::string svg;double width=0,height=0;};
std::expected<StateRenderInput,std::string> prepare_state(std::u16string_view,const ConfigSession&);
std::expected<StateSvg,std::string> render_state_svg(const StateRenderInput&,const StateMeasure&);
}
