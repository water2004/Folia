#include "support/folia_test.hpp"
import folia.mermaid.frontmatter;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

std::u16string decode(std::string_view text) {
    if (text == "-") return {};
    if (text.size() % 4) throw std::runtime_error("Invalid fixture UTF-16");
    std::u16string result;
    for (std::size_t i = 0; i < text.size(); i += 4) {
        unsigned unit{};
        auto parsed = std::from_chars(text.data() + i, text.data() + i + 4, unit, 16);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + i + 4) throw std::runtime_error("Invalid fixture hex");
        result += static_cast<char16_t>(unit);
    }
    return result;
}
std::string hex(std::u16string_view text) {
    if (text.empty()) return "-";
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (auto c : text) for (int shift = 12; shift >= 0; shift -= 4) result += digits[(c >> shift) & 15];
    return result;
}
std::string snapshot(const ConfigValue& value) {
    switch (value.kind) {
    case ConfigKind::null: return "N";
    case ConfigKind::boolean: return value.boolean ? "T" : "F";
    case ConfigKind::string: return "S" + hex(value.string) + ";";
    case ConfigKind::number:
        if (std::isnan(value.number)) return "DNaN;";
        if (std::isinf(value.number)) return value.number < 0 ? "D-Infinity;" : "DInfinity;";
        if (value.number == 0 && std::signbit(value.number)) return "D-0;";
        return "D" + *format_number(value.number) + ";";
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
    throw std::runtime_error("Invalid value kind");
}
std::string snapshot(const FrontMatterMetadata& metadata) {
    ConfigValue object; object.kind = ConfigKind::mapping;
    if (metadata.config) object.mapping.emplace(u"config", *metadata.config);
    for (auto [key, text] : {std::pair{u"title", &metadata.title}, std::pair{u"displayMode", &metadata.display_mode}}) {
        if (*text) {
            ConfigValue value; value.kind = ConfigKind::string; value.string = **text;
            object.mapping.emplace(key, std::move(value));
        }
    }
    return snapshot(object);
}

suite frontmatter = [] {
    "mermaid_frontmatter_matches_original_extractor_and_yaml_schema"_test = [] {
        const auto override = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override ? override : FOLIA_MERMAID_FIXTURE_DIR} / "frontmatter.tsv"};
        expect(fatal(input.is_open()));
        std::size_t count{};
        std::string line;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.front() == '#') continue;
            std::vector<std::string_view> fields;
            std::string_view rest{line};
            while (true) {
                auto tab = rest.find('\t'); fields.push_back(rest.substr(0, tab));
                if (tab == rest.npos) break;
                rest.remove_prefix(tab + 1);
            }
            expect(fatal(fields.size() == 5_u));
            const auto result = extract_front_matter(decode(fields[1]));
            const bool expected_ok = fields[2] == "ok";
            expect(result.has_value() == expected_ok) << fields[0] << (result ? "unexpected success" : result.error());
            if (result && expected_ok) {
                expect(bool(result->text == decode(fields[3]))) << fields[0] << "remaining source";
                expect(snapshot(result->metadata) == fields[4]) << fields[0];
            }
            ++count;
        }
        expect(count == 515_u);
    };
    "mermaid_yaml_resource_limits_cycles_and_unicode_errors"_test = [] {
        expect(!parse_config_yaml(u"&self [*self]"));
        expect(!parse_config_yaml(u"a: &a [1, 2]\nb: [*a, *a, *a]", {.values = 10}));
        expect(!parse_config_yaml(u"key: abc", {.source_units = 3}));
        expect(!parse_config_yaml(u"key: abc", {.expanded_units = 3}));
        expect(!parse_config_yaml(u"[[[0]]]", {.depth = 2}));
        std::u16string deep(1000, u'['); deep += u'0'; deep += std::u16string(1000, u']');
        expect(!parse_config_yaml(deep));
        expect(!parse_config_yaml(std::u16string{u't', u':', u' ', char16_t{0xd800}}));
        expect(!parse_config_yaml(u"a: 1\n---\nb: 2"));
        auto huge = parse_config_yaml(std::u16string(100000, u'9'));
        expect(fatal(huge.has_value()));
        expect(huge->kind == ConfigKind::string);
        expect(parse_config_yaml(u"[]", {.depth = 0}).has_value());
        auto alias = parse_config_yaml(u"base: &a [1]\ncopy: *a");
        expect(fatal(alias.has_value()));
        alias->mapping.at(u"base").sequence[0].number = 9;
        expect(alias->mapping.at(u"copy").sequence[0].number == 1.0);
    };
    "mermaid_frontmatter_owns_values_and_preserves_diagram_source"_test = [] {
        auto result = extract_front_matter(std::u16string{u"---\ntitle: own\nconfig: {x: [true]}\n---\npie\r\n%% comment"});
        expect(fatal(result.has_value()));
        expect(bool(result->text == u"pie\r\n%% comment"));
        expect(bool(result->metadata.title == u"own"));
        expect(fatal(result->metadata.config.has_value()));
        expect(result->metadata.config->find(u"x")->sequence[0].boolean);
        const std::u16string ordinary{u'p', char16_t{0xd800}};
        auto unchanged = extract_front_matter(ordinary);
        expect(fatal(unchanged.has_value()));
        expect(bool(unchanged->text == ordinary));
    };
};
}
