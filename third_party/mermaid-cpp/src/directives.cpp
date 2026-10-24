// Ported from Mermaid 12.2.0 utils.ts detectDirective.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT licensed; see ../LICENSE.
module folia.mermaid.directives;
import std;
import folia.mermaid.directive_lexer;

namespace folia::mermaid {
namespace {
struct SyntaxError {};
struct LimitError { std::string message; };
struct Budget {
    const DirectiveLimits& limits;
    std::size_t values{}, units{};
    void value() {
        if (values >= limits.values) throw LimitError{"Directive value budget exceeded"};
        ++values;
    }
    void string(std::size_t count) {
        if (count > limits.string_units - units) throw LimitError{"Directive string budget exceeded"};
        units += count;
    }
};

// JSON.parse grammar, independent of the more permissive frontmatter YAML
// schema. Strings intentionally retain lone UTF-16 surrogates, including escapes.
class JsonParser {
    std::u16string_view text_;
    std::size_t cursor_{};
    Budget& budget_;
    bool take(char16_t c) {
        if (cursor_ == text_.size() || text_[cursor_] != c) return false;
        ++cursor_;
        return true;
    }
    void space() {
        while (cursor_ < text_.size() && (text_[cursor_] == u' ' || text_[cursor_] == u'\t' ||
               text_[cursor_] == u'\r' || text_[cursor_] == u'\n')) ++cursor_;
    }
    static bool digit(char16_t c) { return c >= u'0' && c <= u'9'; }
    std::u16string string() {
        if (!take(u'"')) throw SyntaxError{};
        std::u16string result;
        while (cursor_ < text_.size()) {
            auto c = text_[cursor_++];
            if (c == u'"') return result;
            if (c < 0x20) throw SyntaxError{};
            if (c == u'\\') {
                if (cursor_ == text_.size()) throw SyntaxError{};
                c = text_[cursor_++];
                switch (c) {
                case u'"': case u'\\': case u'/': break;
                case u'b': c = u'\b'; break;
                case u'f': c = u'\f'; break;
                case u'n': c = u'\n'; break;
                case u'r': c = u'\r'; break;
                case u't': c = u'\t'; break;
                case u'u': {
                    if (text_.size() - cursor_ < 4) throw SyntaxError{};
                    unsigned unit{};
                    for (int i = 0; i < 4; ++i) {
                        const auto h = text_[cursor_++];
                        unsigned nibble{};
                        if (digit(h)) nibble = h - u'0';
                        else if (h >= u'a' && h <= u'f') nibble = h - u'a' + 10;
                        else if (h >= u'A' && h <= u'F') nibble = h - u'A' + 10;
                        else throw SyntaxError{};
                        unit = unit * 16 + nibble;
                    }
                    c = static_cast<char16_t>(unit);
                    break;
                }
                default: throw SyntaxError{};
                }
            }
            budget_.string(1);
            result += c;
        }
        throw SyntaxError{};
    }
    double number() {
        const auto start = cursor_;
        const bool negative = take(u'-');
        if (!take(u'0')) {
            if (cursor_ == text_.size() || text_[cursor_] < u'1' || text_[cursor_] > u'9') throw SyntaxError{};
            while (cursor_ < text_.size() && digit(text_[cursor_])) ++cursor_;
        }
        if (take(u'.')) {
            const auto first = cursor_;
            while (cursor_ < text_.size() && digit(text_[cursor_])) ++cursor_;
            if (cursor_ == first) throw SyntaxError{};
        }
        if (take(u'e') || take(u'E')) {
            if (!take(u'+')) take(u'-');
            const auto first = cursor_;
            while (cursor_ < text_.size() && digit(text_[cursor_])) ++cursor_;
            if (cursor_ == first) throw SyntaxError{};
        }
        const auto token = text_.substr(start, cursor_ - start);
        const std::string ascii(token.begin(), token.end());
        double result{};
        const auto parsed = std::from_chars(ascii.data(), ascii.data() + ascii.size(), result);
        if (parsed.ec == std::errc{} && parsed.ptr == ascii.data() + ascii.size()) return result;
        if (parsed.ec != std::errc::result_out_of_range) throw SyntaxError{};

        // from_chars reports overflow and underflow using the same error. Find
        // the decimal order without strtod/locale or overflowing an exponent.
        const auto exponent_at = ascii.find_first_of("eE");
        const auto mantissa = std::string_view{ascii}.substr(negative ? 1 : 0,
            (exponent_at == ascii.npos ? ascii.size() : exponent_at) - (negative ? 1 : 0));
        const auto dot = mantissa.find('.');
        const auto integral_digits = dot == mantissa.npos ? mantissa.size() : dot;
        std::size_t preceding_zeros{};
        bool nonzero = false;
        for (char c : mantissa) {
            if (c == '.') continue;
            if (c != '0') { nonzero = true; break; }
            ++preceding_zeros;
        }
        // An arbitrarily large exponent cannot turn a zero mantissa into infinity.
        if (!nonzero) return negative ? -0.0 : 0.0;
        long long exponent{};
        if (exponent_at != ascii.npos) {
            auto index = exponent_at + 1;
            const bool minus = ascii[index] == '-';
            if (ascii[index] == '+' || minus) ++index;
            // Saturate beyond any possible mantissa displacement in this input.
            const auto cap = static_cast<long long>(ascii.size()) + 1000;
            for (; index < ascii.size(); ++index) exponent = std::min(cap, exponent * 10 + ascii[index] - '0');
            if (minus) exponent = -exponent;
        }
        const auto order = exponent + static_cast<long long>(integral_digits) -
                           static_cast<long long>(preceding_zeros) - 1;
        result = order < 0 ? 0.0 : std::numeric_limits<double>::infinity();
        return negative ? -result : result;
    }
    ConfigValue value(std::size_t depth) {
        if (depth > std::min<std::size_t>(budget_.limits.depth, 64)) throw LimitError{"Directive depth budget exceeded"};
        budget_.value();
        space();
        if (cursor_ == text_.size()) throw SyntaxError{};
        ConfigValue result;
        const auto c = text_[cursor_];
        if (c == u'"') {
            result.kind = ConfigKind::string; result.string = string();
        } else if (take(u'[')) {
            result.kind = ConfigKind::sequence;
            space();
            if (!take(u']')) {
                do { result.sequence.push_back(value(depth + 1)); space(); } while (take(u','));
                if (!take(u']')) throw SyntaxError{};
            }
        } else if (take(u'{')) {
            result.kind = ConfigKind::mapping;
            space();
            if (!take(u'}')) {
                do {
                    space();
                    auto key = string();
                    space();
                    if (!take(u':')) throw SyntaxError{};
                    auto item = value(depth + 1);
                    result.mapping.insert_or_assign(std::move(key), std::move(item));
                    space();
                } while (take(u','));
                if (!take(u'}')) throw SyntaxError{};
            }
        } else if (text_.substr(cursor_).starts_with(u"null")) {
            cursor_ += 4;
        } else if (text_.substr(cursor_).starts_with(u"true") || text_.substr(cursor_).starts_with(u"false")) {
            result.kind = ConfigKind::boolean;
            result.boolean = c == u't';
            cursor_ += result.boolean ? 4 : 5;
        } else {
            result.kind = ConfigKind::number; result.number = number();
        }
        return result;
    }
public:
    JsonParser(std::u16string_view text, Budget& budget) : text_{text}, budget_{budget} {}
    ConfigValue parse() {
        auto result = value(0);
        space();
        if (cursor_ != text_.size()) throw SyntaxError{};
        return result;
    }
};

std::u16string prepare(std::u16string_view input) {
    input = detail::trim_ecmascript(input);
    std::u16string result;
    result.reserve(input.size());
    // The official negative lookahead starts with '{', while its following
    // lookahead requires '}%%'. Thus only %%}%% ... LF can be removed. Dot
    // cannot cross CR/LS/PS. Scan each line once, including failed matches.
    std::size_t start{};
    while (start < input.size()) {
        const auto stop = input.find_first_of(u"\r\n\u2028\u2029", start);
        const auto end = stop == input.npos ? input.size() : stop;
        const auto segment = input.substr(start, end - start);
        const auto marker = segment.find(u"%%}%%");
        if (stop != input.npos && input[stop] == u'\n' && marker != segment.npos) {
            result += segment.substr(0, marker);
        } else {
            result += segment;
            if (stop != input.npos) result += input[stop];
        }
        start = stop == input.npos ? input.size() : stop + 1;
    }
    std::replace(result.begin(), result.end(), u'\'', u'"');
    return result;
}
bool matches(std::u16string_view type, DirectiveFilter filter) {
    if (filter == DirectiveFilter::all) return true;
    if (filter == DirectiveFilter::wrap) return type.find(u"wrap") != type.npos;
    // Captured types contain only ASCII word units, so a word boundary can
    // occur only at the end; the official init pattern has no start anchor.
    return type.ends_with(u"init") || type.ends_with(u"initialize");
}
} // namespace

std::expected<std::vector<Directive>, std::string> detect_directives(
    std::u16string_view text, DirectiveFilter filter, const DirectiveLimits& limits) {
    DirectiveScanState state;
    return detect_directives(text, filter, state, limits);
}
std::expected<std::vector<Directive>, std::string> detect_directives(
    std::u16string_view text, DirectiveFilter filter, DirectiveScanState& state,
    const DirectiveLimits& limits) {
    if (text.size() > limits.source_units) return std::unexpected("Directive source budget exceeded");
    try {
        const auto prepared = prepare(text);
        std::vector<Directive> result;
        Budget budget{limits};
        std::size_t count{};
        while (const auto match = detail::next_directive(prepared, state.cursor)) {
            state.cursor = match->end;
            if (count >= limits.directives) throw LimitError{"Directive count budget exceeded"};
            ++count;
            if (!matches(match->type, filter)) continue;
            budget.string(match->type.size());
            Directive directive{std::u16string{match->type}, {}};
            if (match->word_argument) {
                budget.value();
                budget.string(match->argument.size());
                directive.args.kind = ConfigKind::string;
                directive.args.string = match->argument;
            } else if (!match->argument.empty()) {
                directive.args = JsonParser{detail::trim_ecmascript(match->argument), budget}.parse();
            } else {
                budget.value();
            }
            result.push_back(std::move(directive));
        }
        state.cursor = 0;
        if (result.empty()) {
            budget.value();
            budget.string(prepared.size());
            result.push_back(Directive{prepared, {}});
        }
        return result;
    } catch (const SyntaxError&) {
        return std::vector<Directive>{Directive{std::nullopt, {}}};
    } catch (const LimitError& error) {
        return std::unexpected(error.message);
    }
}
}
