#include "support/folia_test.hpp"
import folia.mermaid.er;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"

void same_er_text(std::u16string_view actual, const ConfigValue& object, std::u16string_view key) {
    expect(bool(actual == field(object, key).string)) << hex(key) << hex(actual) << hex(field(object, key).string);
}
void same_er_list(const std::vector<std::u16string>& actual, const ConfigValue& object, std::u16string_view key) {
    const auto& expected = field(object, key).sequence;
    expect(actual.size() == expected.size()) << hex(key);
    for (std::size_t i = 0; i < std::min(actual.size(), expected.size()); ++i)
        expect(bool(actual[i] == expected[i].string)) << hex(key) << hex(actual[i]) << hex(expected[i].string);
}

suite er_regressions = [] {
    "mermaid_er_official_complex_examples_match_database"_test = [] {
        std::ifstream input{std::filesystem::path{FOLIA_MERMAID_FIXTURE_DIR} / "er-regressions.tsv"};
        expect(fatal(input.is_open()));
        std::string line;
        std::size_t count = 0;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.front() == '#') continue;
            std::vector<std::string_view> fields;
            std::string_view rest{line};
            for (;;) {
                const auto tab = rest.find('\t');
                fields.push_back(rest.substr(0, tab));
                if (tab == rest.npos) break;
                rest.remove_prefix(tab + 1);
            }
            expect(fatal(fields.size() == 4_u));
            ++count;
            auto session = ConfigSession::create();
            expect(fatal(session.has_value()));
            const auto site = json(decode(fields[2]));
            expect(fatal(session->save_initialize_config(site).has_value()));
            expect(fatal(session->set_site_config(site).has_value()));
            const auto expected = json(decode(fields[3]));
            const auto actual = prepare_er(decode(fields[1]), *session);
            expect(actual.has_value()) << fields[0] << (actual ? "" : actual.error());
            if (!actual) continue;
            same_er_text(actual->direction, expected, u"direction");
            const auto& entities = field(expected, u"entities").sequence;
            expect(fatal(actual->entities.size() == entities.size())) << fields[0];
            for (std::size_t i = 0; i < actual->entities.size(); ++i) {
                const auto& a = actual->entities[i];
                const auto& e = entities[i];
                same_er_text(a.name, e, u"name"); same_er_text(a.id, e, u"id");
                same_er_text(a.alias, e, u"alias"); same_er_text(a.parent, e, u"parent");
                same_er_text(a.classes, e, u"classes");
                same_er_list(a.styles, e, u"styles"); same_er_list(a.compiled_styles, e, u"compiledStyles");
                const auto& attributes = field(e, u"attributes").sequence;
                expect(fatal(a.attributes.size() == attributes.size())) << fields[0];
                for (std::size_t j = 0; j < a.attributes.size(); ++j) {
                    const auto& aa = a.attributes[j]; const auto& ea = attributes[j];
                    same_er_text(aa.type, ea, u"type"); same_er_text(aa.name, ea, u"name");
                    same_er_text(aa.comment, ea, u"comment"); same_er_list(aa.keys, ea, u"keys");
                }
            }
            const auto& groups = field(expected, u"groups").sequence;
            expect(fatal(actual->groups.size() == groups.size())) << fields[0];
            for (std::size_t i = 0; i < actual->groups.size(); ++i) {
                const auto& a = actual->groups[i]; const auto& e = groups[i];
                same_er_text(a.id, e, u"id"); same_er_text(a.title, e, u"title");
                same_er_text(a.parent, e, u"parent"); same_er_text(a.direction, e, u"direction");
                same_er_list(a.members, e, u"members"); same_er_list(a.classes, e, u"classes");
                same_er_list(a.styles, e, u"styles"); same_er_list(a.compiled_styles, e, u"compiledStyles");
            }
            const auto& relationships = field(expected, u"relationships").sequence;
            expect(fatal(actual->relationships.size() == relationships.size())) << fields[0];
            for (std::size_t i = 0; i < actual->relationships.size(); ++i) {
                const auto& a = actual->relationships[i]; const auto& e = relationships[i];
                same_er_text(a.entity_a, e, u"entityA"); same_er_text(a.entity_b, e, u"entityB");
                same_er_text(a.role_a, e, u"roleA"); same_er_text(a.label, e, u"roleA");
                const auto& spec = field(e, u"relSpec");
                same_er_text(a.rel_type, spec, u"relType");
                same_er_text(a.card_a, spec, u"cardA"); same_er_text(a.card_b, spec, u"cardB");
            }
        }
        expect(count >= 9_u);
    };
    "mermaid_er_attribute_grammar_and_alias_identity"_test = [] {
        auto session = ConfigSession::create();
        expect(fatal(session.has_value()));
        const auto actual = prepare_er(uR"(erDiagram
"User Profile"[Profile] { string[] names int? age string id PK, FK "shared key" }
"User Profile"["Later alias"]
"User Profile" ||..o{ "Login Session" : "has sessions"
)", *session);
        expect(fatal(actual.has_value())) << (actual ? "" : actual.error());
        expect(actual->entities.size() == 2_u);
        expect(bool(actual->entities.front().alias == u"Profile"));
        expect(actual->entities.front().attributes.size() == 3_u);
        expect(actual->entities.front().attributes.back().keys.size() == 2_u);
        expect(bool(actual->relationships.front().label == u"has sessions"));
        const auto tight=prepare_er(u"erDiagram\nA o{--|| B : owns\nB one--o| C : uses",*session);
        expect(fatal(tight.has_value())) << (tight?"":tight.error());
        expect(tight->relationships.size()==2_u);
        expect(bool(tight->relationships[0].card_b==u"ZERO_OR_MORE"));
        expect(bool(tight->relationships[1].card_a==u"ZERO_OR_ONE"));
        for (const auto source : {
            u"erDiagram\nA ||--o{ B", u"erDiagram\nA { int id PK, }",
            u"erDiagram\nA { int }", u"erDiagram\nsubgraph G\nA",
            u"erDiagram\nend", u"erDiagram\nA [\"unclosed]"})
            expect(!prepare_er(source, *session)) << hex(source);
        ErLimits limits;
        limits.groups = 1;
        expect(!prepare_er(u"erDiagram\nsubgraph A\nsubgraph B\nx\nend\nend", *session, limits));
    };
};
}
