#include "support/folia_test.hpp"
import folia.mermaid.diagram_registry;
import folia.mermaid.directives;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"
ConfigValue string_value(std::u16string_view text) {
    ConfigValue value; value.kind = ConfigKind::string; value.string = text; return value;
}
ConfigValue bool_value(bool bit) { ConfigValue value; value.kind = ConfigKind::boolean; value.boolean = bit; return value; }
ConfigValue list_value() { ConfigValue value; value.kind = ConfigKind::sequence; return value; }
ConfigValue map_value() { ConfigValue value; value.kind = ConfigKind::mapping; return value; }
ConfigValue ids(const DiagramRegistry& registry) {
    auto value = list_value();
    for (const auto& entry : registry.detectors()) value.sequence.push_back(string_value(entry.id));
    return value;
}
template<class Function> std::size_t fixture(const char* name, Function fn) {
    const auto override = std::getenv("FOLIA_MERMAID_FIXTURES");
    std::ifstream input{std::filesystem::path{override ? override : FOLIA_MERMAID_FIXTURE_DIR} / name};
    expect(fatal(input.is_open()));
    std::string line;
    std::size_t count{};
    while (std::getline(input,line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line.front() == '#') continue;
        std::vector<std::string_view> columns;
        std::string_view rest{line};
        while (true) {
            const auto tab = rest.find('\t'); columns.push_back(rest.substr(0,tab));
            if (tab == rest.npos) break;
            rest.remove_prefix(tab+1);
        }
        fn(columns,++count);
    }
    return count;
}
suite diagram_registry = [] {
    "mermaid_diagram_builtin_order_and_loader_routes_match_original"_test = [] {
        const auto count = fixture("detector-order.tsv",[](const auto& row, auto) {
            expect(fatal(row.size() == 3_u));
            DiagramRegistry registry;
            const bool large = row[0] == "1";
            expect(fatal(registry.add_builtin_diagrams(large).has_value()));
            expect(snapshot(ids(registry)) == snapshot(json(decode(row[1]))));
            auto loaders = list_value();
            for (const auto& entry : registry.detectors()) {
                const auto route = registry.loader_route(entry.id);
                expect(fatal(route.has_value()));
                loaders.sequence.push_back(bool_value(route->has_value()));
                if (*route) expect(bool(**route == entry.id));
            }
            expect(snapshot(loaders) == snapshot(json(decode(row[2]))));
            const auto before = snapshot(ids(registry));
            expect(fatal(registry.add_builtin_diagrams(!large).has_value()));
            expect(snapshot(ids(registry)) == before);
            expect(!registry.loader_route(u"not registered"));
        });
        expect(count == 2_u);
    };
    "mermaid_diagram_builtin_dispatch_matches_original"_test = [] {
        DiagramRegistry registries[2];
        expect(fatal(registries[0].add_builtin_diagrams(false).has_value()));
        expect(fatal(registries[1].add_builtin_diagrams(true).has_value()));
        const auto config = json(uR"({"layout":"dagre","flowchart":{"defaultRenderer":"elk"},"custom":["retained"]})");
        const auto before = snapshot(config);
        const auto count = fixture("detectors.tsv",[&](const auto& row, auto index) {
            expect(fatal(row.size() == 5_u));
            const auto source = decode(row[1]);
            const auto result = registries[row[0] == "1" ? 1 : 0].detect(source,&config);
            expect(snapshot(config) == before) << index << "input config mutation";
            if (row[4] == "-") {
                expect(fatal(result.has_value())) << index;
                expect(hex(result->id) == row[3]) << index;
                expect(snapshot(result->config) == snapshot(json(decode(row[2])))) << index;
            } else {
                expect(fatal(!result)) << index;
                expect(result.error().kind == DiagramDetectionErrorKind::unknown_diagram) << index;
                expect(hex(result.error().message) == row[4]) << index;
            }
        });
        expect(count == 8084_u);
    };
    "mermaid_diagram_custom_registry_matches_original_lifecycle"_test = [] {
        DiagramRegistry registry;
        auto trace = list_value();
        std::size_t warnings{};
        std::string scenario;
        std::function<DiagramDetector(std::u16string,std::u16string)> callback;
        callback = [&](std::u16string id,std::u16string rule) -> DiagramDetector {
            return [&,id=std::move(id),rule=std::move(rule)](std::u16string_view text,ConfigValue& config) -> std::expected<bool,std::u16string> {
                auto item = map_value();
                item.mapping[u"id"] = string_value(id); item.mapping[u"text"] = string_value(text);
                item.mapping[u"layout"] = field(config,u"layout"); trace.sequence.push_back(std::move(item));
                if (rule == u"fail") return std::unexpected(u"detector failed");
                if (rule == u"set") { config.mapping[u"layout"] = string_value(u"custom"); return false; }
                if (rule == u"replace") {
                    const auto result = registry.add_detector(u"next",callback(u"next",u"true"));
                    expect(fatal(result.has_value())); warnings += *result ? 1 : 0; return false;
                }
                return rule == u"true" || (rule == u"layout" && field(config,u"layout").string == u"custom");
            };
        };
        const auto count = fixture("detector-registry.tsv",[&](const auto& row, auto index) {
            expect(fatal(row.size() == 3_u));
            if (scenario != row[0]) {registry = DiagramRegistry{}; warnings = 0; scenario = row[0];}
            trace.sequence.clear();
            auto config = json(uR"({"layout":"dagre"})");
            auto result = map_value();
            const auto step = json(decode(row[1]));
            const auto& op = field(step,u"op").string;
            if (op == u"add") {
                const auto& id = field(step,u"id").string;
                const auto loader = step.find(u"loader");
                const auto added = registry.add_detector(id,callback(id,field(step,u"rule").string),
                    loader ? std::optional<std::u16string>{loader->string} : std::nullopt);
                expect(fatal(added.has_value())); warnings += *added ? 1 : 0;
            } else if (op == u"builtins") {
                const auto added = registry.add_builtin_diagrams();
                expect(fatal(added.has_value())); warnings += *added;
            } else if (op == u"load") {
                const auto loader = registry.loader_route(field(step,u"id").string);
                if (!loader) result.mapping[u"missing"] = bool_value(true);
                else result.mapping[u"loader"] = *loader ? string_value(**loader) : ConfigValue{};
            } else {
                const auto detected = registry.detect(field(step,u"text").string,&config);
                if (detected) {result.mapping[u"id"] = string_value(detected->id); config = detected->config;}
                else result.mapping[u"error"] = string_value(detected.error().message);
            }
            auto output = map_value();
            output.mapping[u"ids"] = ids(registry);
            ConfigValue number; number.kind = ConfigKind::number; number.number = static_cast<double>(warnings);
            output.mapping[u"warnings"] = number; output.mapping[u"trace"] = trace;
            output.mapping[u"result"] = result; output.mapping[u"config"] = config;
            expect(snapshot(output) == snapshot(json(decode(row[2])))) << index;
        });
        expect(count == 39_u);
    };
    "mermaid_diagram_registry_ownership_limits_and_transactional_failures"_test = [] {
        const DiagramDetector yes = [](auto,auto&) -> std::expected<bool,std::u16string> {return true;};
        DiagramRegistry small{{.source_units=4,.detectors=2,.registry_units=8}};
        std::u16string key=u"a", loader=u"load";
        expect(fatal(small.add_detector(key,yes,loader).has_value()));
        key[0]=u'b'; loader[0]=u'x';
        expect(bool(small.detectors()[0].id == u"a"));
        expect(bool(**small.loader_route(u"a") == u"load"));
        auto descriptors = small.detectors(); descriptors[0].id=u"changed";
        expect(bool(small.detectors()[0].id == u"a"));
        expect(!small.add_detector(u"long",yes,u"loader"));
        expect(!small.add_builtin_diagrams());
        expect(small.detectors().size() == 1_u); // builtin batch rolled back
        expect(fatal(small.add_detector(u"b",yes).has_value()));
        expect(!small.add_detector(u"c",yes));
        expect(fatal(small.add_detector(u"a",yes).has_value())); // overwrite at capacity
        expect(!*small.loader_route(u"a"));
        expect(!small.detect(u"12345")); expect(small.detect(u"1234").has_value());
        expect(!small.add_detector(u"a",{}));
        for (auto id : {u"__proto__",u"constructor",u"toString",u"hasOwnProperty"}) expect(!small.add_detector(id,yes));

        DiagramRegistry registry;
        expect(fatal(registry.add_builtin_diagrams().has_value()));
        const auto input=json(uR"({"layout":"dagre","extra":{"array":[1,2],"value":null}})");
        const auto before=snapshot(input);
        auto detected=registry.detect(u"flowchart-elk",&input);
        expect(fatal(detected.has_value()));
        expect(bool(field(detected->config,u"layout").string == u"elk"));
        detected->config.mapping[u"extra"]={}; expect(snapshot(input)==before);
        expect(registry.detect(u"flowchart-elk").has_value());
        for (auto scalar : {ConfigValue{},string_value(u"x"),list_value()}) expect(!registry.detect(u"pie",&scalar));
        auto hidden=map_value(); hidden.string=u"inactive";
        expect(!registry.detect(u"pie",&hidden));
        DiagramRegistry limited{{.config={.depth=1,.values=5,.string_units=10,.operations=100}}};
        expect(fatal(limited.add_detector(u"a",yes).has_value()));
        expect(!limited.detect(u"pie",&input));
        expect(fatal(limited.add_detector(u"a",[](auto,auto& config)->std::expected<bool,std::u16string> {
            config.mapping[u"large"]=string_value(u"01234567890");return true;
        }).has_value()));
        expect(!limited.detect(u"pie"));
        DiagramRegistry rollback;
        expect(fatal(rollback.add_detector(u"fail",[](auto,auto& config)->std::expected<bool,std::u16string> {
            config.mapping[u"layout"]=string_value(u"changed");return std::unexpected(u"failure");
        }).has_value()));
        const auto failed=rollback.detect(u"pie",&input);
        expect(fatal(!failed)); expect(failed.error().kind==DiagramDetectionErrorKind::detector_failure);
        expect(snapshot(input)==before);
        DiagramRegistry isolated; expect(isolated.detectors().empty());
        auto copied=registry; expect(fatal(copied.add_detector(u"pie",yes,u"custom").has_value()));
        expect(bool(**registry.loader_route(u"pie") == u"pie"));
    };
};
}
