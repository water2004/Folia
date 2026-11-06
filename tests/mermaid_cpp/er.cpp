#include "support/folia_test.hpp"
import folia.mermaid.er;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"
void same(std::u16string_view actual, const ConfigValue& expected, std::u16string_view key) {
    expect(bool(actual == field(expected,key).string)) << hex(key) << hex(actual) << hex(field(expected,key).string);
}
suite er = [] {
    "mermaid_er_matches_original_parser_and_database"_test = [] {
        std::ifstream input{std::filesystem::path{FOLIA_MERMAID_FIXTURE_DIR}/"er.tsv"};
        expect(fatal(input.is_open()));
        std::string line; std::size_t count = 0, successes = 0, failures = 0;
        while (std::getline(input,line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.front() == '#') continue;
            std::vector<std::string_view> fields; std::string_view rest{line};
            for (;;) { const auto tab = rest.find('\t'); fields.push_back(rest.substr(0,tab)); if (tab == rest.npos) break; rest.remove_prefix(tab+1); }
            expect(fatal(fields.size() == 4_u)); ++count;
            auto session = ConfigSession::create(); expect(fatal(session.has_value()));
            const auto site = json(decode(fields[2]));
            expect(fatal(session->save_initialize_config(site).has_value()));
            expect(fatal(session->set_site_config(site).has_value()));
            const auto expected = json(decode(fields[3]));
            const auto actual = prepare_er(decode(fields[1]),*session);
            if (expected.kind == ConfigKind::null) { ++failures; expect(!actual) << fields[0]; continue; }
            ++successes;
            expect(actual.has_value()) << fields[0] << (actual?"":actual.error());
            if (!actual) continue;
            same(actual->direction,expected,u"direction");
            const auto& entities = field(expected,u"entities").sequence;
            expect(fatal(actual->entities.size() == entities.size())) << fields[0];
            for (std::size_t i = 0; i < entities.size() && i < actual->entities.size(); ++i) {
                const auto& a = actual->entities[i]; const auto& e = entities[i];
                same(a.name,e,u"name"); same(a.id,e,u"id"); same(a.alias,e,u"alias");
                const auto& attributes = field(e,u"attributes").sequence;
                expect(fatal(a.attributes.size() == attributes.size()));
                for (std::size_t j = 0; j < attributes.size() && j < a.attributes.size(); ++j) {
                    const auto& aa = a.attributes[j]; const auto& ea = attributes[j];
                    same(aa.type,ea,u"type"); same(aa.name,ea,u"name"); same(aa.comment,ea,u"comment");
                    const auto& keys = field(ea,u"keys").sequence;
                    expect(fatal(aa.keys.size() == keys.size()));
                    for (std::size_t k = 0; k < keys.size() && k < aa.keys.size(); ++k)
                        expect(bool(aa.keys[k] == keys[k].string)) << hex(aa.keys[k]) << hex(keys[k].string);
                }
            }
            const auto& relationships = field(expected,u"relationships").sequence;
            expect(fatal(actual->relationships.size() == relationships.size())) << fields[0];
            for (std::size_t i = 0; i < relationships.size() && i < actual->relationships.size(); ++i) {
                const auto& a = actual->relationships[i]; const auto& e = relationships[i];
                same(a.entity_a,e,u"entityA"); same(a.role_a,e,u"roleA"); same(a.label,e,u"roleA"); same(a.entity_b,e,u"entityB");
                same(a.rel_type,field(e,u"relSpec"),u"relType");
                same(a.card_a,field(e,u"relSpec"),u"cardA");
                same(a.card_b,field(e,u"relSpec"),u"cardB");
            }
        }
        expect(count == 10_u); expect(successes == 8_u); expect(failures == 2_u);
    };
    "mermaid_er_owned_requests_and_limits"_test = [] {
        auto session = ConfigSession::create(); expect(fatal(session.has_value()));
        const auto site = json(uR"({"theme":"default"})");
        expect(fatal(session->save_initialize_config(site).has_value()));
        expect(fatal(session->set_site_config(site).has_value()));
        std::u16string source = u"erDiagram\nA ||--o{ B : owns";
        auto first = prepare_er(source,*session); expect(fatal(first.has_value())); source.clear();
        expect(bool(first->relationships[0].label == u"owns"));
        ErLimits limits; limits.entities = 1;
        expect(!prepare_er(u"erDiagram\nA ||--o{ B",*session,limits));
        const auto second = prepare_er(u"erDiagram\nA",*session);
        expect(fatal(second.has_value())); expect(second->entities.size() == 1_u);
        expect(first->entities.size() == 2_u);
    };
};
}
