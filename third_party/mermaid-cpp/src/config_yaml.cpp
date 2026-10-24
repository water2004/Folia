module;
#include <yaml-cpp/parser.h>
#include <yaml-cpp/eventhandler.h>
#include <yaml-cpp/exceptions.h>
module folia.mermaid.frontmatter;
import std;

namespace folia::mermaid {
namespace {
[[noreturn]] void fail(std::string message) { throw std::runtime_error(std::move(message)); }

std::string utf8(std::u16string_view text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        std::uint32_t c = text[i];
        if (c >= 0xd800 && c <= 0xdbff) {
            if (++i == text.size() || text[i] < 0xdc00 || text[i] > 0xdfff) fail("Invalid YAML UTF-16");
            c = 0x10000 + ((c - 0xd800) << 10) + text[i] - 0xdc00;
        } else if (c >= 0xdc00 && c <= 0xdfff) fail("Invalid YAML UTF-16");
        if (c < 0x80) out += static_cast<char>(c);
        else {
            if (c < 0x800) out += static_cast<char>(0xc0 | (c >> 6));
            else {
                if (c < 0x10000) out += static_cast<char>(0xe0 | (c >> 12));
                else {
                    out += static_cast<char>(0xf0 | (c >> 18));
                    out += static_cast<char>(0x80 | ((c >> 12) & 63));
                }
                out += static_cast<char>(0x80 | ((c >> 6) & 63));
            }
            out += static_cast<char>(0x80 | (c & 63));
        }
    }
    return out;
}

std::u16string utf16(std::string_view text) {
    std::u16string out;
    for (std::size_t i = 0; i < text.size();) {
        auto c = static_cast<unsigned char>(text[i++]);
        std::uint32_t scalar = c;
        unsigned count = 0;
        if (c >= 0xc2 && c <= 0xdf) { scalar = c & 31; count = 1; }
        else if (c >= 0xe0 && c <= 0xef) { scalar = c & 15; count = 2; }
        else if (c >= 0xf0 && c <= 0xf4) { scalar = c & 7; count = 3; }
        else if (c >= 0x80) fail("Invalid YAML UTF-8 output");
        for (unsigned j = 0; j < count; ++j) {
            if (i == text.size()) fail("Truncated YAML UTF-8 output");
            auto next = static_cast<unsigned char>(text[i++]);
            if ((next & 0xc0) != 0x80) fail("Invalid YAML UTF-8 continuation");
            scalar = (scalar << 6) | (next & 63);
        }
        if ((count == 1 && scalar < 0x80) || (count == 2 && scalar < 0x800) ||
            (count == 3 && scalar < 0x10000) || scalar > 0x10ffff ||
            (scalar >= 0xd800 && scalar <= 0xdfff)) fail("Invalid YAML Unicode scalar");
        if (scalar < 0x10000) out += static_cast<char16_t>(scalar);
        else {
            scalar -= 0x10000;
            out += static_cast<char16_t>(0xd800 + (scalar >> 10));
            out += static_cast<char16_t>(0xdc00 + (scalar & 1023));
        }
    }
    return out;
}

bool digits_in_base(std::string_view text, unsigned base) {
    if (text.empty()) return false;
    for (char c : text) {
        const auto digit = c >= '0' && c <= '9' ? c - '0' :
            c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 99;
        if (static_cast<unsigned>(digit) >= base) return false;
    }
    return true;
}
bool integer_syntax(std::string_view text) {
    if (text.starts_with('-') || text.starts_with('+')) text.remove_prefix(1);
    if (text.starts_with("0b")) return digits_in_base(text.substr(2), 2);
    if (text.starts_with("0o")) return digits_in_base(text.substr(2), 8);
    if (text.starts_with("0x")) return digits_in_base(text.substr(2), 16);
    return digits_in_base(text, 10);
}
bool float_syntax(std::string_view text) {
    const bool signed_value = text.starts_with('-') || text.starts_with('+');
    if (signed_value) text.remove_prefix(1);
    // The released resolver's leading-dot alternative has no sign prefix.
    if (text.empty() || (signed_value && text.front() == '.')) return false;
    const auto exponent = text.find_first_of("eE");
    if (exponent != text.npos) {
        auto suffix = text.substr(exponent + 1);
        if (suffix.starts_with('-') || suffix.starts_with('+')) suffix.remove_prefix(1);
        if (!digits_in_base(suffix, 10)) return false;
        text = text.substr(0, exponent);
    }
    const auto dot = text.find('.');
    if (dot == text.npos) return digits_in_base(text, 10);
    if (dot == 0) return digits_in_base(text.substr(1), 10);
    return digits_in_base(text.substr(0, dot), 10) &&
        (dot + 1 == text.size() || digits_in_base(text.substr(dot + 1), 10));
}

// Bases 2/8/16 are rounded once to binary64, including the guard/sticky bits.
// Repeated double multiply/add would incorrectly round large integer literals.
double based_integer(std::string_view digits, unsigned width) {
    std::string bits;
    for (char c : digits) {
        unsigned n = c <= '9' ? c - '0' : (c <= 'F' ? c - 'A' + 10 : c - 'a' + 10);
        for (unsigned i = width; i > 0; --i) bits += ((n >> (i - 1)) & 1) ? '1' : '0';
    }
    const auto first = bits.find('1');
    if (first == std::string::npos) return 0;
    std::string_view significant{bits.data() + first, bits.size() - first};
    if (significant.size() > 1024) return std::numeric_limits<double>::infinity();
    std::uint64_t mantissa = 0;
    for (std::size_t i = 0; i < std::min<std::size_t>(53, significant.size()); ++i) {
        mantissa = (mantissa << 1) | (significant[i] - '0');
    }
    if (significant.size() <= 53) return static_cast<double>(mantissa);
    if (significant[53] == '1' && ((mantissa & 1) || significant.substr(54).find('1') != std::string_view::npos)) ++mantissa;
    return std::ldexp(static_cast<double>(mantissa), static_cast<int>(significant.size()) - 53);
}

std::optional<double> decimal(std::string_view text) {
    if (text.starts_with('+')) text.remove_prefix(1);
    double value{};
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec == std::errc{} && result.ptr == text.data() + text.size()) return value;
    if (result.ec == std::errc::result_out_of_range) {
        // from_chars reports both overflow and underflow. Determine magnitude
        // from the decimal digits/exponent without a locale-dependent strtod.
        auto unsigned_text = text;
        if (unsigned_text.starts_with('-')) unsigned_text.remove_prefix(1);
        const auto exponent_at = unsigned_text.find_first_of("eE");
        const auto significand = unsigned_text.substr(0, exponent_at);
        const auto dot = significand.find('.');
        auto exponent = std::int64_t{0};
        if (exponent_at != std::string_view::npos) {
            auto exp = unsigned_text.substr(exponent_at + 1);
            bool negative = exp.starts_with('-');
            if (negative || exp.starts_with('+')) exp.remove_prefix(1);
            for (char c : exp) exponent = std::min<std::int64_t>(10000000, exponent * 10 + c - '0');
            if (negative) exponent = -exponent;
        }
        std::int64_t digit_index = 0;
        for (char c : significand) {
            if (c == '.') continue;
            if (c != '0') {
                const auto order = static_cast<std::int64_t>(dot == std::string_view::npos ? significand.size() : dot) - digit_index - 1 + exponent;
                if (order < 0) return text.starts_with('-') ? -0.0 : 0.0;
                return std::nullopt;
            }
            ++digit_index;
        }
        return text.starts_with('-') ? -0.0 : 0.0;
    }
    return std::nullopt;
}

// Resolvers follow js-yaml 4.3.0 lib/type/{null,bool,int,float}.js, in order.
ConfigValue scalar_value(std::string_view text, std::string_view tag) {
    ConfigValue value;
    const bool implicit = tag == "?";
    const auto is_tag = [&](std::string_view suffix) { return tag == std::string("tag:yaml.org,2002:") + std::string(suffix); };
    if ((implicit || is_tag("null")) && (text.empty() || text == "~" || text == "null" || text == "Null" || text == "NULL")) return value;
    if (implicit || is_tag("bool")) {
        if (text == "true" || text == "True" || text == "TRUE" || text == "false" || text == "False" || text == "FALSE") {
            value.kind = ConfigKind::boolean;
            value.boolean = text == "true" || text == "True" || text == "TRUE";
            return value;
        }
    }
    if ((implicit || is_tag("int")) && integer_syntax(text)) {
        auto digits = text;
        bool negative = digits.starts_with('-');
        if (negative || digits.starts_with('+')) digits.remove_prefix(1);
        std::optional<double> number;
        if (digits.starts_with("0b") || digits.starts_with("0o") || digits.starts_with("0x")) {
            const auto width = digits[1] == 'b' ? 1u : digits[1] == 'o' ? 3u : 4u;
            number = based_integer(digits.substr(2), width) * (negative ? -1 : 1);
        } else number = digits == "0" ? std::optional{0.0} : decimal(text);
        if (number && std::isfinite(*number)) {
            value.kind = ConfigKind::number; value.number = *number; return value;
        }
    }
    if (implicit || is_tag("float")) {
        std::optional<double> number;
        auto unsigned_text = text;
        if (unsigned_text.starts_with('-') || unsigned_text.starts_with('+')) unsigned_text.remove_prefix(1);
        if (float_syntax(text)) number = decimal(text);
        else if (unsigned_text == ".inf" || unsigned_text == ".Inf" || unsigned_text == ".INF") number = text.starts_with('-') ? -std::numeric_limits<double>::infinity() : std::numeric_limits<double>::infinity();
        else if (text == ".nan" || text == ".NaN" || text == ".NAN") number = std::numeric_limits<double>::quiet_NaN();
        if (number) { value.kind = ConfigKind::number; value.number = *number; return value; }
    }
    if (!implicit && tag != "!" && !is_tag("str")) fail("Unknown YAML tag or invalid explicit scalar");
    value.kind = ConfigKind::string; value.string = utf16(text); return value;
}

std::u16string mapping_key(const ConfigValue& value) {
    if (value.kind == ConfigKind::mapping) return u"[object Object]";
    if (value.kind == ConfigKind::sequence) {
        std::u16string result;
        for (std::size_t i = 0; i < value.sequence.size(); ++i) {
            if (i) result += u',';
            const auto& item = value.sequence[i];
            if (item.kind == ConfigKind::sequence) fail("Nested arrays are not supported inside YAML keys");
            if (item.kind != ConfigKind::null) result += mapping_key(item);
        }
        return result;
    }
    return *config_to_string(value);
}

class Builder final : public YAML::EventHandler {
    struct Frame {
        ConfigValue value;
        YAML::anchor_t anchor;
        std::optional<std::u16string> key;
    };
    YamlLimits limits;
    std::size_t values = 0, units = 0, documents = 0;
    std::vector<Frame> stack;
    std::map<YAML::anchor_t, ConfigValue> anchors;

    void charge(std::size_t amount, std::size_t& used, std::size_t maximum) {
        if (amount > maximum - used) fail("YAML expansion budget exceeded");
        used += amount;
    }
    void account(const ConfigValue& value, std::size_t depth) {
        if (depth > limits.depth) fail("YAML depth limit exceeded");
        charge(1, values, limits.values);
        charge(value.string.size(), units, limits.expanded_units);
        for (const auto& item : value.sequence) account(item, depth + 1);
        for (const auto& [key, item] : value.mapping) {
            charge(key.size(), units, limits.expanded_units);
            account(item, depth + 1);
        }
    }
    void attach(ConfigValue value, YAML::anchor_t anchor) {
        if (anchor != YAML::NullAnchor) {
            account(value, stack.size()); // Include the owned anchor copy in budgets.
            anchors.insert_or_assign(anchor, value);
        }
        if (stack.empty()) { root = std::move(value); return; }
        auto& parent = stack.back();
        if (parent.value.kind == ConfigKind::sequence) parent.value.sequence.push_back(std::move(value));
        else if (!parent.key) {
            parent.key = mapping_key(value);
            charge(parent.key->size(), units, limits.expanded_units);
        } else {
            if (!parent.value.mapping.emplace(*parent.key, std::move(value)).second) fail("Duplicate YAML mapping key");
            parent.key.reset();
        }
    }
    void start(ConfigKind kind, const std::string& tag, YAML::anchor_t anchor) {
        const auto expected = kind == ConfigKind::sequence ? "tag:yaml.org,2002:seq" : "tag:yaml.org,2002:map";
        if (tag != "?" && tag != "!" && tag != expected) fail("Unknown or mismatched YAML collection tag");
        ConfigValue value; value.kind = kind;
        account(value, stack.size());
        stack.push_back({std::move(value), anchor, {}});
    }
    void end() {
        auto frame = std::move(stack.back()); stack.pop_back();
        if (frame.key) fail("Missing YAML mapping value");
        attach(std::move(frame.value), frame.anchor);
    }
public:
    ConfigValue root;
    explicit Builder(YamlLimits requested) : limits(requested) {
        // Keep the native parser and recursive value operations below a fixed
        // stack ceiling even if the caller supplies a larger depth budget.
        limits.depth = std::min<std::size_t>(limits.depth, 64);
    }
    void OnDocumentStart(const YAML::Mark&) override {
        if (++documents != 1) fail("Expected one YAML document");
    }
    void OnDocumentEnd() override {}
    void OnNull(const YAML::Mark&, YAML::anchor_t anchor) override {
        ConfigValue value; account(value, stack.size()); attach(std::move(value), anchor);
    }
    void OnScalar(const YAML::Mark&, const std::string& tag, YAML::anchor_t anchor, const std::string& text) override {
        auto value = scalar_value(text, tag);
        account(value, stack.size()); attach(std::move(value), anchor);
    }
    void OnAlias(const YAML::Mark&, YAML::anchor_t anchor) override {
        const auto found = anchors.find(anchor);
        if (found == anchors.end()) fail("Cyclic or unresolved YAML alias");
        account(found->second, stack.size());
        attach(found->second, YAML::NullAnchor);
    }
    void OnSequenceStart(const YAML::Mark&, const std::string& tag, YAML::anchor_t anchor, YAML::EmitterStyle::value) override { start(ConfigKind::sequence, tag, anchor); }
    void OnSequenceEnd() override { end(); }
    void OnMapStart(const YAML::Mark&, const std::string& tag, YAML::anchor_t anchor, YAML::EmitterStyle::value) override { start(ConfigKind::mapping, tag, anchor); }
    void OnMapEnd() override { end(); }
};
}

std::expected<ConfigValue, std::string> parse_config_yaml(std::u16string_view source, YamlLimits limits) {
    if (source.size() > limits.source_units) return std::unexpected("YAML source limit exceeded");
    try {
        if (source.find(u'\0') != source.npos) fail("NUL in YAML source");
        auto bytes = utf8(source);
        // A document-end token keeps yaml-cpp's event API from losing an
        // explicit tag on an empty node at EOF. It adds no scalar content;
        // js-yaml also supplies a final LF when the input has none.
        if (!bytes.empty() && bytes.back() != '\n' && bytes.back() != '\r') bytes += '\n';
        bytes += "...\n";
        std::istringstream input{bytes};
        YAML::Parser parser{input};
        Builder builder{limits};
        while (parser.HandleNextDocument(builder)) {}
        return std::move(builder.root);
    } catch (const std::exception& error) {
        return std::unexpected(std::string("YAML: ") + error.what());
    }
}
}
