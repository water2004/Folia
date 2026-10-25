#include "support/folia_test.hpp"
import folia.mermaid.sequence;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"
void same(std::u16string_view actual, const ConfigValue& expected, std::u16string_view key) {
    expect(bool(actual == field(expected,key).string)) << hex(key) << hex(actual) << hex(field(expected,key).string);
}
suite sequence = [] {
    "mermaid_sequence_matches_original_actors_messages_and_activations"_test = [] {
        const auto override = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override ? override : FOLIA_MERMAID_FIXTURE_DIR}/"sequence.tsv"};
        expect(fatal(input.is_open()));
        std::string line; std::size_t count = 0;
        while (std::getline(input,line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.front() == '#') continue;
            std::vector<std::string_view> fields; std::string_view rest{line};
            for (;;) { auto tab = rest.find('\t'); fields.push_back(rest.substr(0,tab)); if (tab == rest.npos) break; rest.remove_prefix(tab+1); }
            expect(fatal(fields.size() == 4_u)); ++count;
            auto session = ConfigSession::create(); expect(fatal(session.has_value()));
            const auto site = json(decode(fields[2]));
            expect(fatal(session->save_initialize_config(site).has_value()));
            expect(fatal(session->set_site_config(site).has_value()));
            const auto expected = json(decode(fields[3]));
            auto actual = prepare_sequence(decode(fields[1]),*session);
            if (expected.kind == ConfigKind::null) { expect(!actual) << fields[0]; continue; }
            expect(actual.has_value()) << fields[0] << (actual ? "" : actual.error());
            if (!actual) continue;
            same(actual->title,expected,u"title"); same(actual->acc_title,expected,u"accTitle");
            same(actual->acc_description,expected,u"accDescription");
            const auto& actors = field(expected,u"actors").sequence;
            expect(fatal(actual->actors.size() == actors.size())) << fields[0];
            for (std::size_t i = 0; i < actors.size(); ++i) {
                const auto& a = actual->actors[i]; const auto& e = actors[i];
                same(a.id,e,u"id"); same(a.name,e,u"name"); same(a.description,e,u"description"); same(a.type,e,u"type");
                expect(a.wrap == field(e,u"wrap").boolean) << fields[0];
            }
            const auto& messages = field(expected,u"messages").sequence;
            expect(fatal(actual->messages.size() == messages.size())) << fields[0];
            for (std::size_t i = 0; i < messages.size(); ++i) {
                const auto& m = actual->messages[i]; const auto& e = messages[i];
                same(m.id,e,u"id"); same(m.from,e,u"from"); same(m.to,e,u"to");
                if(m.type==SequenceKind::autonumber) {
                    const auto& number=field(e,u"message");
                    for(auto [key,value]:{std::pair{u"start",m.number_start},{u"step",m.number_step}}) {
                        const auto* expected_number=number.find(key);
                        expect(value.has_value()==(expected_number!=nullptr));
                        if(value&&expected_number) expect(*value==expected_number->number);
                    }
                    expect(m.number_visible==field(number,u"visible").boolean);
                } else same(m.message,e,u"message");
                expect(static_cast<int>(m.type) == field(e,u"type").number);
                expect(m.wrap == field(e,u"wrap").boolean); expect(m.activate == field(e,u"activate").boolean);
                expect(m.placement == field(e,u"placement").number);
            }
        }
        expect(count == 40_u);
    };
    "mermaid_sequence_owned_requests_and_unsupported_statements"_test = [] {
        auto session = ConfigSession::create(); expect(fatal(session.has_value()));
        const auto site = json(uR"({"theme":"default"})");
        expect(fatal(session->save_initialize_config(site).has_value()));
        expect(fatal(session->set_site_config(site).has_value()));
        std::u16string source = u"sequenceDiagram\nA->>+B: Save\nB-->>-A: Done";
        auto first = prepare_sequence(source,*session);
        expect(fatal(first.has_value())) << (first ? "" : first.error());
        source.clear();
        expect(bool(first->messages[0].message == u"Save"));
        for (const auto bad : {u"sequenceDiagram\nautonumber invalid\nA->>B: Go",
            u"sequenceDiagram\nparticipant A@{type:database}",u"sequenceDiagram\nA->>-B: Invalid"})
            expect(!prepare_sequence(bad,*session));
        SequenceLimits limits; limits.actors = 1;
        expect(!prepare_sequence(u"sequenceDiagram\nA->>B: Go",*session,limits));
        auto second = prepare_sequence(u"sequenceDiagram\nA->>B: New",*session);
        expect(fatal(second.has_value()));
        expect(second->messages.size() == 1_u); expect(bool(second->actors[1].description == u"B"));
    };
};
}
