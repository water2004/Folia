#include "support/folia_test.hpp"
import folia.mermaid.frontend;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"

suite frontend = [] {
    "mermaid_frontend_matches_original_composed_sequence"_test = [] {
        const auto override = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override ? override : FOLIA_MERMAID_FIXTURE_DIR} / "frontend.tsv"};
        expect(fatal(input.is_open()));
        std::vector<std::string> values;
        std::string line;
        std::size_t count{};
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.starts_with("# value\t")) {
                const auto tab = line.find('\t', 8);
                expect(fatal(tab != std::string::npos));
                expect(fatal(std::stoul(line.substr(8, tab - 8)) == values.size()));
                values.push_back(line.substr(tab + 1)); continue;
            }
            if (line.empty() || line.front() == '#') continue;
            std::vector<std::string_view> fields;
            std::string_view rest{line};
            while (true) {
                const auto tab = rest.find('\t'); fields.push_back(rest.substr(0, tab));
                if (tab == rest.npos) break;
                rest.remove_prefix(tab + 1);
            }
            expect(fatal(fields.size() == 4_u || fields.size() == 11_u));
            auto session = ConfigSession::create();
            expect(fatal(session.has_value()));
            const auto site = json(decode(fields[2]));
            expect(fatal(session->save_initialize_config(site).has_value()));
            expect(fatal(session->set_site_config(site).has_value()));
            const auto before = snapshot(*session->current_config());
            const auto source = decode(fields[1]);
            const auto result = prepare_diagram(source, *session);
            expect(snapshot(*session->current_config()) == before) << fields[0] << "session changed";
            if (fields[3] == "E") {
                expect(!result) << fields[0] << "expected error";
            } else {
                expect(result.has_value()) << fields[0] << (result ? "" : result.error());
                if (result) {
                    expect(hex(result->code.raw) == fields[3]) << fields[0] << "raw";
                    expect(hex(result->code.cleaned) == fields[4]) << fields[0] << "cleaned";
                    expect(hex(result->code.with_comments) == fields[5]) << fields[0] << "comments";
                    expect(result->code.frontmatter_line_offset == std::stoul(std::string{fields[6]})) << fields[0];
                    expect((result->title ? hex(*result->title) : "U") == fields[7]) << fields[0] << "title";
                    expect(snapshot(result->config) == values.at(std::stoul(std::string{fields[8]}))) << fields[0] << "config";
                    expect(hex(result->type) == fields[9]) << fields[0] << "type";
                    expect(snapshot(result->effective_config) == values.at(std::stoul(std::string{fields[10]})))
                        << fields[0] << "effective config";
                }
            }
            ++count;
        }
        expect(count == 49_u);
    };
    "mermaid_frontend_isolates_requests_and_preserves_source"_test = [] {
        auto session = ConfigSession::create();
        expect(fatal(session.has_value()));
        const auto site = json(uR"({"theme":"default"})");
        expect(fatal(session->save_initialize_config(site).has_value()));
        expect(fatal(session->set_site_config(site).has_value()));
        auto source = std::u16string{u"%%{init:{\"theme\":\"dark\"}}%%\npie"};
        auto first = prepare_diagram(source, *session);
        expect(fatal(first.has_value()));
        source.clear();
        expect(!first->code.raw.empty());
        expect(bool(field(first->effective_config,u"theme").string == u"dark"));
        first->effective_config.mapping.clear();
        expect(!prepare_diagram(u"---\nconfig: [\n---\npie", *session));
        auto second = prepare_diagram(u"packet\n0-7: \"Data\"", *session);
        expect(fatal(second.has_value()));
        expect(bool(field(second->effective_config,u"theme").string == u"default"));
        expect(bool(second->type == u"packet"));
        FrontendLimits limits; limits.source_units = 2;
        expect(!prepare_diagram(u"pie", *session, limits));
        expect(prepare_diagram(u"pie", *session).has_value());
    };
};
}
