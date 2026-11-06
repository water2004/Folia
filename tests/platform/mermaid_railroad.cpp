#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;

namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;

struct RailroadCase {
    std::string_view name;
    std::u16string_view source;
};

constexpr std::array cases{
    RailroadCase{"ir-simple", u"railroad-beta\ndigit = terminal('0');\n"},
    RailroadCase{"ir-nested",
        u"railroad-beta\n"
        u"accTitle: JSON values\n"
        u"accDescr: Recursive grammar\n"
        u"object = sequence(terminal('{'), optional(sequence(nonterminal('member'), "
        u"zeroOrMore(sequence(terminal(','), nonterminal('member'))))), terminal('}'));\n"
        u"member = sequence(nonterminal('string'), terminal(':'), "
        u"choice(nonterminal('number'), terminal('true'), special('anything')));\n"},
    RailroadCase{"ebnf",
        u"railroad-ebnf-beta\n"
        u"identifier = letter, { letter | digit | '_' };\n"
        u"letter = ? Unicode letter ? ;\n"
        u"digit = '0' | '1' | '2';\n"},
    RailroadCase{"abnf",
        u"railroad-abnf-beta\n"
        u"scheme = ALPHA *( ALPHA / DIGIT / \"+\" / \"-\" / \".\" ) ;\n"
        u"digits = 1*3DIGIT ;\n"},
    RailroadCase{"peg",
        u"railroad-peg-beta\n"
        u"Expression <- Term (('+' / '-') Term)* ;\n"
        u"Term <- Digit+ ('x' Digit)? ;\n"
        u"Digit <- '0' / '1' / '2' ;\n"},
    RailroadCase{"empty", u"railroad-beta\n"},
    RailroadCase{"entities", u"railroad-beta\nchars = sequence(terminal('#amp;'), terminal('#lt;'));\n"},
    RailroadCase{"configured",
        u"---\nconfig:\n  railroad:\n"
        u"    padding: 7\n    verticalSeparation: 13\n    horizontalSeparation: 17\n"
        u"    arcRadius: 9\n    markerRadius: 3\n    strokeWidth: 1.5\n"
        u"    fontSize: 20\n    fontFamily: Arial\n    terminalFill: '#abcdef'\n"
        u"    lineColor: purple\n    useMaxWidth: false\n---\n"
        u"railroad-beta\n"
        u"item = sequence(terminal(''), terminal('é  ñ'), optional(nonterminal('item')));\n"},
};

suite mermaid_railroad = [] {
    "mermaid_railroad_measured_svg_matches_browser_cases"_test = [] {
        for (auto theme : {std::u16string_view(u"default"), std::u16string_view(u"dark")}) {
            for (const auto& test_case : cases) {
                auto session = ConfigSession::create();
                expect(fatal(session.has_value()));
                ConfigValue site{.kind=ConfigKind::mapping};
                site.mapping[u"fontFamily"] = ConfigValue{.kind=ConfigKind::string,.string=u"Arial"};
                site.mapping[u"theme"] = ConfigValue{.kind=ConfigKind::string,.string=std::u16string(theme)};
                site.mapping[u"themeVariables"] = ConfigValue{.kind=ConfigKind::mapping,
                    .mapping={{u"fontFamily", ConfigValue{.kind=ConfigKind::string,.string=u"Arial"}}}};
                expect(fatal(session->save_initialize_config(site).has_value()));
                expect(fatal(session->set_site_config(site).has_value()));

                auto result = render_mermaid(test_case.source, *session);
                expect(result.has_value()) << test_case.name << (result ? "" : result.error());
                if (!result) continue;
                expect(result->width > 0 && result->height > 0) << test_case.name;
                expect(result->svg.find("railroad-diagram") != std::string::npos) << test_case.name;
                if (auto directory = std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                    std::filesystem::create_directories(directory);
                    std::string name(test_case.name);
                    if (theme == u"dark") name += "-dark";
                    std::ofstream output(std::filesystem::path(directory) /
                        ("railroad-" + name + ".svg"), std::ios::binary);
                    output << result->svg;
                    expect(output.good()) << name;
                }
            }
        }
    };
};
}
