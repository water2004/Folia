export module folia.mermaid.kanban_svg;
export import folia.mermaid.kanban;
import std;
export namespace folia::mermaid {
struct KanbanConfig {double section_width=200,padding=10,font_size=16;};
struct KanbanTheme {std::u16string font_family,background,node_border,text_color;std::vector<std::u16string> section_fills;};
struct KanbanRenderInput {KanbanDocument document;KanbanConfig config;KanbanTheme theme;};
struct KanbanTextBox {double advance=0,width=0,height=0,baseline=0,line_height=0;};
using KanbanMeasure=std::function<std::expected<KanbanTextBox,std::string>(std::u16string_view,double)>;
struct KanbanSvg {std::string svg;double width=0,height=0;};
std::expected<KanbanRenderInput,std::string> prepare_kanban(std::u16string_view,const ConfigSession&);
std::expected<KanbanSvg,std::string> render_kanban_svg(const KanbanRenderInput&,const KanbanMeasure&);
}
