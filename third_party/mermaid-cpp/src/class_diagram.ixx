// Mermaid 12.2.0 Class grammar, ClassDB and ClassMember.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see ../LICENSE.
export module folia.mermaid.class_diagram;
import std;
export import folia.mermaid.frontend;

export namespace folia::mermaid {
enum class ClassMarker { none = -1, aggregation = 0, extension = 1, composition = 2, dependency = 3 };
struct ClassMember {
    std::u16string id, visibility, classifier, parameters, return_type;
    std::u16string text, display_text, css_style;
};
struct ClassNode {
    std::u16string id, label, type;
    std::vector<std::u16string> annotations;
    std::vector<ClassMember> members, methods;
};
struct ClassRelation {
    std::u16string from, to, title, start_label = u"none", end_label = u"none";
    ClassMarker start = ClassMarker::none, end = ClassMarker::none;
    bool dotted = false;
};
struct ClassNote { std::u16string id, class_id, text; };
struct ClassInput {
    std::u16string title, acc_title, acc_description, direction = u"TB";
    std::vector<ClassNode> classes;
    std::vector<ClassRelation> relations;
    std::vector<ClassNote> notes;
    ConfigValue effective_config;
};
struct ClassLimits {
    FrontendLimits frontend;
    std::size_t classes = 10000, members = 10000, relations = 10000, notes = 10000;
};
// Owned Class declarations, attributes/methods and ordinary relationships.
// Unsupported statements fail explicitly; rendering is a separate boundary.
std::expected<ClassInput, std::string> prepare_class(
    std::u16string_view source, const ConfigSession& session, ClassLimits limits = {});
}
