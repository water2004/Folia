// Mermaid 12.2.0 C4 parser and DB. MIT; see ../LICENSE.
export module folia.mermaid.c4;
export import folia.mermaid.frontend;
import std;
export namespace folia::mermaid {
struct C4Shape {std::u16string alias,label,description,type,parent=u"global";std::optional<std::u16string> technology;std::map<std::u16string,std::u16string> styles;};
struct C4Boundary {std::u16string alias,label,type,parent,node_type,description;std::map<std::u16string,std::u16string> styles;};
struct C4Relation {std::u16string type,from,to,label,technology,description;std::map<std::u16string,std::u16string> styles;};
struct C4Document {std::u16string type,title,acc_title,acc_description;std::vector<C4Shape> shapes;std::vector<C4Boundary> boundaries;std::vector<C4Relation> relations;int shapes_in_row=4,boundaries_in_row=2;};
std::expected<C4Document,std::string> parse_c4(std::u16string_view);
}
