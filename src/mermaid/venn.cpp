module folia.mermaid.venn;
import std;

namespace folia::mermaid {
namespace {
using View = std::u16string_view;
bool space(char16_t c) { return c == u' ' || c == u'\t' || c == u'\r'; }
bool digit(char16_t c) { return c >= u'0' && c <= u'9'; }
bool letter(char16_t c) { return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z'); }
View trim(View value) {
    while (!value.empty() && space(value.front())) value.remove_prefix(1);
    while (!value.empty() && space(value.back())) value.remove_suffix(1);
    return value;
}
bool is(View value, View lower) {
    return std::ranges::equal(value, lower, [](char16_t a, char16_t b) {
        return (a >= u'A' && a <= u'Z' ? a + (u'a' - u'A') : a) == b;
    });
}
bool reserved(View value) {
    return is(value, u"set") || is(value, u"union") || is(value, u"text") ||
        is(value, u"style") || is(value, u"venn-beta");
}
[[noreturn]] void invalid(const char* message) { throw std::runtime_error(message); }

class Line {
public:
    explicit Line(View line) : rest_(trim(line)) {}
    bool empty() { return rest_.empty() || rest_.starts_with(u"%%"); }
    bool take(char16_t c) {
        if (rest_.empty() || rest_.front() != c) return false;
        rest_ = trim(rest_.substr(1));
        return true;
    }
    View word() {
        if (rest_.empty() || !(letter(rest_.front()) || rest_.front() == u'_'))
            invalid("expected identifier");
        std::size_t length = 1;
        while (length < rest_.size()) {
            const auto c = rest_[length];
            if (!(letter(c) || digit(c) || c == u'_' || c == u'-')) break;
            ++length;
        }
        return consume(length);
    }
    std::u16string identifier(bool allow_number = false) {
        if (rest_.starts_with(u"\"")) return quoted();
        if (allow_number && !rest_.empty() &&
            (digit(rest_[0]) || rest_[0] == u'+' || rest_[0] == u'-' || rest_[0] == u'.'))
            return std::u16string(numeric());
        auto value = word();
        if (reserved(value)) invalid("expected identifier");
        return std::u16string(value);
    }
    VennSets identifiers() {
        VennSets ids{identifier()};
        while (take(u',')) ids.push_back(identifier());
        std::ranges::sort(ids);
        return ids;
    }
    std::u16string label() {
        if (rest_.starts_with(u"[\"")) {
            const auto end = rest_.find(u'"', 2);
            if (end == View::npos || end + 1 == rest_.size() || rest_[end + 1] != u']')
                invalid("unterminated quoted label");
            return std::u16string(trim(consume(end + 2).substr(2, end - 2)));
        }
        if (!rest_.starts_with(u"[")) return {};
        const auto end = rest_.find_first_of(u"]\"", 1);
        if (end == View::npos || rest_[end] != u']' || end == 1) invalid("invalid label");
        return std::u16string(trim(consume(end + 1).substr(1, end - 1)));
    }
    double size() {
        auto value = numeric();
        if (value.starts_with(u"+")) value.remove_prefix(1);
        const std::string ascii(value.begin(), value.end());
        double result;
        const auto conversion = std::from_chars(ascii.data(), ascii.data() + ascii.size(), result);
        if (conversion.ec != std::errc{} || conversion.ptr != ascii.data() + ascii.size() ||
            !std::isfinite(result) || result < 0)
            invalid("expected finite nonnegative area size");
        return result;
    }
    std::u16string title() {
        const auto end = rest_.find(u"%%");
        const auto value = trim(rest_.substr(0, end));
        if (value.empty() || value.find_first_of(u"#;") != View::npos) invalid("invalid title");
        rest_ = {};
        return std::u16string(value);
    }
    std::u16string style_value() {
        if (rest_.starts_with(u"\"")) return quoted();
        std::u16string result;
        while (!empty() && !rest_.starts_with(u",")) {
            View value;
            if (rest_.starts_with(u"#")) {
                std::size_t length = 1;
                while (length < rest_.size() && length <= 8) {
                    const auto c = rest_[length];
                    if (!(digit(c) || (c >= u'a' && c <= u'f') || (c >= u'A' && c <= u'F'))) break;
                    ++length;
                }
                if (length < 4) invalid("invalid hex color");
                value = consume(length);
            } else if (rest_.starts_with(u"rgb(") || rest_.starts_with(u"rgba(")) {
                const auto end = rest_.find(u')');
                if (end == View::npos) invalid("unterminated color");
                value = consume(end + 1);
            } else if (digit(rest_[0]) || rest_[0] == u'+' || rest_[0] == u'-' || rest_[0] == u'.') {
                value = numeric();
            } else {
                value = word();
                if (reserved(value)) invalid("invalid style value");
            }
            if (!result.empty()) result += u' ';
            result += value;
        }
        if (result.empty()) invalid("missing style value");
        return result;
    }
private:
    View consume(std::size_t length) {
        const auto value = rest_.substr(0, length);
        rest_ = trim(rest_.substr(length));
        return value;
    }
    std::u16string quoted() {
        const auto end = rest_.find(u'"', 1);
        if (end == View::npos) invalid("unterminated string");
        return std::u16string(consume(end + 1).substr(1, end - 1));
    }
    View numeric() {
        std::size_t pos = 0, digits = 0;
        if (!rest_.empty() && (rest_.front() == u'+' || rest_.front() == u'-')) ++pos;
        while (pos < rest_.size() && digit(rest_[pos])) { ++pos; ++digits; }
        if (pos + 1 < rest_.size() && rest_[pos] == u'.' && digit(rest_[pos + 1])) {
            ++pos;
            while (pos < rest_.size() && digit(rest_[pos])) { ++pos; ++digits; }
        }
        if (!digits) invalid("expected number");
        return consume(pos);
    }
    View rest_;
};
}

std::expected<VennDiagram, std::string> parse_venn(std::u16string_view source) {
    VennDiagram diagram;
    std::set<std::u16string> known;
    VennSets current;
    bool header = false;
    std::size_t line_number = 0;
    try {
        while (!source.empty()) {
            const auto end = source.find(u'\n');
            const auto raw = source.substr(0, end);
            source = end == View::npos ? View{} : source.substr(end + 1);
            ++line_number;
            Line line(raw);
            if (line.empty()) continue;
            const bool indented = space(raw.front());
            auto command = line.word();
            if (!header) {
                if (!is(command, u"venn-beta")) invalid("expected venn-beta");
                header = true;
                if (line.empty()) continue;
                command = line.word();
            }
            if (!indented) current.clear();
            if (is(command, u"title")) {
                diagram.title = line.title();
            } else if (is(command, u"set") || is(command, u"union")) {
                const bool singleton = is(command, u"set");
                auto ids = singleton ? VennSets{line.identifier()} : line.identifiers();
                if (singleton) {
                    if (!known.insert(ids.front()).second) invalid("duplicate set identifier");
                } else {
                    if (ids.size() < 2) invalid("union requires multiple identifiers");
                    if (std::ranges::adjacent_find(ids) != ids.end()) invalid("duplicate union member");
                    for (const auto& id : ids)
                        if (!known.contains(id)) invalid("unknown set identifier");
                }
                current = ids;
                auto label = line.label();
                const auto count = static_cast<double>(ids.size());
                const auto size = line.take(u':') ? line.size() : 10 / (count * count);
                diagram.areas.push_back({std::move(ids), size, std::move(label)});
            } else if (is(command, u"text")) {
                auto ids = indented && !current.empty() ? current : line.identifiers();
                auto id = line.identifier(true);
                auto label = line.label();
                diagram.texts.push_back({std::move(ids), std::move(id), std::move(label)});
            } else if (is(command, u"style")) {
                auto& paint = diagram.styles[line.identifiers()];
                do {
                    auto key = line.identifier();
                    if (!line.take(u':')) invalid("missing style colon");
                    paint[std::move(key)] = line.style_value();
                } while (line.take(u','));
            } else {
                invalid("unknown statement");
            }
            if (!line.empty()) invalid("unexpected trailing input");
        }
        if (!header) invalid("expected venn-beta");
        return diagram;
    } catch (const std::runtime_error& error) {
        return std::unexpected("Venn line " + std::to_string(line_number) + ": " + error.what());
    }
}
}
