// Mermaid 12.2.0 Event Modeling syntax. MIT; see THIRD_PARTY_NOTICES.md.
module folia.mermaid.eventmodeling;
import std;

namespace folia::mermaid {
namespace {

using View = std::u16string_view;

constexpr View keywords[] = {
    u"eventmodeling", u"tf", u"timeframe", u"rf", u"resetframe",
    u"entity", u"data", u"note", u"gwt", u"given", u"when", u"then",
    u"ui", u"rmo", u"readmodel", u"cmd", u"command", u"evt", u"event", u"pcr", u"processor",
    u"json", u"jsobj", u"figma", u"salt", u"uri", u"md", u"html", u"text",
};

constexpr bool horizontal(char16_t ch) { return ch == u' ' || ch == u'\t'; }
constexpr bool ascii_break(char16_t ch) { return ch == u'\n' || ch == u'\r'; }
constexpr bool line_break(char16_t ch) {
    return ascii_break(ch) || ch == u'\u2028' || ch == u'\u2029';
}
constexpr bool space(char16_t ch) {
    return horizontal(ch) || line_break(ch) || ch == u'\v' || ch == u'\f' ||
        ch == u'\u00a0' || ch == u'\u1680' || (ch >= u'\u2000' && ch <= u'\u200a') ||
        ch == u'\u202f' || ch == u'\u205f' || ch == u'\u3000' || ch == u'\ufeff';
}
constexpr bool identifier_start(char16_t ch) {
    return ch == u'_' || (ch >= u'A' && ch <= u'Z') || (ch >= u'a' && ch <= u'z');
}
constexpr bool digit(char16_t ch) { return ch >= u'0' && ch <= u'9'; }

enum class Kind { end, keyword, symbol, id, frame_id, inline_data, block_data, title, acc_title, acc_descr, invalid };
struct Token {
    Kind kind;
    View text;
    std::size_t line;
};

class Lexer {
public:
    explicit Lexer(View source) : source(source) {}

    Token next() {
        while (true) {
            if (source.empty()) return {Kind::end, {}, line};
            // The pinned Chevrotain lexer considers the metadata terminals
            // at horizontal space before its generic whitespace token.
            if (horizontal(source.front())) {
                if (auto token = metadata()) return *token;
            }
            if (!skip_hidden()) break;
        }

        const auto first = source.front();
        if (identifier_start(first)) {
            auto end = std::size_t{1};
            while (identifier_start(at(end)) || digit(at(end))) ++end;
            const auto word = source.substr(0, end);
            const auto boundary = end == source.size() || space(at(end)) ||
                source.substr(end).starts_with(u"%%");
            const auto keyword = std::ranges::find(keywords, word) != std::end(keywords);
            return take(keyword && (word != u"eventmodeling" || boundary) ? Kind::keyword : Kind::id, end);
        }
        if (digit(first)) {
            auto end = std::size_t{1};
            while (end < 3 && digit(at(end))) ++end;
            return take(Kind::frame_id, end);
        }
        if (first == u'{') {
            auto index = std::size_t{1};
            while (horizontal(at(index))) ++index;
            if (at(index) == u'\r') ++index;
            if (at(index) == u'\n') {
                if (const auto end = fenced_end(index + 1, u"}")) return take(Kind::block_data, *end);
            }
        }
        if (first == u'{' || first == u'"' || first == u'\'') {
            const auto closing = first == u'{' ? u'}' : first;
            const auto end = source.find_first_of(u"\r\n\u2028\u2029", 1);
            const auto match = source.substr(0, end).rfind(closing);
            if (match != View::npos && match > 0) return take(Kind::inline_data, match + 1);
        }
        for (const auto symbol : {u"->>", u"[[", u"]]", u".", u"`"}) {
            if (source.starts_with(symbol)) return take(Kind::symbol, View(symbol).size());
        }
        return take(Kind::invalid, 1);
    }

private:
    View source;
    std::size_t line = 1;

    char16_t at(std::size_t index) const { return index < source.size() ? source[index] : u'\0'; }

    Token take(Kind kind, std::size_t end) {
        const auto value = source.substr(0, end);
        const auto token_line = line;
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (value[i] == u'\n' || (value[i] == u'\r' && at(i + 1) != u'\n')) ++line;
        }
        source.remove_prefix(end);
        return {kind, value, token_line};
    }

    std::size_t line_end(std::size_t start) const {
        const auto end = source.find_first_of(u"\r\n", start);
        return end == View::npos ? source.size() : end;
    }

    std::optional<std::size_t> fence_end(std::size_t end) const {
        if (at(end) == u'\n') return end + 1;
        if (at(end) == u'\r' && at(end + 1) == u'\n') return end + 2;
        if (end == source.size() || space(at(end))) return end;
        return std::nullopt;
    }

    std::optional<std::size_t> fenced_end(std::size_t index, View delimiter) const {
        while (index < source.size()) {
            if (source.substr(index).starts_with(delimiter)) {
                if (const auto end = fence_end(index + delimiter.size())) return end;
            }
            const auto next = source.find(u'\n', index);
            if (next == View::npos) break;
            index = next + 1;
        }
        return std::nullopt;
    }

    bool skip_hidden() {
        const auto first = source.front();
        if (space(first)) {
            auto end = std::size_t{1};
            while (space(at(end))) ++end;
            take(Kind::invalid, end);
            return true;
        }
        if (source.starts_with(u"---")) {
            auto index = std::size_t{3};
            while (horizontal(at(index))) ++index;
            if (at(index) == u'\r') ++index;
            if (at(index) == u'\n') {
                if (const auto end = fenced_end(index + 1, u"---")) {
                    take(Kind::invalid, *end);
                    return true;
                }
            }
        }
        if (source.starts_with(u"%%{")) {
            for (auto close = source.find(u"}%%", 3); close != View::npos;
                 close = source.find(u"}%%", close + 3)) {
                if (const auto end = fence_end(close + 3)) {
                    take(Kind::invalid, *end);
                    return true;
                }
            }
        }
        if (source.starts_with(u"%%") || source.starts_with(u"//")) {
            take(Kind::invalid, line_end(2));
            return true;
        }
        if (source.starts_with(u"/*")) {
            const auto close = source.find(u"*/", 2);
            if (close != View::npos) {
                take(Kind::invalid, close + 2);
                return true;
            }
        }
        return false;
    }

    std::optional<Token> metadata() {
        auto start = std::size_t{1};
        while (horizontal(at(start))) ++start;
        const auto tail = source.substr(start);
        auto line_value = [&](Kind kind, std::size_t end) {
            const auto text = source.substr(end, line_end(end) - end);
            const auto comment = text.find(u"%%");
            return take(kind, end + (comment == View::npos ? text.size() : comment));
        };
        if (tail.starts_with(u"title")) {
            auto end = start + 5;
            return horizontal(at(end)) ? line_value(Kind::title, end) : take(Kind::title, end);
        }
        if (tail.starts_with(u"accTitle") || tail.starts_with(u"accDescr")) {
            const auto description = tail.starts_with(u"accDescr");
            auto end = start + 8;
            while (horizontal(at(end))) ++end;
            if (at(end) == u':') return line_value(description ? Kind::acc_descr : Kind::acc_title, end + 1);
            if (description) {
                while (space(at(end))) ++end;
                if (at(end) == u'{') {
                    const auto close = source.find(u'}', end + 1);
                    if (close != View::npos) return take(Kind::acc_descr, close + 1);
                }
            }
        }
        return std::nullopt;
    }
};

std::u16string data_contents(View raw, bool block) {
    const auto open = raw.find(block ? u"{\n" : u"{");
    const auto start = open == View::npos ? (block ? 1u : 0u) : open + (block ? 2 : 1);
    const auto close = raw.rfind(u'}');
    if (close == View::npos || close <= start) return {};
    return std::u16string(raw.substr(start, close - start - 1));
}

struct Error {
    std::string message;
};

class Parser {
public:
    explicit Parser(View source) : lexer(source), token(lexer.next()) {}

    EventmodelingDiagram parse() {
        require(u"eventmodeling", "'eventmodeling'");
        EventmodelingDiagram document;
        while (token.kind != Kind::end) {
            if (token.kind == Kind::title) document.title = take();
            else if (token.kind == Kind::acc_title) {
                const auto raw = take();
                document.acc_title = raw.substr(raw.find_first_not_of(u" \t"));
            } else if (token.kind == Kind::acc_descr) document.acc_description = take();
            else if (accept(u"entity")) qualified_name();
            else if (accept(u"data")) {
                const auto id = identifier();
                data_entities.try_emplace(id, block());
            } else if (accept(u"note")) {
                frame_id();
                block();
            } else if (accept(u"gwt")) {
                frame_id();
                require(u"given", "'given'");
                statements();
                if (accept(u"when")) statements();
                require(u"then", "'then'");
                statements();
            } else if (matches(u"tf") || matches(u"timeframe") || matches(u"rf") || matches(u"resetframe")) {
                document.frames.push_back(frame());
            } else fail("an Event Modeling statement");
        }

        for (auto& frame : document.frames) {
            if (const auto data = data_entities.find(frame.data_reference); data != data_entities.end()) {
                frame.data = data->second;
                frame.has_data = true;
                frame.data_from_reference = true;
            }
        }
        return document;
    }

private:
    Lexer lexer;
    Token token;
    std::set<View> frame_ids;
    std::map<View, std::u16string> data_entities;

    [[noreturn]] void fail(std::string_view expected) const {
        throw Error{std::format("Expected {} on Event Modeling line {}", expected, token.line)};
    }

    View take() {
        const auto text = token.text;
        token = lexer.next();
        return text;
    }

    bool matches(View text) const {
        return (token.kind == Kind::keyword || token.kind == Kind::symbol) && token.text == text;
    }

    bool accept(View text) {
        if (!matches(text)) return false;
        take();
        return true;
    }

    void require(View text, std::string_view expected) {
        if (!accept(text)) fail(expected);
    }

    View identifier() {
        if (token.kind != Kind::id) fail("an identifier");
        return take();
    }

    View frame_id() {
        if (token.kind != Kind::frame_id) fail("a frame ID");
        return take();
    }

    std::u16string qualified_name() {
        std::u16string name(identifier());
        while (accept(u".")) {
            name += u'.';
            name += identifier();
        }
        return name;
    }

    bool entity_kind() const {
        constexpr View kinds[] = {u"ui", u"rmo", u"readmodel", u"cmd", u"command", u"evt", u"event", u"pcr", u"processor"};
        return std::ranges::find(kinds, token.text) != std::end(kinds);
    }

    void statements() {
        if (!entity_kind()) fail("a GWT statement");
        do {
            take();
            identifier();
        } while (entity_kind());
    }

    bool data_type() {
        if (!accept(u"`")) return false;
        constexpr View types[] = {u"json", u"jsobj", u"figma", u"salt", u"uri", u"md", u"html", u"text"};
        if (std::ranges::find(types, token.text) == std::end(types)) fail("a data type");
        take();
        require(u"`", "a closing backtick");
        return true;
    }

    std::u16string block() {
        data_type();
        if (token.kind != Kind::block_data) fail("a data block");
        return data_contents(take(), true);
    }

    EventmodelingFrame frame() {
        EventmodelingFrame result;
        result.line = token.line;
        const auto declaration = take();
        result.reset = declaration == u"rf" || declaration == u"resetframe";
        const auto id = frame_id();
        if (!frame_ids.insert(id).second) {
            throw Error{std::format("Duplicate event modeling frame ID \"{}\" on line {}",
                                    std::string(id.begin(), id.end()), result.line)};
        }
        result.id = id;
        if (!entity_kind()) fail("a model entity type");
        result.kind = take();
        result.entity = qualified_name();
        while (accept(u"->>")) result.sources.emplace_back(frame_id());
        if (accept(u"[[")) {
            result.data_reference = identifier();
            require(u"]]", "']]'");
        }
        const auto typed = data_type();
        if (token.kind == Kind::inline_data) {
            result.data = data_contents(take(), false);
            result.has_data = true;
        } else if (typed) fail("inline data");
        return result;
    }
};

}

std::expected<EventmodelingDiagram, std::string> parse_eventmodeling(std::u16string_view source) {
    try {
        return Parser(source).parse();
    } catch (const Error& error) {
        return std::unexpected(error.message);
    }
}

}
