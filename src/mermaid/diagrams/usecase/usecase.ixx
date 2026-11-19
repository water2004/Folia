// Mermaid 12.2.0 usecase diagram model and parser.
export module folia.mermaid.usecase;
import std;
export import folia.mermaid.frontend;

export namespace folia::mermaid {
enum class UsecaseNodeKind { actor, usecase, boundary, note, json };
enum class UsecaseShape { ellipse, rectangle, package };
enum class UsecaseActor { normal, hollow, awesome, icon };
struct UsecaseNode {
    std::u16string id, label, parent, stereotype, icon, note_target;
    UsecaseNodeKind kind = UsecaseNodeKind::usecase;
    UsecaseShape shape = UsecaseShape::ellipse;
    UsecaseActor actor = UsecaseActor::normal;
    bool markdown = false, business = false;
    std::vector<std::u16string> classes, styles;
    struct JsonRow { std::u16string key, value; };
    std::vector<JsonRow> json_rows;
};
enum class UsecaseRelationship { association, include, extend, generalization, note };
enum class UsecaseArrow { none, point, circle, cross };
struct UsecaseEdge {
    std::u16string id, from, to, label;
    UsecaseRelationship kind = UsecaseRelationship::association;
    UsecaseArrow arrow = UsecaseArrow::point;
    std::size_t min_length = 1;
    bool reverse = false, markdown = false;
    std::u16string animation;
    std::vector<std::u16string> classes, styles;
};
struct UsecaseDocument {
    std::u16string direction = u"LR", title, acc_title, acc_description;
    std::vector<UsecaseNode> nodes;
    std::vector<UsecaseEdge> edges;
    std::map<std::u16string, std::vector<std::u16string>> class_styles;
};
std::expected<UsecaseDocument,std::string> parse_usecase(std::u16string_view source);
}
