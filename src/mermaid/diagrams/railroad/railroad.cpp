// Mermaid 12.2.0 Railroad grammars and AST transforms. MIT; see THIRD_PARTY_NOTICES.md.
module folia.mermaid.railroad;
import std;

namespace folia::mermaid {
namespace {

using View = std::u16string_view;
using Kind = RailroadNodeKind;
using Node = RailroadNode;

enum class Syntax { ir, ebnf, abnf, peg };

constexpr std::pair<View, Syntax> headers[] = {
    {u"railroad-beta", Syntax::ir},
    {u"railroad-ebnf-beta", Syntax::ebnf},
    {u"railroad-abnf-beta", Syntax::abnf},
    {u"railroad-peg-beta", Syntax::peg},
};

constexpr std::pair<View, Kind> ir_functions[] = {
    {u"terminal", Kind::terminal}, {u"nonterminal", Kind::nonterminal},
    {u"special", Kind::special}, {u"sequence", Kind::sequence},
    {u"choice", Kind::choice}, {u"optional", Kind::optional},
    {u"oneOrMore", Kind::repetition}, {u"zeroOrMore", Kind::repetition},
};

enum class TokenKind {
    end, invalid, header, identifier, string, number, repeat, special,
    title, acc_title, acc_description, acc_block, ir_function, symbol,
};

struct Token {
    TokenKind kind{TokenKind::end};
    View text;
    std::size_t offset{};
};

constexpr bool horizontal(char16_t ch) { return ch == u' ' || ch == u'\t'; }
constexpr bool line_break(char16_t ch) { return ch == u'\r' || ch == u'\n'; }
constexpr bool js_line_break(char16_t ch) {
    return line_break(ch) || ch == u'\u2028' || ch == u'\u2029';
}
constexpr bool js_space(char16_t ch) {
    return horizontal(ch) || js_line_break(ch) || ch == u'\v' || ch == u'\f' ||
        ch == u'\u00a0' || ch == u'\u1680' || (ch >= u'\u2000' && ch <= u'\u200a') ||
        ch == u'\u202f' || ch == u'\u205f' || ch == u'\u3000' || ch == u'\ufeff';
}
constexpr bool letter(char16_t ch) {
    return (ch >= u'a' && ch <= u'z') || (ch >= u'A' && ch <= u'Z');
}
constexpr bool digit(char16_t ch) { return ch >= u'0' && ch <= u'9'; }
constexpr bool hex(char16_t ch) {
    return digit(ch) || (ch >= u'a' && ch <= u'f') || (ch >= u'A' && ch <= u'F');
}

View trim(View value) {
    while (!value.empty() && js_space(value.front())) value.remove_prefix(1);
    while (!value.empty() && js_space(value.back())) value.remove_suffix(1);
    return value;
}

std::u16string collapse_horizontal(View value) {
    std::u16string result;
    for (std::size_t i = 0; i < value.size();) {
        const auto start = i++;
        if (horizontal(value[start])) {
            while (i < value.size() && horizontal(value[i])) ++i;
        }
        result += i - start == 1 ? value[start] : u' ';
    }
    return result;
}

std::u16string normalize_description(View block) {
    std::u16string result;
    char16_t separator{};
    while (!block.empty()) {
        const auto end = block.find_first_of(u"\r\n\u2028\u2029");
        const auto line = trim(block.substr(0, end));
        if (!line.empty()) {
            if (!result.empty()) result += separator;
            result += collapse_horizontal(line);
        }
        if (end == View::npos) break;
        if (!line.empty()) separator = block[end];
        block.remove_prefix(end + 1);
    }
    return result;
}

std::u16string unquote(View value, bool decode) {
    const auto content = value.size() < 2 ? View{} : value.substr(1, value.size() - 2);
    if (!decode) return std::u16string(content);
    std::u16string result;
    for (std::size_t i = 0; i < content.size(); ++i) {
        auto ch = content[i];
        if (ch == u'\\' && i + 1 < content.size()) {
            ch = content[++i];
            switch (ch) {
            case u'n': ch = u'\n'; break;
            case u'r': ch = u'\r'; break;
            case u't': ch = u'\t'; break;
            }
        }
        result += ch;
    }
    return result;
}

class Lexer {
public:
    Lexer(View input, Syntax syntax) : source(input), syntax(syntax) {}

    Token next() {
        while (skip_hidden()) {}
        const auto tail = source.substr(position);
        if (tail.empty()) return {TokenKind::end, {}, position};

        for (const auto& [header, language] : headers) {
            if (language == syntax && tail.starts_with(header) &&
                (tail.size() == header.size() || js_space(tail[header.size()]) ||
                 tail.substr(header.size()).starts_with(u"%%"))) {
                return take(TokenKind::header, header.size());
            }
        }

        if (tail.starts_with(u"title")) {
            constexpr std::size_t prefix = 5;
            if (horizontal(at(prefix))) return take_line_metadata(TokenKind::title, prefix);
            return take(TokenKind::title, prefix, prefix);
        }
        if (tail.starts_with(u"accTitle") || tail.starts_with(u"accDescr")) {
            auto end = std::size_t{8};
            while (horizontal(at(end))) ++end;
            const bool description = tail.starts_with(u"accDescr");
            if (at(end) == u':') {
                return take_line_metadata(description ? TokenKind::acc_description : TokenKind::acc_title,
                                          end + 1);
            }
            if (description) {
                while (js_space(at(end))) ++end;
                if (at(end) == u'{') {
                    const auto close = tail.find(u'}', end + 1);
                    if (close != View::npos) return take(TokenKind::acc_block, close + 1, end + 1);
                }
            }
        }

        const auto first = tail.front();
        if (letter(first) || (syntax != Syntax::abnf && first == u'_')) {
            auto end = std::size_t{1};
            while (letter(at(end)) || digit(at(end)) || at(end) == u'-' ||
                   (syntax != Syntax::abnf && at(end) == u'_')) ++end;
            if (syntax == Syntax::ir) {
                for (const auto& [name, kind] : ir_functions) {
                    if (tail.substr(0, end) == name) return take(TokenKind::ir_function, end);
                }
            }
            return take(TokenKind::identifier, end);
        }
        if (first == u'"' || (first == u'\'' && syntax != Syntax::abnf)) {
            for (auto end = std::size_t{1}; end < tail.size(); ++end) {
                if (at(end) == first) return take(TokenKind::string, end + 1);
                if (at(end) == u'\\' && syntax != Syntax::abnf) {
                    if (++end == tail.size() || js_line_break(at(end))) break;
                }
            }
            return take(TokenKind::invalid, 1);
        }
        if (syntax == Syntax::abnf) {
            if (first == u'%' && View(u"xXdDbB").find(at(1)) != View::npos && hex(at(2))) {
                auto end = std::size_t{3};
                while (hex(at(end))) ++end;
                while ((at(end) == u'.' || at(end) == u'-') && hex(at(end + 1))) {
                    end += 2;
                    while (hex(at(end))) ++end;
                }
                return take(TokenKind::number, end);
            }
            if (digit(first) || first == u'*') {
                auto end = std::size_t{0};
                while (digit(at(end))) ++end;
                if (at(end) == u'*') {
                    ++end;
                    while (digit(at(end))) ++end;
                }
                return take(TokenKind::repeat, end);
            }
        }
        if (syntax == Syntax::ebnf && first == u'?') {
            const auto close = tail.find_first_of(u"?;", 1);
            if (close != View::npos && tail[close] == u'?' && !trim(tail.substr(1, close - 1)).empty()) {
                return take(TokenKind::special, close + 1);
            }
        }
        if (syntax == Syntax::ebnf && tail.starts_with(u"::=")) return take(TokenKind::symbol, 3);
        if (syntax == Syntax::peg && tail.starts_with(u"<-")) return take(TokenKind::symbol, 2);
        return take(TokenKind::symbol, 1);
    }

private:
    View source;
    Syntax syntax;
    std::size_t position{};

    char16_t at(std::size_t relative) const {
        return relative < source.size() - position ? source[position + relative] : u'\0';
    }

    Token take(TokenKind kind, std::size_t length, std::size_t prefix = 0) {
        Token token{kind, source.substr(position + prefix, length - prefix), position};
        position += length;
        return token;
    }

    Token take_line_metadata(TokenKind kind, std::size_t prefix) {
        auto value = source.substr(position + prefix);
        value = value.substr(0, value.find_first_of(u"\r\n"));
        value = value.substr(0, value.find(u"%%"));
        return take(kind, prefix + value.size(), prefix);
    }

    bool skip_hidden() {
        const auto tail = source.substr(position);
        if (tail.empty()) return false;
        const auto first = tail.front();
        if (horizontal(first) || line_break(first)) {
            do { ++position; } while (horizontal(at(0)) || line_break(at(0)));
            return true;
        }
        if (tail.starts_with(u"---")) {
            auto start = std::size_t{3};
            while (horizontal(at(start))) ++start;
            if (at(start) == u'\r') ++start;
            if (at(start) == u'\n') {
                for (auto fence = start + 1; fence <= tail.size();) {
                    if (tail.substr(fence).starts_with(u"---") && end_fence(fence + 3)) return true;
                    const auto newline = tail.find(u'\n', fence);
                    if (newline == View::npos) break;
                    fence = newline + 1;
                }
            }
        }
        if (tail.starts_with(u"%%{")) {
            for (auto end = tail.find(u"}%%", 3); end != View::npos; end = tail.find(u"}%%", end + 3)) {
                if (end_fence(end + 3)) return true;
            }
        }
        // A bare ABNF semicolon is the grammar keyword. A longer same-line
        // match wins in Chevrotain and hides the entire comment instead.
        if (tail.starts_with(u"%%") || (syntax == Syntax::peg && first == u'#') ||
            (syntax == Syntax::abnf && first == u';' && tail.size() > 1 && !line_break(at(1)))) {
            position += tail.substr(0, tail.find_first_of(u"\r\n")).size();
            return true;
        }
        if (((syntax == Syntax::ir || syntax == Syntax::ebnf) && tail.starts_with(u"/*")) ||
            (syntax == Syntax::ebnf && tail.starts_with(u"(*"))) {
            const auto close = tail.find(first == u'/' ? u"*/" : u"*)", 2);
            if (close != View::npos) {
                position += close + 2;
                return true;
            }
        }
        return false;
    }

    bool end_fence(std::size_t end) {
        if (at(end) == u'\n') ++end;
        else if (at(end) == u'\r' && at(end + 1) == u'\n') end += 2;
        else if (end != source.size() - position && !js_space(at(end))) return false;
        position += end;
        return true;
    }
};

Node group(Kind kind, std::vector<Node> children) {
    if (children.size() == 1) return std::move(children.front());
    return {kind, {}, std::move(children)};
}

Node wrap(Kind kind, Node child, double min = 0,
          double max = std::numeric_limits<double>::infinity()) {
    std::vector<Node> children;
    children.push_back(std::move(child));
    return {kind, {}, std::move(children), min, max};
}

double decimal(View value) {
    double number = 0;
    for (const auto ch : value) number = number * 10 + (ch - u'0');
    return number;
}

struct ParseError {
    std::size_t offset;
    std::string_view expected;
};

class Parser {
public:
    Parser(View source, Syntax syntax) : lexer(source, syntax), syntax(syntax), token(lexer.next()) {}

    RailroadDocument parse() {
        if (token.kind != TokenKind::header) fail("a railroad header");
        advance();
        RailroadDocument document;
        while (read_metadata(document)) {}
        while (token.kind != TokenKind::end) {
            if (token.kind != TokenKind::identifier) fail("a rule name");
            RailroadRule rule{std::u16string(token.text), {}};
            advance();
            if (syntax == Syntax::peg) require(u"<-", "'<-'");
            else if (syntax != Syntax::ebnf || !accept(u"::=")) require(u"=", "'='");
            rule.definition = syntax == Syntax::ir ? ir_expression(0) : choice(0);
            require(u";", "';'");
            document.rules.push_back(std::move(rule));
        }
        return document;
    }

private:
    Lexer lexer;
    Syntax syntax;
    Token token;

    void advance() { token = lexer.next(); }

    [[noreturn]] void fail(std::string_view expected) const {
        throw ParseError{token.offset, expected};
    }

    bool accept(View symbol) {
        if (token.kind != TokenKind::symbol || token.text != symbol) return false;
        advance();
        return true;
    }

    void require(View symbol, std::string_view expected) {
        if (!accept(symbol)) fail(expected);
    }

    bool read_metadata(RailroadDocument& document) {
        switch (token.kind) {
        case TokenKind::title: {
            auto title = collapse_horizontal(trim(token.text));
            if (!title.empty() && (title.front() == u'"' || title.front() == u'\'') &&
                title.front() == title.back()) {
                title = unquote(title, syntax != Syntax::abnf);
            }
            document.title = std::move(title);
            break;
        }
        case TokenKind::acc_title:
            document.acc_title = collapse_horizontal(trim(token.text));
            break;
        case TokenKind::acc_description:
            document.acc_description = collapse_horizontal(trim(token.text));
            break;
        case TokenKind::acc_block:
            document.acc_description = normalize_description(token.text.substr(0, token.text.size() - 1));
            break;
        default:
            return false;
        }
        advance();
        return true;
    }

    Node ir_expression(std::size_t depth) {
        if (depth >= 256) fail("a shallower expression");
        if (token.kind != TokenKind::ir_function) fail("a railroad expression function");
        const auto fn = token.text;
        const auto function = std::ranges::find(ir_functions, fn, [](const auto& f) { return f.first; });
        const auto kind = function->second;
        advance();
        require(u"(", "'('");
        Node result;
        if (kind == Kind::terminal || kind == Kind::nonterminal || kind == Kind::special) {
            if (token.kind != TokenKind::string) fail("a quoted label");
            result = {kind, unquote(token.text, true)};
            advance();
        } else if (kind == Kind::sequence || kind == Kind::choice) {
            std::vector<Node> children;
            do { children.push_back(ir_expression(depth + 1)); } while (accept(u","));
            result = group(kind, std::move(children));
        } else {
            result = wrap(kind, ir_expression(depth + 1), fn == u"oneOrMore" ? 1 : 0);
        }
        require(u")", "')'");
        return result;
    }

    Node choice(std::size_t depth) {
        if (depth >= 256) fail("a shallower expression");
        std::vector<Node> alternatives;
        do {
            std::vector<Node> terms;
            do {
                terms.push_back(term(depth));
            } while ((syntax == Syntax::ebnf && accept(u",")) || starts_term());
            alternatives.push_back(group(Kind::sequence, std::move(terms)));
        } while (accept(syntax == Syntax::ebnf ? u"|" : u"/"));
        return group(Kind::choice, std::move(alternatives));
    }

    bool starts_term() const {
        if (token.kind == TokenKind::identifier || token.kind == TokenKind::string ||
            token.kind == TokenKind::special || token.kind == TokenKind::number ||
            token.kind == TokenKind::repeat) return true;
        if (token.kind != TokenKind::symbol) return false;
        if (syntax == Syntax::peg) return token.text == u"(" || token.text == u"." ||
                                           token.text == u"&" || token.text == u"!";
        return token.text == u"(" || token.text == u"[" ||
            (syntax == Syntax::ebnf && token.text == u"{");
    }

    Node primary(std::size_t depth) {
        switch (token.kind) {
        case TokenKind::string: {
            Node node{Kind::terminal, unquote(token.text, syntax != Syntax::abnf)};
            advance();
            return node;
        }
        case TokenKind::identifier:
        case TokenKind::number:
        case TokenKind::special: {
            const auto kind = token.kind == TokenKind::identifier ? Kind::nonterminal :
                token.kind == TokenKind::number ? Kind::terminal : Kind::special;
            const auto text = kind == Kind::special ? trim(token.text.substr(1, token.text.size() - 2)) : token.text;
            advance();
            return {kind, std::u16string(text)};
        }
        default:
            break;
        }
        if (accept(u"(")) {
            auto node = choice(depth + 1);
            require(u")", "')'");
            return node;
        }
        if (syntax == Syntax::peg) {
            if (accept(u".")) return {Kind::special, u"."};
        } else {
            if (accept(u"[")) {
                auto node = choice(depth + 1);
                require(u"]", "']'");
                return wrap(Kind::optional, std::move(node));
            }
            if (syntax == Syntax::ebnf && accept(u"{")) {
                auto node = choice(depth + 1);
                require(u"}", "'}'");
                return wrap(Kind::repetition, std::move(node));
            }
        }
        fail("a grammar expression");
    }

    Node term(std::size_t depth) {
        if (syntax == Syntax::abnf) return abnf_term(depth);
        if (syntax == Syntax::peg) return peg_term(depth);
        auto node = primary(depth);
        for (std::size_t count = depth;; ++count) {
            if (token.kind != TokenKind::symbol ||
                View(u"?*+-").find(token.text) == View::npos) return node;
            if (count >= 256) fail("a shallower expression");
            if (accept(u"?")) node = wrap(Kind::optional, std::move(node));
            else if (accept(u"*")) node = wrap(Kind::repetition, std::move(node));
            else if (accept(u"+")) node = wrap(Kind::repetition, std::move(node), 1);
            else {
                advance();
                std::vector<Node> elements;
                elements.push_back(std::move(node));
                elements.push_back({Kind::terminal, u"-"});
                elements.push_back(primary(count));
                node = group(Kind::sequence, std::move(elements));
            }
        }
    }

    Node abnf_term(std::size_t depth) {
        if (token.kind != TokenKind::repeat) return primary(depth);
        const auto repeat = token.text;
        advance();
        auto node = primary(depth);
        const auto star = repeat.find(u'*');
        const auto min = decimal(repeat.substr(0, star));
        const auto max = star == View::npos ? min : star + 1 == repeat.size() ?
            std::numeric_limits<double>::infinity() : decimal(repeat.substr(star + 1));
        if (min == 0 && max == 1) return wrap(Kind::optional, std::move(node));
        return wrap(Kind::repetition, std::move(node), min, max);
    }

    Node peg_term(std::size_t depth) {
        View prefix;
        if (token.kind == TokenKind::symbol && (token.text == u"&" || token.text == u"!")) {
            prefix = token.text;
            advance();
        }
        auto node = primary(depth);
        if (accept(u"?")) node = wrap(Kind::optional, std::move(node));
        else if (accept(u"*")) node = wrap(Kind::repetition, std::move(node));
        else if (accept(u"+")) node = wrap(Kind::repetition, std::move(node), 1);
        if (prefix.empty()) return node;

        auto label = std::u16string(prefix);
        switch (node.kind) {
        case Kind::terminal: label += u"\"" + node.text + u"\""; break;
        case Kind::nonterminal:
        case Kind::special: label += node.text; break;
        default: label += u"(...)"; break;
        }
        return {Kind::special, std::move(label)};
    }
};

}

std::expected<RailroadDocument, std::string> parse_railroad(std::u16string_view source) {
    for (const auto& [header, syntax] : headers) {
        // Lexing handles hidden tokens before the header, including YAML and
        // comments. Only the selected grammar may recognize its header.
        Lexer probe(source, syntax);
        if (probe.next().kind != TokenKind::header) continue;
        try {
            return Parser(source, syntax).parse();
        } catch (const ParseError& error) {
            return std::unexpected(std::format("Railroad parse error at offset {}: expected {}",
                                               error.offset, error.expected));
        }
    }
    return std::unexpected("Expected a railroad-beta, railroad-ebnf-beta, railroad-abnf-beta or railroad-peg-beta header");
}

}
