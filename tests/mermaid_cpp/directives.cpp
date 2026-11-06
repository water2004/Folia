#include "support/folia_test.hpp"
import folia.mermaid.directives;
import folia.mermaid.source_text;
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
std::string snapshot(const std::vector<Directive>& directives) {
    std::string result;
    for (const auto& directive : directives) {
        if (!result.empty()) result += '|';
        result += directive.type ? "S" + hex(*directive.type) + ";" : "U";
        result += snapshot(directive.args);
    }
    return result;
}
suite directives = [] {
    "mermaid_directives_match_official_isolated_detection"_test = [] {
        const auto override = std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override ? override : FOLIA_MERMAID_FIXTURE_DIR} / "directives.tsv"};
        expect(fatal(input.is_open()));
        std::size_t count{};
        std::string line;
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
            expect(fatal(fields.size() == 4_u));
            const auto filter = fields[1] == "all" ? DirectiveFilter::all :
                                fields[1] == "init" ? DirectiveFilter::init : DirectiveFilter::wrap;
            const auto source = decode(fields[2]);
            const auto result = detect_directives(source, filter);
            expect(result.has_value()) << fields[0] << fields[1] << (result ? "" : result.error());
            if (result) expect(snapshot(*result) == fields[3]) << fields[0] << fields[1];
            ++count;
        }
        expect(count == 3279_u);
    };
    "mermaid_directive_budgets_are_explicit_and_shared_across_matches"_test = [] {
        expect(!detect_directives(u"%%{wrap}%%", DirectiveFilter::all, {.source_units = 3}));
        expect(!detect_directives(u"%%{init:[[[0]]]}%%", DirectiveFilter::all, {.depth = 2}));
        expect(detect_directives(u"%%{init:[]}%%", DirectiveFilter::all, {.depth = 0}).has_value());
        std::u16string deep = u"%%{init:";
        deep += std::u16string(1000, u'['); deep += u'0'; deep += std::u16string(1000, u']'); deep += u"}%%";
        expect(!detect_directives(deep, DirectiveFilter::all, {.depth = 2000}));
        expect(!detect_directives(u"%%{init:[1,2]}%%", DirectiveFilter::all, {.values = 2}));
        expect(!detect_directives(u"%%{init:{'a':1,'a':2}}%%", DirectiveFilter::all, {.values = 2}));
        expect(!detect_directives(u"%%{wrap}%% %%{wrap}%%", DirectiveFilter::all, {.values = 1}));
        expect(!detect_directives(u"%%{init:'abc'}%%", DirectiveFilter::all, {.string_units = 6}));
        expect(detect_directives(u"%%{init:'abc'}%%", DirectiveFilter::all, {.string_units = 7}).has_value());
        expect(!detect_directives(u"%%{init:{'abc':0}}%%", DirectiveFilter::all, {.string_units = 6}));
        expect(!detect_directives(u"%%{init:'\\u0041'}%%", DirectiveFilter::all, {.string_units = 4}));
        expect(!detect_directives(u"%%{wrap}%% %%{wrap}%%", DirectiveFilter::init, {.directives = 1}));
        expect(!detect_directives(u"pie", DirectiveFilter::all, {.string_units = 2}));
        expect(!detect_directives(u"", DirectiveFilter::all, {.values = 0}));
    };
    "mermaid_directives_own_data_keep_source_and_recover_after_errors"_test = [] {
        auto source = std::u16string{u"%%{init: {'x':[1,2], 'raw':'\\uD800'}}%%\r\npie"};
        const auto original = source;
        auto result = detect_directives(source);
        expect(fatal(result.has_value()));
        expect(bool(source == original));
        source.assign(1000, u'x');
        expect(bool(result->at(0).type == u"init"));
        expect(result->at(0).args.find(u"x")->sequence[1].number == 2.0);
        expect(bool(result->at(0).args.find(u"raw")->string == std::u16string(1, char16_t{0xd800})));
        const auto failed = detect_directives(u"%%{wrap}%% %%{init:{bad}}%% %%{wrap}%%");
        expect(fatal(failed.has_value()));
        expect(failed->size() == 1_u);
        expect(!failed->at(0).type);
        const auto recovered = detect_directives(u"%%{wrap}%%");
        expect(fatal(recovered.has_value()));
        expect(bool(recovered->at(0).type == u"wrap"));
        expect(bool(remove_directives(u"%%{wrap}%%pie") == u"pie"));
    };
    "mermaid_directives_scan_adversarial_sources_with_bounded_results"_test = [] {
        std::u16string text;
        for (int i = 0; i < 10000; ++i) text += u"%%}%%";
        auto result = detect_directives(text);
        expect(fatal(result.has_value()));
        expect(bool(result->at(0).type == text));
        text.clear();
        for (int i = 0; i < 10000; ++i) text += u"%%{ ";
        result = detect_directives(text);
        expect(fatal(result.has_value()));
        expect(result->size() == 1_u);
        text.clear();
        for (int i = 0; i < 10001; ++i) text += u"%%{wrap}%%";
        expect(!detect_directives(text));
        text = u"%%{init:[-0e"; text += std::u16string(10000, u'9'); text += u"]}%%";
        result = detect_directives(text);
        expect(fatal(result.has_value()));
        expect(fatal(result->at(0).args.kind == ConfigKind::sequence));
        const auto zero = result->at(0).args.sequence[0].number;
        expect(zero == 0.0 && std::signbit(zero));
    };
};
}
