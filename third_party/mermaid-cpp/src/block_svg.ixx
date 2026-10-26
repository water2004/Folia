export module folia.mermaid.block_svg;
export import folia.mermaid.block;
import std;
export namespace folia::mermaid {
struct BlockConfig {double padding=8,font_size=16;};
struct BlockTheme {std::u16string font_family,fill,stroke,text_color,line_color,cluster_fill,cluster_stroke;};
struct BlockRenderInput {BlockDocument document;BlockConfig config;BlockTheme theme;};
struct BlockTextBox {double advance=0,width=0,height=0,baseline=0,line_height=0;};
using BlockMeasure=std::function<std::expected<BlockTextBox,std::string>(std::u16string_view,double)>;
struct BlockSvg {std::string svg;double width=0,height=0;};
std::expected<BlockRenderInput,std::string> prepare_block(std::u16string_view,const ConfigSession&);
std::expected<BlockSvg,std::string> render_block_svg(const BlockRenderInput&,const BlockMeasure&);
}
