export module folia.mermaid.c4_svg;
export import folia.mermaid.c4;
import std;
export namespace folia::mermaid {
struct C4Font {std::u16string family;double size=14;unsigned weight=400;bool italic=false;};
struct C4Config {double margin_x=50,margin_y=10,shape_margin=50,shape_padding=20,width=216,next_line_padding=0;bool wrap=true;ConfigValue original;};
struct C4Theme {std::u16string font_family,text_color;double font_size=16;};
struct C4RenderInput {C4Document document;C4Config config;C4Theme theme;};
struct C4TextBox {double advance=0,width=0,height=0,baseline=0,middle_offset=0;};
using C4Measure=std::function<std::expected<C4TextBox,std::string>(std::u16string_view,const C4Font&)>;
struct C4Measurements {C4Measure measure;double available_width=0;};
struct C4Svg {std::string svg;double width=0,height=0;};
std::expected<C4RenderInput,std::string> prepare_c4(std::u16string_view,const ConfigSession&);
std::expected<C4Svg,std::string> render_c4_svg(const C4RenderInput&,const C4Measurements&);
}
