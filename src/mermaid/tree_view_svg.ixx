// Mermaid 12.2.0 treeView renderer. Built-in file/folder glyphs; MIT.
export module folia.mermaid.tree_view_svg;
import std;
export import folia.mermaid.tree_view;
export import folia.mermaid.svg_text_measure;
export namespace folia::mermaid {
struct TreeViewSvg { std::string svg; double width{},height{}; };
std::expected<TreeViewSvg,std::string> render_tree_view_svg(const TreeViewRenderInput&,const SvgTextMeasure&);
}
