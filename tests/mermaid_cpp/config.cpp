#include "support/folia_test.hpp"
import folia.mermaid.config_sanitize;
import folia.mermaid.config_merge;
import folia.mermaid.config_appearance;
import folia.mermaid.directives;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
std::u16string decode(std::string_view text) {
    if (text == "-") return {};
    if (text.size() % 4) throw std::runtime_error("Invalid UTF-16 fixture");
    std::u16string result;
    for (std::size_t i = 0; i < text.size(); i += 4) {
        unsigned c{};
        const auto parsed = std::from_chars(text.data() + i, text.data() + i + 4, c, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + i + 4) throw std::runtime_error("Invalid hex");
        result += static_cast<char16_t>(c);
    }
    return result;
}
std::string hex(std::u16string_view text) {
    if (text.empty()) return "-";
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (const auto c : text) for (int shift = 12; shift >= 0; shift -= 4) result += digits[(c >> shift) & 15];
    return result;
}
std::string snapshot(const ConfigValue& value) {
    switch (value.kind) {
    case ConfigKind::null: return "N";
    case ConfigKind::boolean: return value.boolean ? "T" : "F";
    case ConfigKind::number:
        if (std::isnan(value.number)) return "DNaN;";
        if (std::isinf(value.number)) return value.number < 0 ? "D-Infinity;" : "DInfinity;";
        if (value.number == 0 && std::signbit(value.number)) return "D-0;";
        return "D" + *format_number(value.number) + ";";
    case ConfigKind::string: return "S" + hex(value.string) + ";";
    case ConfigKind::sequence: {
        std::string result = "[";
        for (const auto& item : value.sequence) result += snapshot(item);
        return result + "]";
    }
    case ConfigKind::mapping: {
        std::string result = "{";
        for (const auto& [key, item] : value.mapping) result += "S" + hex(key) + ";" + snapshot(item);
        return result + "}";
    }
    }
    throw std::runtime_error("Invalid config kind");
}
ConfigValue json(std::u16string_view source) {
    // Bare directive words (null/false/numbers) intentionally remain strings.
    // Wrap the fixture's JSON value in an array to enter the strict JSON branch.
    auto parsed = detect_directives(u"%%{init:[" + std::u16string{source} + u"]}%%");
    if (!parsed || parsed->size() != 1 || !parsed->front().type) throw std::runtime_error("Invalid fixture JSON");
    if (parsed->front().args.kind != ConfigKind::sequence || parsed->front().args.sequence.size() != 1)
        throw std::runtime_error("Invalid wrapped fixture JSON");
    return std::move(parsed->front().args.sequence.front());
}
const ConfigValue& field(const ConfigValue& value, std::u16string_view key) {
    const auto found = value.find(key);
    if (!found) throw std::runtime_error("Missing fixture property");
    return *found;
}
suite config = [] {
    "mermaid_config_matches_official_sanitizer_merge_appearance"_test = [] {
        const auto override = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override ? override : FOLIA_MERMAID_FIXTURE_DIR} / "config.tsv"};
        expect(fatal(input.is_open()));
        std::string line;
        std::size_t count{};
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.front() == '#') continue;
            std::vector<std::string_view> fields;
            std::string_view rest{line};
            while (true) {
                const auto tab = rest.find('\t'); fields.push_back(rest.substr(0, tab));
                if (tab == rest.npos) break;
                rest.remove_prefix(tab + 1);
            }
            expect(fatal(fields.size() == 5_u));
            const auto arg = json(decode(fields[2]));
            const auto before = snapshot(arg);
            std::expected<ConfigValue, std::string> result;
            if (fields[1] == "sanitize") result = sanitize_directive_config(arg);
            else if (fields[1] == "secure") {
                std::vector<std::u16string> secure;
                for (const auto& item : field(arg, u"secure").sequence) secure.push_back(item.string);
                result = sanitize_secure_config(field(arg, u"value"), secure);
            } else if (fields[1] == "merge") result = assign_config_with_depth(field(arg, u"dst"), field(arg, u"src"), static_cast<int>(field(arg, u"depth").number));
            else if (fields[1] == "appearance") {
                const auto& diagram = field(arg, u"diagram");
                std::optional<std::u16string_view> scope;
                if (diagram.kind == ConfigKind::string) scope = diagram.string;
                result = resolve_config_appearance(field(arg, u"cfg"), field(arg, u"sum"), field(arg, u"directives").sequence, field(arg, u"site"), scope);
            } else throw std::runtime_error("Unknown config operation");
            expect(snapshot(arg) == before) << fields[0] << "input mutated";
            if (fields[3] == "E" || fields[4] != "-") expect(!result) << fields[0] << fields[1] << "expected explicit rejection";
            else {
                expect(result.has_value()) << fields[0] << fields[1] << (result ? "" : result.error());
                if (result) expect(snapshot(*result) == fields[3]) << fields[0] << fields[1] << snapshot(*result) << fields[3];
            }
            ++count;
        }
        expect(count == 2940_u);
    };
    "mermaid_config_owns_results_and_enforces_resource_limits"_test = [] {
        auto source = json(uR"({"theme":"dark","flowchart":{"curve":"basis"}})");
        auto cleaned = sanitize_directive_config(source);
        expect(fatal(cleaned.has_value()));
        source.mapping.clear();
        expect(bool(cleaned->find(u"theme")->string == u"dark"));
        const auto value = json(uR"({"themeVariables":{"primaryColor":"#123"}})");
        expect(!sanitize_directive_config(value, {.depth = 1}));
        expect(!sanitize_directive_config(value, {.values = 1}));
        expect(!sanitize_directive_config(value, {.string_units = 1}));
        expect(!sanitize_directive_config(value, {.operations = 1}));
        expect(!sanitize_secure_config(value, {u"theme"}, {.operations = 1}));
        expect(!sanitize_secure_config(value, {std::u16string(100, u'a')}, {.string_units = 50}));
        const auto empty = json(u"{}");
        expect(!assign_config_with_depth(empty, value, 2, {.values = 1}));
        expect(!assign_config_with_depth(json(u"[1,2,3]"), json(u"[4,5,6]"), 2, {.operations = 15}));
        expect(!resolve_config_appearance(empty, empty, {value}, empty, u"pie", {.depth = 1}));
        expect(!resolve_config_appearance(empty, empty, {}, empty, u"pie", {.operations = 1}));
        expect(!resolve_config_appearance(empty, empty, {}, empty, u"pie", {.string_units = 1}));
        auto deep = empty;
        for (int i = 0; i < 70; ++i) { auto outer = empty; outer.mapping.emplace(u"flowchart", std::move(deep)); deep = std::move(outer); }
        expect(!sanitize_directive_config(deep, {.depth = 1000}));
        expect(!assign_config_with_depth(empty, deep, 2, {.depth = 1000}));
        ConfigValue malformed;
        malformed.mapping.emplace(u"hidden", deep);
        expect(!sanitize_directive_config(malformed));
        expect(!sanitize_secure_config(malformed, {}));
        expect(!assign_config_with_depth(empty, malformed));
        expect(!resolve_config_appearance(empty, malformed, {}, empty, u"pie"));
    };
    "mermaid_config_array_union_uses_identity_and_same_value_zero"_test = [] {
        auto a = json(u"[0]");
        ConfigValue nan; nan.kind = ConfigKind::number; nan.number = std::numeric_limits<double>::quiet_NaN();
        a.sequence.push_back(nan);
        auto b = json(u"[-0]"); b.sequence.push_back(nan);
        auto merged = assign_config_with_depth(a, b);
        expect(fatal(merged.has_value()));
        expect(merged->sequence.size() == 2_u);
        auto objects = json(u"[{\"theme\":\"dark\"}]");
        merged = assign_config_with_depth(objects, objects);
        expect(fatal(merged.has_value()));
        expect(merged->sequence.size() == 2_u);
        merged->sequence[0].mapping.clear();
        expect(merged->sequence[1].find(u"theme") != nullptr);
    };
};
}
