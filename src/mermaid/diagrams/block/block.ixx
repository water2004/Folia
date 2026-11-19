// Mermaid 12.2.0 Block parser/DB. MIT; see ../LICENSE.
export module folia.mermaid.block;
export import folia.mermaid.frontend;
import std;
export namespace folia::mermaid {
struct BlockNode {std::u16string id,type=u"na",label;int columns=-1,span=1;std::vector<std::u16string> classes,styles,directions;std::vector<std::size_t> children;};
struct BlockEdge {std::u16string id,start,end,label,thickness=u"normal",pattern=u"solid",arrow_end=u"",arrow_start=u"arrow_open";};
struct BlockClass {std::u16string id;std::vector<std::u16string> styles,text_styles;};
struct BlockDocument {std::u16string title,acc_title,acc_description;std::vector<BlockNode> nodes;std::vector<BlockEdge> edges;std::vector<BlockClass> classes;};
std::expected<BlockDocument,std::string> parse_block(std::u16string_view);
}
