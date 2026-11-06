// Mermaid 12.2.0 requirement diagram parser and DB projection. MIT; see ../LICENSE.
export module folia.mermaid.requirement;
import std;
export namespace folia::mermaid {
struct RequirementRequirement {
    std::u16string name, type, requirement_id, text, risk, verify_method;
    std::vector<std::u16string> classes{u"default"}, css_styles;
};
struct RequirementElement {
    std::u16string name, type, doc_ref;
    std::vector<std::u16string> classes{u"default"}, css_styles;
};
struct RequirementRelationship { std::u16string type, source, target; };
struct RequirementClass { std::u16string id; std::vector<std::u16string> styles, text_styles; };
struct RequirementDocument {
    std::u16string direction=u"TB", title, acc_title, acc_description;
    std::vector<RequirementRequirement> requirements;
    std::vector<RequirementElement> elements;
    std::vector<RequirementRelationship> relationships;
    std::vector<RequirementClass> classes;
};
std::expected<RequirementDocument,std::string> parse_requirement(std::u16string_view source);
}
