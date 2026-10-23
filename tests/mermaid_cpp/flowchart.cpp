#include "support/folia_test.hpp"
import folia.mermaid.flowchart;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"
void same(std::u16string_view actual, const ConfigValue& expected, std::u16string_view key) {
    expect(bool(actual == field(expected,key).string)) << hex(key) << hex(actual) << hex(field(expected,key).string);
}
void same(const std::vector<std::u16string>& actual, const ConfigValue& expected, std::u16string_view key) {
    const auto& values = field(expected,key).sequence;
    expect(fatal(actual.size() == values.size())) << hex(key);
    for (std::size_t i = 0; i < actual.size(); ++i) expect(bool(actual[i] == values[i].string)) << hex(key);
}
suite flowchart = [] {
    "mermaid_flowchart_matches_original_parser_and_layout_input"_test = [] {
        const auto override = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override ? override : FOLIA_MERMAID_FIXTURE_DIR}/"flowchart.tsv"};
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
            auto actual = prepare_flowchart(decode(fields[1]),*session);
            if (expected.kind == ConfigKind::null) { expect(!actual) << fields[0]; continue; }
            expect(fatal(actual.has_value())) << fields[0] << (actual ? "" : actual.error());
            same(actual->direction,expected,u"direction"); same(actual->title,expected,u"title");
            same(actual->acc_title,expected,u"accTitle"); same(actual->acc_description,expected,u"accDescription");
            expect(actual->node_spacing == field(expected,u"nodeSpacing").number);
            expect(actual->rank_spacing == field(expected,u"rankSpacing").number);
            const auto& nodes = field(expected,u"nodes").sequence;
            expect(fatal(actual->nodes.size() == nodes.size())) << fields[0];
            for (std::size_t i = 0; i < nodes.size(); ++i) {
                const auto& n = actual->nodes[i]; const auto& e = nodes[i];
                same(n.id,e,u"id"); same(n.label,e,u"label"); same(n.label_type,e,u"labelType"); same(n.shape,e,u"shape");
                same(n.parent,e,u"parent"); same(n.direction,e,u"dir"); same(n.classes,e,u"classes");
                expect(n.group == field(e,u"group").boolean); expect(n.padding == field(e,u"padding").number);
                same(n.styles,e,u"styles"); same(n.compiled_styles,e,u"compiled");
            }
            const auto& edges = field(expected,u"edges").sequence;
            expect(fatal(actual->edges.size() == edges.size())) << fields[0];
            for (std::size_t i = 0; i < edges.size(); ++i) {
                const auto& n = actual->edges[i]; const auto& e = edges[i];
                same(n.id,e,u"id"); same(n.start,e,u"start"); same(n.end,e,u"end"); same(n.label,e,u"label");
                same(n.label_type,e,u"labelType"); same(n.type,e,u"type"); same(n.stroke,e,u"stroke");
                same(n.arrow_start,e,u"arrowStart"); same(n.arrow_end,e,u"arrowEnd"); same(n.curve,e,u"curve");
                expect(static_cast<double>(n.min_length) == field(e,u"minlen").number);
                same(n.styles,e,u"styles"); same(n.compiled_styles,e,u"compiled");
            }
        }
        expect(count == 24_u);
    };
    "mermaid_flowchart_request_isolation_limits_and_explicit_stage_errors"_test = [] {
        auto session = ConfigSession::create(); expect(fatal(session.has_value()));
        const auto source = u"graph TD\nA-->B";
        auto before = prepare_flowchart(source,*session); expect(fatal(before.has_value()));
        FlowchartLimits limits; limits.edges = 0;
        expect(!prepare_flowchart(source,*session,limits));
        limits = {}; limits.nodes = 1;
        expect(!prepare_flowchart(source,*session,limits));
        limits = {}; limits.subgraph_depth = 0;
        expect(!prepare_flowchart(u"graph TD\nsubgraph g\nA\nend",*session,limits));
        for (const auto input : {u"graph TD\nA@{shape:rect}",u"graph TD\nA[\"<b>HTML</b>\"]",
            u"graph TD\nA[\"`markdown`\"]",u"graph TD\nA-->B\nclick A callback",u"pie\n\"A\":1"})
            expect(!prepare_flowchart(input,*session));
        auto after = prepare_flowchart(source,*session); expect(fatal(after.has_value()));
        expect(snapshot(before->effective_config) == snapshot(after->effective_config));
        expect(bool(after->nodes[0].label == u"A"));
        before->nodes[0].label = u"Owned";
        expect(bool(after->nodes[0].label == u"A"));
    };
};
}
