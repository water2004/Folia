#include "support/folia_test.hpp"
#include "math/support/fixture_values.hpp"
import folia.math.dependencies.mhchem_parser;

namespace {
using namespace boost::ut;
using namespace folia::math::mhchem;

enum class MhchemSourceMode { parse, pattern, tables, regexes };
struct MhchemSourceCase {
    MhchemSourceMode mode;
    std::u16string name, input;
    std::string expected;
};

std::vector<MhchemSourceCase> sourceCases() {
    std::vector<MhchemSourceCase> result;
    for (auto& row : math_test::fixtures::rows("math/fixtures/mhchem_parser.tsv", 4, 7960)) {
        MhchemSourceMode mode;
        if (row[0] == "parse") mode = MhchemSourceMode::parse;
        else if (row[0] == "pattern") mode = MhchemSourceMode::pattern;
        else if (row[0] == "tables") mode = MhchemSourceMode::tables;
        else if (row[0] == "regexes") mode = MhchemSourceMode::regexes;
        else throw std::runtime_error("Unknown mhchem reference mode: " + row[0]);
        result.push_back({mode, folia::test::data::decode_utf16(row[1]),
                          folia::test::data::decode_utf16(row[2]), std::move(row[3])});
    }
    return result;
}

std::string quote(std::u16string_view value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result = "\"";
    for (const auto unit : value) {
        result += "\\u";
        for (int shift = 12; shift >= 0; shift -= 4)
            result += digits[(unit >> shift) & 15];
    }
    return result + '"';
}

std::string undefinedValue() {
    return "{\"$\":" + quote(u"undefined") + '}';
}

std::string output(const Output&);
std::string node(const Parsed& value) {
    if (value.raw) return quote(value.text);
    std::string result = "{\"type_\":" + quote(value.type);
    const auto scalar = [&](std::string_view field, const std::u16string& text) {
        result += ",\"" + std::string(field) + "\":" + quote(text);
    };
    const auto children = [&](std::string_view field, const Output& nodes) {
        result += ",\"" + std::string(field) + "\":" + output(nodes);
    };
    if (value.type == u"chemfive") {
        children("a", value.a); children("b", value.b); children("p", value.p);
        children("o", value.o); children("q", value.q); children("d", value.d);
        result += ",\"dType\":" + (value.dType.empty() ? undefinedValue() : quote(value.dType));
    } else if (value.type == u"arrow") {
        scalar("r", value.r); children("rd", value.rd); children("rq", value.rq);
    } else if (value.type == u"bond" || value.type == u"operator") scalar("kind_", value.kind);
    else if (value.type == u"color") { scalar("color1", value.color1); children("color2", value.color2); }
    else if (value.type == u"color0") scalar("color", value.color);
    else if (value.type == u"state of aggregation" || value.type == u"state of aggregation subscript")
        children("p1", value.p1Nodes);
    else if (value.type == u"pu-frac" || value.type == u"frac-ce" || value.type == u"overset" ||
             value.type == u"underset" || value.type == u"underbrace") {
        children("p1", value.p1Nodes); children("p2", value.p2Nodes);
    } else if (value.type == u"frac") {
        scalar("p1", value.p1); scalar("p2", value.p2);
    } else if (value.type == u"rm" || value.type == u"text" || value.type == u"roman numeral" ||
               value.type == u"tex-math" || value.type == u"1st-level escape" ||
               value.type == u"comma enumeration L" || value.type == u"comma enumeration M" ||
               value.type == u"comma enumeration S") scalar("p1", value.p1);
    return result + '}';
}

std::string output(const Output& values) {
    std::string result = "[";
    for (const auto& value : values) {
        if (result.size() > 1) result += ',';
        result += node(value);
    }
    return result + ']';
}

std::string option(const Option& value) {
    if (const auto* text = std::get_if<std::u16string>(&value)) return quote(*text);
    if (const auto* number = std::get_if<double>(&value)) {
        char buffer[64];
        const auto converted = std::to_chars(std::begin(buffer), std::end(buffer), *number);
        return {buffer, converted.ptr};
    }
    if (const auto* boolean = std::get_if<bool>(&value)) return *boolean ? "true" : "false";
    return undefinedValue();
}

std::string tables() {
    std::string result = "{";
    for (const auto& machine : machines()) {
        if (result.size() > 1) result += ',';
        result += quote(machine.name) + ":{";
        bool firstState = true;
        for (const auto& state : machine.states) {
            if (!firstState) result += ',';
            firstState = false;
            result += quote(state.name) + ":[";
            bool firstRow = true;
            for (const auto& row : state.transitions) {
                if (!firstRow) result += ',';
                firstRow = false;
                result += "{\"pattern\":" + quote(row.pattern) + ",\"actions\":[";
                for (std::size_t i = 0; i < row.actions.size(); ++i) {
                    if (i) result += ',';
                    result += "{\"type\":" + quote(row.actions[i].type) +
                        ",\"option\":" + option(row.actions[i].option) + '}';
                }
                result += "],\"nextState\":" + quote(row.nextState) +
                    ",\"revisit\":" + (row.revisit ? "true" : "false") +
                    ",\"toContinue\":" + (row.toContinue ? "true" : "false") + '}';
            }
            result += ']';
        }
        result += '}';
    }
    return result + '}';
}

std::string regexes() {
    std::string result = "[";
    for (const auto& pattern : regexPatterns()) {
        if (result.size() > 1) result += ',';
        result += "{\"name\":" + quote(pattern.name) + ",\"source\":" + quote(pattern.source) +
            ",\"flags\":" + quote(pattern.flags) + '}';
    }
    return result + ']';
}

std::string observation(const MhchemSourceCase& row) {
    try {
        std::string result;
        if (row.mode == MhchemSourceMode::tables) result = tables();
        else if (row.mode == MhchemSourceMode::regexes) result = regexes();
        else if (row.mode == MhchemSourceMode::pattern) {
            const auto found = match(row.name, row.input);
            if (!found) result = "null";
            else {
                const auto matched = [&] {
                    if (!found->array) return quote(found->text);
                    std::string values = "[";
                    for (std::size_t i = 0; i < found->captures.size(); ++i) {
                        if (i) values += ',';
                        values += found->captures[i] ? quote(*found->captures[i]) : undefinedValue();
                    }
                    return values + ']';
                }();
                result = "{\"match_\":" + matched + ",\"remainder\":" + quote(found->remainder) + '}';
            }
        } else {
            auto parsed = parse(std::u16string(row.input), row.name);
            const auto before = output(parsed);
            const auto first = texify(parsed, row.name != u"tex");
            const auto after = output(parsed);
            const auto second = texify(parsed, row.name != u"tex");
            result = "{\"parsed\":" + before + ",\"tex\":" + quote(first) + ",\"after\":" + after +
                ",\"repeat\":" + quote(second) + ",\"afterRepeat\":" + output(parsed) +
                ",\"toTex\":" + quote(toTex(std::u16string(row.input), row.name)) + '}';
        }
        return "{\"result\":" + result + ",\"error\":null}";
    } catch (const Error& error) {
        return "{\"result\":null,\"error\":[" + quote(error.id) + ',' + quote(error.message) + "]}";
    }
}

std::size_t firstMismatch(std::string_view actual, std::string_view expected) {
    const auto limit = std::min(actual.size(), expected.size());
    for (std::size_t i = 0; i < limit; ++i)
        if (actual[i] != expected[i]) return i;
    return actual.size() == expected.size() ? std::string_view::npos : limit;
}

std::string nearby(std::string_view value, std::size_t mismatch) {
    const auto begin = mismatch > 80 ? mismatch - 80 : 0;
    const auto end = std::min(value.size(), mismatch + 80);
    return std::string(value.substr(begin, end - begin));
}

std::string utf8(std::u16string_view value, std::size_t maxUnits = 80) {
    std::string result;
    const auto limit = std::min(value.size(), maxUnits);
    const auto append = [&](std::uint32_t cp) {
        if (cp <= 0x7f) result += static_cast<char>(cp);
        else if (cp <= 0x7ff) {
            result += static_cast<char>(0xc0 | (cp >> 6));
            result += static_cast<char>(0x80 | (cp & 0x3f));
        } else if (cp <= 0xffff) {
            result += static_cast<char>(0xe0 | (cp >> 12));
            result += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
            result += static_cast<char>(0x80 | (cp & 0x3f));
        } else {
            result += static_cast<char>(0xf0 | (cp >> 18));
            result += static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
            result += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
            result += static_cast<char>(0x80 | (cp & 0x3f));
        }
    };
    for (std::size_t i = 0; i < limit; ++i) {
        const auto first = static_cast<std::uint16_t>(value[i]);
        if (first >= 0xd800 && first <= 0xdbff && i + 1 < limit) {
            const auto second = static_cast<std::uint16_t>(value[i + 1]);
            if (second >= 0xdc00 && second <= 0xdfff) {
                append(0x10000 + ((first - 0xd800) << 10) + (second - 0xdc00));
                ++i;
                continue;
            }
        }
        if (first >= 0xd800 && first <= 0xdfff) append(0xfffd);
        else append(first);
    }
    if (value.size() > limit) result += "…";
    return result;
}

std::string_view modeName(MhchemSourceMode mode) {
    switch (mode) {
    case MhchemSourceMode::parse: return "parse";
    case MhchemSourceMode::pattern: return "pattern";
    case MhchemSourceMode::tables: return "tables";
    case MhchemSourceMode::regexes: return "regexes";
    }
    return "unknown";
}
} // namespace

suite mhchem_parser_source = [] {
    "mhchem parser, patterns, FSMs, and regexes match every pinned Source record"_test = [] {
        const auto mhchemSourceCases = sourceCases();
        expect(std::size(mhchemSourceCases) == 7960);
        std::size_t parseCount = 0, patternCount = 0, tableCount = 0, regexCount = 0;
        std::size_t failures = 0;
        std::size_t parseFailures = 0, patternFailures = 0, tableFailures = 0, regexFailures = 0;
        for (std::size_t index = 0; index < std::size(mhchemSourceCases); ++index) {
            const auto& row = mhchemSourceCases[index];
            if (row.mode == MhchemSourceMode::parse) ++parseCount;
            else if (row.mode == MhchemSourceMode::pattern) ++patternCount;
            else if (row.mode == MhchemSourceMode::tables) ++tableCount;
            else if (row.mode == MhchemSourceMode::regexes) ++regexCount;
            const auto actual = observation(row);
            const auto& expected = row.expected;
            const auto mismatch = firstMismatch(actual, expected);
            if (mismatch != std::string_view::npos) {
                if (failures < 8) {
                    std::cerr << "mhchem Source mismatch row=" << index << " mode=" << modeName(row.mode)
                        << " name=\"" << utf8(row.name) << "\" input_length_utf16=" << row.input.size()
                        << " input_prefix_utf8=\"" << utf8(row.input) << "\""
                        << " first_mismatch_byte=" << mismatch << '\n'
                        << "  actual[-80,+80]=\"" << nearby(actual, mismatch) << "\"\n"
                        << "  expected[-80,+80]=\"" << nearby(expected, mismatch) << "\"\n";
                }
                ++failures;
                if (row.mode == MhchemSourceMode::parse) ++parseFailures;
                else if (row.mode == MhchemSourceMode::pattern) ++patternFailures;
                else if (row.mode == MhchemSourceMode::tables) ++tableFailures;
                else if (row.mode == MhchemSourceMode::regexes) ++regexFailures;
            }
        }
        if (failures)
            std::cerr << "mhchem Source mismatch summary: total=" << failures << " detailed_rows="
                << std::min<std::size_t>(failures, 8) << " checked{parse=" << parseCount << " pattern=" << patternCount
                << " tables=" << tableCount << " regexes=" << regexCount << "} failureCounts{parse=" << parseFailures
                << " pattern=" << patternFailures << " tables=" << tableFailures << " regexes=" << regexFailures << "}\n";
        expect(failures == 0);
        expect(parseCount == 1679);
        expect(patternCount == 6279);
        expect(tableCount == 1);
        expect(regexCount == 1);
        expect(machines().size() == 14);
        expect(regexPatterns().size() == 62);
    };
};
