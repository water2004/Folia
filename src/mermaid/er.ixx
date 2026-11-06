// Mermaid 12.2.0 ER grammar and ER database projection.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see ../LICENSE.
export module folia.mermaid.er;
import std;
export import folia.mermaid.frontend;

export namespace folia::mermaid {
struct ErAttribute {
    std::u16string type, name;
    std::vector<std::u16string> keys;
    std::u16string comment;
};
struct ErEntity {
    std::u16string name, id, alias;
    std::vector<ErAttribute> attributes;
};
struct ErRelationship {
    std::u16string entity_a, role_a, entity_b;
    std::u16string rel_type, card_a, card_b, label;
};
struct ErInput {
    std::u16string direction = u"TB";
    std::u16string title, acc_title, acc_description;
    std::vector<ErEntity> entities;
    std::vector<ErRelationship> relationships;
    ConfigValue effective_config;
};
struct ErLimits {
    FrontendLimits frontend;
    std::size_t entities = 10000, attributes = 100000, relationships = 10000;
};
std::expected<ErInput, std::string> prepare_er(
    std::u16string_view source, const ConfigSession& session, ErLimits limits = {});
}
