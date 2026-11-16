// Derived from @zenuml/core 3.49.2. MIT; see ../ZENUML-LICENSE.
module folia.mermaid.zenuml;
import std;

namespace folia::mermaid {
namespace {
using View = std::u16string_view;

bool space(char16_t c) {
    return c == u' ' || c == u'\t' || c == u'\r' || c == u'\n';
}
std::u16string trim(View value) {
    while (!value.empty() && space(value.front()))
        value.remove_prefix(1);
    while (!value.empty() && space(value.back()))
        value.remove_suffix(1);
    return std::u16string(value);
}
std::u16string compact(View value) {
    std::u16string result;
    char16_t quote{};
    bool escaped{};
    for (const auto character : value) {
        if (quote) {
            result += character;
            if (escaped)
                escaped = false;
            else if (character == u'\\')
                escaped = true;
            else if (character == quote)
                quote = 0;
        } else if (character == u'"' || character == u'\'') {
            quote = character;
            result += character;
        } else if (!space(character))
            result += character;
    }
    return result;
}
bool keyword(View value, View word) {
    return value.starts_with(word) && (value.size() == word.size() || space(value[word.size()]) ||
                                       value[word.size()] == u'(' || value[word.size()] == u'{');
}
std::u16string unquote(View value) {
    if (value.size() > 1 && (value.front() == u'"' || value.front() == u'\'') &&
        value.back() == value.front())
        value = value.substr(1, value.size() - 2);
    return std::u16string(value);
}

class Parser {
  public:
    explicit Parser(View input) : source(input) {
        if (source.starts_with(u"zenuml"))
            source.remove_prefix(6);
    }

    ZenDocument parse() {
        for (;;) {
            separators();
            comments();
            const auto end = source.find(u'\n', position);
            const auto line =
                trim(source.substr(position, end == source.npos ? source.size() - position : end - position));
            if (keyword(line, u"title")) {
                document.title = trim(View(line).substr(5));
            } else if (!declaration(line))
                break;
            pending_comment.clear();
            position = end == source.npos ? source.size() : end + 1;
        }
        document.statements = block(u"_STARTER_", u"_STARTER_", false);
        if (starter || document.participants.empty()) {
            document.participants.insert(document.participants.begin(), {u"_STARTER_", {}, {}});
        }
        return std::move(document);
    }

  private:
    View source;
    std::size_t position{};
    ZenDocument document;
    bool starter{};
    std::u16string pending_comment;

    void separators() {
        while (position < source.size() && (space(source[position]) || source[position] == u';'))
            ++position;
    }
    void comments() {
        while (source.substr(position).starts_with(u"//")) {
            const auto end = source.find(u'\n', position);
            const auto length = end == source.npos ? source.size() - position - 2 : end - position - 2;
            if (!pending_comment.empty())
                pending_comment += u'\n';
            pending_comment += source.substr(position + 2, length);
            position = end == source.npos ? source.size() : end + 1;
            separators();
        }
    }
    ZenParticipant *participant(std::u16string name) {
        name = unquote(name);
        if (name == u"_STARTER_") {
            starter = true;
            return nullptr;
        }
        if (name.empty())
            return nullptr;
        const auto found = std::ranges::find(document.participants, name, &ZenParticipant::name);
        if (found != document.participants.end())
            return &*found;
        document.participants.push_back({std::move(name), {}, {}});
        return &document.participants.back();
    }
    bool declaration(View line) {
        if (line.empty() || line.starts_with(u"@return") || line.starts_with(u"@reply"))
            return false;
        std::u16string type;
        if (line.front() == u'@') {
            const auto end = line.find_first_of(u" \t");
            if (end == line.npos)
                return false;
            type = line.substr(1, end - 1);
            line.remove_prefix(end + 1);
        }
        const auto alias = line.find(u" as ");
        auto name = trim(line.substr(0, alias));
        if (name.find_first_of(u".({}:;=->") != name.npos || name.empty())
            return false;
        for (const auto word :
             {u"if", u"else", u"while", u"for", u"forEach", u"foreach", u"loop", u"opt", u"par", u"critical",
              u"section", u"try", u"catch", u"finally", u"new", u"return"}) {
            if (keyword(name, word))
                return false;
        }
        if (name.find_first_of(u" \t") != name.npos && name.front() != u'"' && name.front() != u'\'')
            return false;
        auto *value = participant(std::move(name));
        if (!value)
            return true;
        value->type = std::move(type);
        if (alias != line.npos)
            value->label = unquote(trim(line.substr(alias + 4)));
        return true;
    }
    std::u16string head() {
        const auto begin = position;
        int parentheses{};
        bool asynchronous_content{};
        char16_t quote{};
        bool escaped{};
        while (position < source.size()) {
            const auto character = source[position];
            if (quote) {
                if (escaped)
                    escaped = false;
                else if (character == u'\\')
                    escaped = true;
                else if (character == quote)
                    quote = 0;
            } else if (character == u'"' || character == u'\'')
                quote = character;
            else {
                if (character == u':' && !parentheses &&
                    source.substr(begin, position - begin).find(u"->") != View::npos)
                    asynchronous_content = true;
                if (!asynchronous_content) {
                    if (character == u'(')
                        ++parentheses;
                    else if (character == u')')
                        --parentheses;
                }
                if (!parentheses &&
                    (character == u'{' || character == u'}' || character == u'\n' || character == u';'))
                    break;
            }
            ++position;
        }
        if (quote || parentheses)
            throw std::runtime_error("Unclosed ZenUML expression");
        return trim(source.substr(begin, position - begin));
    }
    std::u16string condition(View text, View command, bool required = false) {
        auto tail = trim(text.substr(command.size()));
        if (tail.empty() && !required)
            return {};
        if (tail.size() < 2 || tail.front() != u'(' || tail.back() != u')') {
            throw std::runtime_error("Expected ZenUML condition in parentheses");
        }
        return trim(View(tail).substr(1, tail.size() - 2));
    }
    std::vector<ZenStatement> body(const std::u16string &owner, const std::u16string &caller) {
        separators();
        if (position == source.size() || source[position] != u'{') {
            throw std::runtime_error("Expected ZenUML block");
        }
        ++position;
        return block(owner, caller, true);
    }
    void fragment_sections(ZenStatement &statement, const std::u16string &owner,
                           const std::u16string &caller) {
        statement.body = true;
        statement.sections.push_back({{}, body(owner, caller)});
        if (statement.kind == ZenKind::alternative) {
            for (;;) {
                separators();
                if (!keyword(source.substr(position), u"else"))
                    break;
                position += 4;
                separators();
                std::u16string label = u"[else]";
                bool final = true;
                if (keyword(source.substr(position), u"if")) {
                    label = u"[" + condition(head(), u"if", true) + u"]";
                    final = false;
                }
                statement.sections.push_back({std::move(label), body(owner, caller)});
                if (final)
                    break;
            }
        } else if (statement.kind == ZenKind::try_catch) {
            for (;;) {
                separators();
                if (keyword(source.substr(position), u"catch")) {
                    const auto exception = condition(head(), u"catch");
                    statement.sections.push_back(
                        {exception.empty() ? u"catch" : u"catch " + exception, body(owner, caller)});
                } else if (keyword(source.substr(position), u"finally")) {
                    position += 7;
                    statement.sections.push_back({u"finally", body(owner, caller)});
                    break;
                } else
                    break;
            }
        }
    }
    void message(ZenStatement &statement, std::u16string text, bool annotated_return) {
        auto arrow = text.find(u"->");
        bool return_arrow{};
        if (arrow != text.npos) {
            return_arrow = arrow && text[arrow - 1] == u'-';
            statement.from = trim(View(text).substr(0, arrow - (return_arrow ? 1 : 0)));
            text = trim(View(text).substr(arrow + 2));
            if (const auto colon = text.find(u':');
                (colon != text.npos && colon < text.find(u'(')) || text.find(u'.') == text.npos) {
                statement.kind = annotated_return || return_arrow ? ZenKind::returned : ZenKind::async;
                statement.to = trim(View(text).substr(0, colon));
                if (colon != text.npos) {
                    statement.text = text.substr(colon + 1);
                    statement.label = trim(statement.text);
                    for (std::size_t i = 0; i + 1 < statement.text.size(); ++i) {
                        if (statement.text[i] == u',') {
                            while (i + 1 < statement.text.size() && space(statement.text[i + 1]))
                                statement.text.erase(i + 1, 1);
                        }
                    }
                }
                participant(statement.from);
                participant(statement.to);
                return;
            }
        }
        if (const auto assignment = text.find(u'=');
            assignment != text.npos && assignment < text.find(u'(')) {
            statement.assignment = trim(View(text).substr(0, assignment));
            if (const auto type = statement.assignment.find_last_of(u" \t");
                type != statement.assignment.npos) {
                statement.assignment.erase(0, type + 1);
            }
            text = trim(View(text).substr(assignment + 1));
        }
        if (keyword(text, u"new")) {
            statement.kind = ZenKind::creation;
            text = trim(View(text).substr(3));
            const auto parenthesis = text.find(u'(');
            statement.to = trim(View(text).substr(0, parenthesis));
            const auto parameters =
                parenthesis == text.npos
                    ? std::u16string{}
                    : compact(View(text).substr(parenthesis + 1, text.size() - parenthesis - 2));
            statement.text = u"\u00ab" + (parameters.empty() ? u"create" : parameters) + u"\u00bb";
        } else {
            statement.kind = ZenKind::sync;
            const auto parenthesis = text.find(u'(');
            const auto dot = text.find(u'.');
            if (dot != text.npos && dot < parenthesis) {
                statement.to = trim(View(text).substr(0, dot));
                statement.text = compact(View(text).substr(dot + 1));
            } else if (parenthesis != text.npos && text.back() == u')') {
                statement.text = compact(text);
            } else
                throw std::runtime_error("Unsupported ZenUML statement");
        }
        participant(statement.from);
        participant(statement.to);
    }
    std::vector<ZenStatement> block(const std::u16string &owner, const std::u16string &caller, bool nested) {
        std::vector<ZenStatement> statements;
        for (;;) {
            separators();
            comments();
            if (position == source.size()) {
                if (nested)
                    throw std::runtime_error("Unclosed ZenUML block");
                return statements;
            }
            if (source[position] == u'}') {
                if (!nested)
                    throw std::runtime_error("Unexpected ZenUML closing brace");
                ++position;
                return statements;
            }
            ZenStatement statement;
            statement.begin = position;
            statement.from = statement.to = owner;
            statement.comment = trim(std::exchange(pending_comment, {}));
            bool annotated_return{};
            if (keyword(source.substr(position), u"@return") || keyword(source.substr(position), u"@reply")) {
                position += source.substr(position).starts_with(u"@return") ? 7 : 6;
                separators();
                annotated_return = true;
            }
            const auto text = head();
            if (text.empty())
                throw std::runtime_error("Empty ZenUML statement");
            std::optional<std::pair<View, ZenKind>> fragment;
            for (const auto entry : {std::pair<View, ZenKind>{u"if", ZenKind::alternative},
                                     {u"while", ZenKind::loop},
                                     {u"for", ZenKind::loop},
                                     {u"forEach", ZenKind::loop},
                                     {u"foreach", ZenKind::loop},
                                     {u"loop", ZenKind::loop},
                                     {u"opt", ZenKind::optional},
                                     {u"par", ZenKind::parallel},
                                     {u"critical", ZenKind::critical},
                                     {u"section", ZenKind::section},
                                     {u"try", ZenKind::try_catch}}) {
                if (keyword(text, entry.first)) {
                    fragment = entry;
                    break;
                }
            }
            if (fragment) {
                statement.kind = fragment->second;
                statement.condition =
                    condition(text, fragment->first, statement.kind == ZenKind::alternative);
                if (statement.kind == ZenKind::section)
                    statement.condition.clear();
                fragment_sections(statement, owner, caller);
            } else if (keyword(text, u"return")) {
                statement.kind = ZenKind::returned;
                statement.text = trim(View(text).substr(6));
                statement.to = caller;
            } else {
                message(statement, text, annotated_return);
                separators();
                if (position < source.size() && source[position] == u'{') {
                    statement.body = true;
                    statement.children = body(statement.to, statement.from);
                }
            }
            statement.end = position ? position - 1 : 0;
            statements.push_back(std::move(statement));
        }
    }
};

struct Dual {
    double p{}, v{};
};
std::vector<double> optimal(const std::vector<std::vector<double>> &m) {
    const auto n = m.size();
    std::vector<Dual> gaps(n ? n - 1 : 0, {0, 1});
    for (;;) {
        double delta = std::numeric_limits<double>::infinity();
        std::vector<Dual> values;
        std::vector<int> args;
        Dual maximum{};
        for (std::size_t j = 0; j < n; ++j) {
            int arg = -1;
            if (j) {
                maximum.p += gaps[j - 1].p;
                maximum.v += gaps[j - 1].v;
            }
            for (std::size_t i = 0; i < j; ++i)
                if (m[i][j] > 0) {
                    Dual x{values[i].p + m[i][j], values[i].v};
                    auto a = maximum, b = x;
                    double d = a.p - b.p;
                    bool less =
                        d < -std::sqrt(std::numeric_limits<double>::epsilon()) ||
                        (std::abs(d) <= std::sqrt(std::numeric_limits<double>::epsilon()) && a.v < b.v);
                    if (less)
                        std::swap(a, b);
                    if (a.v < b.v)
                        delta = std::min(delta, (a.p - b.p) / (b.v - a.v));
                    if (less) {
                        arg = static_cast<int>(i);
                        maximum = x;
                    }
                }
            values.push_back(maximum);
            args.push_back(arg);
        }
        if (!std::isfinite(delta)) {
            std::vector<double> result;
            for (auto v : values)
                result.push_back(v.p);
            return result;
        }
        if (values.back().v > 0) {
            std::size_t j = n - 1;
            while (j) {
                if (args[j] >= 0)
                    j = static_cast<std::size_t>(args[j]);
                else
                    gaps[--j].v = 0;
            }
        } else
            for (auto &g : gaps)
                g.p += g.v * delta;
    }
}

// The upstream vertical algorithm uses CSS line heights and structural margins;
// it is independent of platform font widths and can be verified without a DOM.
double comment_height(const ZenStatement &statement) {
    return statement.comment.empty() ? 0 : (1 + std::ranges::count(statement.comment, u'\n')) * 20;
}
double vertical(const std::vector<ZenStatement> &statements, double top,
                std::vector<ZenCoordinate> &coordinates, bool occurrence, bool parallel = false) {
    if (statements.empty())
        return top;
    double cursor = top + 16;
    for (std::size_t i = 0; i < statements.size(); ++i) {
        if (parallel && i)
            ++cursor;
        const auto &statement = statements[i];
        const auto start = cursor;
        cursor += comment_height(statement);
        switch (statement.kind) {
        case ZenKind::async:
            cursor += statement.from == statement.to ? 44 : 16;
            break;
        case ZenKind::returned:
            cursor +=
                statement.from == statement.to ? 20 : ((i + 1 < statements.size() || !occurrence) ? 16 : 0);
            break;
        case ZenKind::sync:
        case ZenKind::creation:
            cursor += statement.kind == ZenKind::creation ? 40 : (statement.from == statement.to ? 30 : 16);
            if (statement.body)
                cursor = vertical(statement.children, cursor, coordinates, true) + 2;
            else
                cursor += 22;
            if (!statement.assignment.empty() &&
                (statement.kind == ZenKind::creation || statement.from != statement.to))
                cursor += 12;
            break;
        default:
            cursor += 26;
            if (!statement.condition.empty() || statement.kind == ZenKind::alternative)
                cursor += 20;
            for (std::size_t section = 0; section < statement.sections.size(); ++section) {
                if (section)
                    cursor += 29;
                cursor = vertical(statement.sections[section].statements, cursor, coordinates, occurrence,
                                  statement.kind == ZenKind::parallel);
            }
            cursor += 11;
            break;
        }
        coordinates.push_back({statement.begin, statement.end, statement.kind, start, cursor - start});
        cursor += 16;
    }
    return cursor;
}
} // namespace

std::expected<ZenDocument, std::string> parse_zenuml(std::u16string_view source) {
    try {
        return Parser(source).parse();
    } catch (const std::runtime_error &error) {
        return std::unexpected(error.what());
    }
}

ZenVerticalLayout layout_zenuml_vertical(const ZenDocument &document) {
    ZenVerticalLayout result;
    result.height = vertical(document.statements, 56, result.coordinates, false);
    return result;
}

std::expected<ZenLayout, std::string> layout_zenuml(const ZenDocument &document, const ZenMeasure &measure) {
    if (!measure)
        return std::unexpected("ZenUML requires real text measurements");
    ZenLayout result;
    const auto count = document.participants.size();
    if (!count)
        return result;
    for (const auto &participant : document.participants) {
        const auto width = measure(trim(participant.label.empty() ? participant.name : participant.label));
        if (!std::isfinite(width) || width < 0)
            return std::unexpected("Invalid ZenUML text measurement");
        result.widths.push_back(std::max(std::floor(width + .5) + (participant.type.empty() ? 0 : 40), 80.0) +
                                20);
    }
    result.constraints.assign(count, std::vector<double>(count));
    for (std::size_t i = 1; i < count; ++i)
        result.constraints[i - 1][i] = (result.widths[i - 1] + result.widths[i]) / 2;
    const auto index = [&](View name) {
        const auto found = std::ranges::find(document.participants, name, &ZenParticipant::name);
        return static_cast<std::size_t>(found - document.participants.begin());
    };
    std::function<void(const std::vector<ZenStatement> &)> walk = [&](const auto &statements) {
        for (const auto &statement : statements) {
            if (!zen_fragment(statement.kind)) {
                const auto from = index(statement.from);
                // RetContext.Owner falls back to the enclosing receiver only when
                // ReturnTo has no explicit starter. Nested and annotated returns
                // otherwise constrain their actual destination participant.
                const auto to = index(statement.kind == ZenKind::returned && statement.to == u"_STARTER_"
                                          ? statement.from : statement.to);
                if (from < count && to < count) {
                    auto text = statement.text;
                    if (statement.from == statement.to && !statement.assignment.empty())
                        text = statement.assignment + u" = " + text;
                    auto width = std::floor(measure(trim(text)) + .5) + 25;
                    if (statement.kind == ZenKind::creation)
                        width += result.widths[to] / 2;
                    auto &constraint = result.constraints[std::min(from, to)][std::max(from, to)];
                    constraint = std::max(constraint, width);
                }
            }
            walk(statement.children);
            for (const auto &section : statement.sections)
                walk(section.statements);
        }
    };
    walk(document.statements);
    result.positions = optimal(result.constraints);
    for (auto &position : result.positions)
        position += result.widths.front() / 2;
    result.width = std::max(200.0, result.positions.back() + result.widths.back() / 2);
    auto vertical_layout = layout_zenuml_vertical(document);
    result.height = vertical_layout.height;
    result.vertical = std::move(vertical_layout.coordinates);
    return result;
}
} // namespace folia::mermaid
