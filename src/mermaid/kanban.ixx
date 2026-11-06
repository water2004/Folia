// Mermaid 12.2.0 Kanban parser/DB. MIT; see ../LICENSE.
export module folia.mermaid.kanban;
export import folia.mermaid.frontend;
import std;
export namespace folia::mermaid {
struct KanbanNode {std::u16string id,label,parent,ticket,priority,assigned,icon;int level=0;bool is_group=false;};
struct KanbanDocument {std::u16string title,acc_title,acc_description;std::vector<KanbanNode> nodes;};
std::expected<KanbanDocument,std::string> parse_kanban(std::u16string_view);
}
