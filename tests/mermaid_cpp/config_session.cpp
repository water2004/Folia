#include "support/folia_test.hpp"
import folia.mermaid.config_session;
import folia.mermaid.directives;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"

std::string delta(const ConfigValue* value, const ConfigValue* base) {
    if (!value || !base) {
        if (value == base) return {};
        return "=" + (value ? snapshot(*value) : "U");
    }
    if (snapshot(*value) == snapshot(*base)) return {};
    if (value->kind == ConfigKind::mapping && base->kind == ConfigKind::mapping) {
        std::set<std::u16string> keys;
        for (const auto& [key, item] : value->mapping) keys.insert(key);
        for (const auto& [key, item] : base->mapping) keys.insert(key);
        std::string result = "{";
        for (const auto& key : keys) {
            const auto difference = delta(value->find(key), base->find(key));
            if (!difference.empty()) result += "S" + hex(key) + ";" + difference;
        }
        return result + "}";
    }
    return "=" + snapshot(*value);
}
std::string state(const ConfigSession& session, const ConfigValue& baseline) {
    const auto site = session.site_config(), current = session.current_config();
    const auto user = session.user_config();
    if (!site) throw std::runtime_error(site.error());
    if (!current) throw std::runtime_error(current.error());
    if (!user) throw std::runtime_error(user.error());
    return delta(&*site, &baseline) + "|" + delta(&*current, &baseline) + "|" + snapshot(*user) +
        "|" + (session.deprecated_diagrams_warning() ? "1" : "0");
}
std::expected<void, std::string> run(ConfigSession& session, const ConfigValue& step) {
    const auto& name = field(step, u"op").string;
    const auto arg = step.find(u"arg");
    if (name == u"site") return session.set_site_config(*arg);
    if (name == u"update") return session.update_site_config(*arg);
    if (name == u"save") return session.save_initialize_config(*arg);
    if (name == u"set") return session.set_config(*arg);
    if (name == u"add") return session.add_directive(*arg);
    if (name == u"reset") return session.reset(arg);
    if (name == u"scope") return session.set_diagram_scope(arg ? std::optional<std::u16string_view>{arg->string} : std::nullopt);
    if (name == u"get") return {};
    throw std::runtime_error("Unknown session command");
}
suite config_session = [] {
    "mermaid_config_session_matches_original_full_state_lifecycle"_test = [] {
        const auto override = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override ? override : FOLIA_MERMAID_FIXTURE_DIR} / "session.tsv"};
        expect(fatal(input.is_open()));
        const auto defaults = default_session_config();
        std::optional<ConfigSession> session;
        std::string line, scenario;
        std::vector<std::string> values;
        std::size_t count{}, baselines{};
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.starts_with("# defaults\t")) {
                expect(snapshot(defaults) == line.substr(11)); ++baselines; continue;
            }
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
            expect(fatal(fields.size() == 5_u));
            if (!session || scenario != fields[0]) {
                auto created = ConfigSession::create();
                expect(fatal(created.has_value()));
                session = std::move(*created); scenario = fields[0];
            }
            const auto step = json(decode(fields[2]));
            const auto source = snapshot(step), before = state(*session, defaults);
            const auto result = run(*session, step);
            expect(snapshot(step) == source) << scenario << fields[1] << "input mutation";
            if (fields[3] == "E" || fields[4] != "-") {
                expect(!result) << scenario << fields[1] << "expected rejection";
                expect(state(*session, defaults) == before) << "failed command changed state";
            } else {
                expect(result.has_value()) << scenario << fields[1] << (result ? "" : result.error());
                if (result) {
                    std::string expected;
                    auto references = fields[3];
                    for (int i = 0; i < 3; ++i) {
                        const auto split = references.find('|');
                        expect(fatal(split != references.npos));
                        expected += values.at(std::stoul(std::string{references.substr(0, split)})) + "|";
                        references.remove_prefix(split + 1);
                    }
                    expected += references;
                    const auto actual = state(*session, defaults);
                    const auto mismatch = std::mismatch(actual.begin(), actual.end(), expected.begin(), expected.end());
                    const auto offset = static_cast<std::size_t>(mismatch.first - actual.begin());
                    expect(actual == expected) << scenario << fields[1] << "at" << offset
                        << actual.substr(offset, 100) << expected.substr(offset, 100);
                }
            }
            ++count;
        }
        expect(count == 640_u); expect(baselines == 1_u);
    };
    "mermaid_config_session_owned_results_and_isolated_instances"_test = [] {
        auto a = ConfigSession::create(), b = ConfigSession::create();
        expect(fatal(a.has_value() && b.has_value()));
        const auto original = b->current_config();
        expect(fatal(original.has_value()));
        auto conf = json(uR"({"theme":"dark","themeVariables":{"primaryColor":"#123"}})");
        expect(a->save_initialize_config(conf).has_value());
        expect(a->set_site_config(conf).has_value());
        conf.mapping.clear();
        auto returned = a->current_config(); expect(fatal(returned.has_value())); returned->mapping.clear();
        auto user = a->user_config(); expect(fatal(user.has_value())); user->mapping.clear();
        expect(a->current_config()->find(u"theme")->string == std::u16string{u"dark"});
        expect(snapshot(*b->current_config()) == snapshot(*original));
        auto c = *a;
        expect(a->set_config(json(uR"({"theme":"default"})")).has_value());
        expect(c.current_config()->find(u"theme")->string == std::u16string{u"dark"});
        expect(c.set_diagram_scope(u"flowchart-v2").has_value());
        expect(c.reset().has_value());
        expect(snapshot(*c.user_config()) != snapshot(json(u"{}")));
    };
    "mermaid_config_session_transactional_rejection_and_budgets"_test = [] {
        const auto defaults = default_session_config();
        auto session = ConfigSession::create({.directives=1});
        expect(fatal(session.has_value()));
        expect(session->add_directive(json(uR"({"fontFamily":"Arial"})")).has_value());
        const auto before = state(*session, defaults);
        expect(!session->add_directive(json(uR"({"theme":"dark"})")));
        expect(state(*session, defaults) == before);
        expect(!session->set_site_config(json(uR"({"theme":"forest"})")));
        expect(state(*session, defaults) == before);
        expect(!session->set_config(json(uR"({"theme":"dark","themeVariables":{"primaryColor":"bad-color"}})")));
        expect(state(*session, defaults) == before);
        expect(!session->set_site_config(json(uR"({"secure":[1]})")));
        expect(state(*session, defaults) == before);
        ConfigValue malformed; malformed.string = u"hidden";
        expect(!session->save_initialize_config(malformed));
        expect(!session->set_diagram_scope(std::u16string(4 * 1024 * 1024 + 1, u'x')));
        expect(state(*session, defaults) == before);
        expect(session->reset().has_value());
        expect(session->add_directive(json(uR"({"theme":"dark"})")).has_value());
        expect(!ConfigSession::create({.config={.values=1}}));
        expect(!ConfigSession::create({.config={.depth=1}}));
        expect(!ConfigSession::create({.config={.string_units=1}}));
        expect(!ConfigSession::create({.config={.operations=1}}));
        auto limited = ConfigSession::create({.config={.values=5000}});
        expect(fatal(limited.has_value()));
        auto large = json(u"{}");
        ConfigValue sequence; sequence.kind=ConfigKind::sequence;
        sequence.sequence.resize(5001);
        large.mapping.emplace(u"custom", std::move(sequence));
        const auto limited_before = state(*limited, defaults);
        expect(!limited->update_site_config(large));
        expect(state(*limited, defaults) == limited_before);
    };
};
}
