#include "support/folia_test.hpp"
import folia.mermaid.class_diagram;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"
void same(std::u16string_view actual, const ConfigValue& expected, std::u16string_view key) {
    expect(bool(actual == field(expected,key).string)) << hex(key) << hex(actual) << hex(field(expected,key).string);
}
void members(const std::vector<ClassMember>& actual, const ConfigValue& expected) {
    expect(fatal(actual.size() == expected.sequence.size()));
    for (std::size_t i = 0; i < actual.size(); ++i) {
        const auto& a = actual[i]; const auto& e = expected.sequence[i];
        same(a.id,e,u"id"); same(a.visibility,e,u"visibility"); same(a.classifier,e,u"classifier");
        same(a.parameters,e,u"parameters"); same(a.return_type,e,u"returnType");
        same(a.text,e,u"text"); same(a.display_text,e,u"displayText"); same(a.css_style,e,u"cssStyle");
    }
}
suite class_diagram = [] {
    "mermaid_class_matches_original_members_and_relationships"_test = [] {
        std::ifstream input{std::filesystem::path{FOLIA_MERMAID_FIXTURE_DIR}/"class.tsv"};
        expect(fatal(input.is_open()));
        std::string line; std::size_t count = 0;
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
            const auto actual = prepare_class(decode(fields[1]),*session);
            if (expected.kind == ConfigKind::null) { expect(!actual) << fields[0]; continue; }
            expect(actual.has_value()) << fields[0] << (actual?"":actual.error());
            if (!actual) continue;
            same(actual->title,expected,u"title"); same(actual->acc_title,expected,u"accTitle");
            same(actual->acc_description,expected,u"accDescription"); same(actual->direction,expected,u"direction");
            const auto& nodes = field(expected,u"classes").sequence;
            expect(fatal(actual->classes.size() == nodes.size())) << fields[0];
            for (std::size_t i = 0; i < nodes.size(); ++i) {
                const auto& a = actual->classes[i]; const auto& e = nodes[i];
                same(a.id,e,u"id"); same(a.label,e,u"label"); same(a.type,e,u"type"); same(a.parent,e,u"parent");
                const auto& annotations = field(e,u"annotations").sequence;
                expect(fatal(a.annotations.size() == annotations.size()));
                for (std::size_t j = 0; j < annotations.size(); ++j) expect(bool(a.annotations[j] == annotations[j].string));
                members(a.members,field(e,u"members")); members(a.methods,field(e,u"methods"));
            }
            const auto& relations = field(expected,u"relations").sequence;
            const auto& notes = field(expected,u"notes").sequence;
            expect(fatal(actual->notes.size() == notes.size())) << fields[0];
            for (std::size_t i = 0; i < notes.size(); ++i) {
                same(actual->notes[i].id,notes[i],u"id");
                same(actual->notes[i].class_id,notes[i],u"class");
                same(actual->notes[i].text,notes[i],u"text");
                same(actual->notes[i].parent,notes[i],u"parent");
            }
            const auto& namespaces = field(expected,u"namespaces").sequence;
            expect(fatal(actual->namespaces.size() == namespaces.size())) << fields[0];
            for (std::size_t i = 0; i < namespaces.size(); ++i) {
                const auto& a = actual->namespaces[i]; const auto& e = namespaces[i];
                same(a.id,e,u"id"); same(a.label,e,u"label"); same(a.parent,e,u"parent");
                expect(a.explicit_declaration == field(e,u"explicit").boolean);
                for (auto [key,values] : {std::pair{u"classes",&a.classes},{u"notes",&a.notes},{u"children",&a.children}}) {
                    const auto& entries = field(e,key).sequence;
                    expect(fatal(values->size() == entries.size()));
                    for (std::size_t j = 0; j < entries.size(); ++j) expect(bool((*values)[j] == entries[j].string));
                }
            }
            expect(fatal(actual->relations.size() == relations.size())) << fields[0];
            for (std::size_t i = 0; i < relations.size(); ++i) {
                const auto& a = actual->relations[i]; const auto& e = relations[i];
                same(a.from,e,u"from"); same(a.to,e,u"to"); same(a.title,e,u"title");
                same(a.start_label,e,u"startLabel"); same(a.end_label,e,u"endLabel");
                expect(static_cast<int>(a.start) == field(e,u"start").number);
                expect(static_cast<int>(a.end) == field(e,u"end").number);
                expect(a.dotted == field(e,u"dotted").boolean);
            }
        }
        expect(count == 20_u);
    };
    "mermaid_class_owned_requests_and_explicit_boundary"_test = [] {
        auto session = ConfigSession::create(); expect(fatal(session.has_value()));
        const auto site = json(uR"({"theme":"default"})");
        expect(fatal(session->save_initialize_config(site).has_value()));
        expect(fatal(session->set_site_config(site).has_value()));
        std::u16string source = u"classDiagram\nclass Document {\n+save() bool\n}";
        auto first = prepare_class(source,*session); expect(fatal(first.has_value())); source.clear();
        expect(bool(first->classes[0].methods[0].display_text == u"+save() : bool"));
        expect(!prepare_class(u"classDiagram\nnamespace Storage {\nclass Document",*session));
        ClassLimits limits; limits.classes = 1;
        expect(!prepare_class(u"classDiagram\nA --> B",*session,limits));
        const auto second = prepare_class(u"classDiagram\nclass Store",*session);
        expect(fatal(second.has_value())); expect(second->classes.size() == 1_u);
        expect(first->classes[0].methods.size() == 1_u);
    };
};
}
