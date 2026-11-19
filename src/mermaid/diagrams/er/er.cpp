// Mermaid 12.2.0 ER grammar and database projection.
module folia.mermaid.er;
import std;

namespace folia::mermaid {
namespace {
using Text = std::u16string;
using View = std::u16string_view;
bool space(char16_t c) { return c == u' ' || c == u'\t' || c == u'\r' || c == u'\n'; }
View trim(View s) {
    while (!s.empty() && space(s.front())) s.remove_prefix(1);
    while (!s.empty() && space(s.back())) s.remove_suffix(1);
    return s;
}
Text lower(View s) {
    Text out{s};
    for (auto& c : out) if (c >= u'A' && c <= u'Z') c += u'a' - u'A';
    return out;
}
bool name_character(char16_t c) {
    return c >= 128 || (c >= u'A' && c <= u'Z') || (c >= u'a' && c <= u'z') ||
        (c >= u'0' && c <= u'9') || c == u'_' || c == u'-' || c == u'*' || c == u'.';
}
std::vector<Text> split(View value, char16_t delimiter) {
    std::vector<Text> result;
    for (;;) {
        const auto end = value.find(delimiter);
        const auto item = trim(value.substr(0, end));
        if (!item.empty()) result.emplace_back(item);
        if (end == View::npos) return result;
        value.remove_prefix(end + 1);
    }
}

class Parser {
    ErInput result;
    ErLimits limits;
    View source;
    std::size_t position = 0, attribute_count = 0;
    std::map<Text, std::size_t, std::less<>> entity_indices, group_indices;
    std::map<Text, std::vector<Text>, std::less<>> class_definitions;
    struct GroupScope { ErGroup group; std::vector<Text> members; };
    std::vector<GroupScope> scopes;
    std::set<Text, std::less<>> assigned_members;

    [[noreturn]] void fail(const char* message) const {
        throw std::runtime_error(std::string{message} + " at prepared UTF-16 offset " +
            std::to_string(position));
    }
    bool at(View value) const { return source.substr(position).starts_with(value); }
    bool take(View value) {
        if (!at(value)) return false;
        position += value.size();
        return true;
    }
    void horizontal() {
        while (position < source.size() && source[position] != u'\n' && space(source[position])) ++position;
    }
    void whitespace() {
        while (position < source.size() && space(source[position])) ++position;
    }
    bool keyword(View value) {
        const auto rest = source.substr(position);
        if (rest.size() < value.size() || lower(rest.substr(0, value.size())) != value ||
            (rest.size() > value.size() && name_character(rest[value.size()]))) return false;
        position += value.size();
        horizontal();
        return true;
    }
    Text quoted(char16_t quote = u'"') {
        if (position == source.size() || source[position] != quote) fail("Expected quoted ER text");
        const auto start = ++position;
        const auto end = source.find(quote, start);
        if (end == View::npos) fail("Unclosed quoted ER text");
        Text out{source.substr(start, end - start)};
        position = end + 1;
        return out;
    }
    Text identifier() {
        horizontal();
        if (at(u"\"")) return quoted();
        const auto start = position;
        while (position < source.size() && name_character(source[position]) &&
            !at(u"--") && !at(u"..") && !at(u".-") && !at(u"-.")) ++position;
        if (start == position) fail("Expected an ER entity name");
        return Text{source.substr(start, position - start)};
    }
    Text line() {
        const auto start = position;
        const auto end = source.find(u'\n', start);
        position = end == View::npos ? source.size() : end;
        return Text{trim(source.substr(start, position - start))};
    }
    void end_line() {
        horizontal();
        if (position < source.size() && source[position] != u'\n') fail("Unexpected text after ER statement");
    }
    std::vector<Text> names() {
        std::vector<Text> values{identifier()};
        horizontal();
        while (take(u",")) {
            values.push_back(identifier());
            horizontal();
        }
        return values;
    }
    void membership(View name) {
        if (!scopes.empty()) scopes.back().members.emplace_back(name);
    }
    std::size_t entity(View name, View alias = {}) {
        if (auto it = entity_indices.find(name); it != entity_indices.end()) {
            auto& existing = result.entities[it->second];
            if (existing.alias.empty() && !alias.empty()) existing.alias = alias;
            return it->second;
        }
        if (result.entities.size() >= limits.entities) fail("ER entity limit exceeded");
        const auto index = result.entities.size();
        entity_indices.emplace(Text{name}, index);
        const auto n = std::to_string(index);
        ErEntity value;
        value.name = name;
        value.id = u"entity-" + Text{name} + u"-" + Text{n.begin(), n.end()};
        value.alias = alias;
        result.entities.push_back(std::move(value));
        return index;
    }
    void assign_classes(View name, const std::vector<Text>& classes) {
        if (auto it = entity_indices.find(name); it != entity_indices.end())
            for (const auto& value : classes) result.entities[it->second].classes += u" " + value;
        if (auto it = group_indices.find(name); it != group_indices.end()) {
            auto& values = result.groups[it->second].classes;
            values.insert(values.end(), classes.begin(), classes.end());
        }
    }
    std::vector<Text> inline_classes() {
        horizontal();
        return take(u":::") ? names() : std::vector<Text>{};
    }
    Text attribute_word(bool type) {
        whitespace();
        if (at(u"`")) {
            auto out = quoted(u'`');
            if (type && take(u"?")) out += u'?';
            return out;
        }
        const auto start = position;
        if (position == source.size()) fail("Expected an ER attribute");
        const auto first = source[position];
        if (!(first >= 128 || (first >= u'A' && first <= u'Z') ||
            (first >= u'a' && first <= u'z') || first == u'_' || first == u'*'))
            fail("Expected an ER attribute word");
        while (position < source.size()) {
            const auto c = source[position];
            if (!(name_character(c) || c == u'[' || c == u']' || c == u'(' ||
                c == u')' || c == u',' || c == u'~')) break;
            ++position;
        }
        if (type) take(u"?");
        return Text{source.substr(start, position - start)};
    }
    bool attribute_key() const {
        const auto rest = source.substr(position);
        const auto key = lower(rest.substr(0, 2));
        return rest.size() >= 2 && (key == u"pk" || key == u"fk" || key == u"uk") &&
            (rest.size() == 2 || !name_character(rest[2]));
    }
    void attributes(std::size_t index) {
        for (;;) {
            whitespace();
            if (take(u"}")) return;
            if (position == source.size()) fail("Unclosed ER attribute block");
            ErAttribute value;
            value.type = attribute_word(true);
            value.name = attribute_word(false);
            whitespace();
            if (attribute_key()) {
                for (;;) {
                    value.keys.emplace_back(source.substr(position, 2));
                    position += 2;
                    whitespace();
                    if (!take(u",")) break;
                    whitespace();
                    if (!attribute_key()) fail("Expected an ER attribute key after comma");
                }
            }
            if (at(u"\"")) value.comment = quoted();
            if (++attribute_count > limits.attributes) fail("ER attribute limit exceeded");
            result.entities[index].attributes.push_back(std::move(value));
        }
    }
    std::optional<Text> cardinality() {
        horizontal();
        for (const auto& [spelling, value] : {
            std::pair<View, View>{u"||", u"ONLY_ONE"}, {u"|o", u"ZERO_OR_ONE"},
            {u"o|", u"ZERO_OR_ONE"}, {u"}|", u"ONE_OR_MORE"}, {u"|{", u"ONE_OR_MORE"},
            {u"}o", u"ZERO_OR_MORE"}, {u"o{", u"ZERO_OR_MORE"},
            {u"one or zero", u"ZERO_OR_ONE"}, {u"zero or one", u"ZERO_OR_ONE"},
            {u"one or more", u"ONE_OR_MORE"}, {u"one or many", u"ONE_OR_MORE"},
            {u"zero or more", u"ZERO_OR_MORE"}, {u"zero or many", u"ZERO_OR_MORE"},
            {u"many(0)", u"ZERO_OR_MORE"}, {u"many(1)", u"ONE_OR_MORE"},
            {u"only one", u"ONLY_ONE"}, {u"many", u"ZERO_OR_MORE"},
            {u"one", u"ONLY_ONE"}, {u"1+", u"ONE_OR_MORE"},
            {u"0+", u"ZERO_OR_MORE"}, {u"1", u"ONLY_ONE"}}) {
            const bool word = spelling.front() >= u'a' && spelling.front() <= u'z' &&
                spelling.find_first_of(u"|{}") == View::npos;
            if (!word) {
                if (take(spelling)) return Text{value};
            } else {
                const auto rest=source.substr(position);
                if (rest.size()<spelling.size() || lower(rest.substr(0,spelling.size()))!=spelling) continue;
                const auto tail=rest.substr(spelling.size());
                if (!tail.empty() && name_character(tail.front()) && !tail.starts_with(u"--") &&
                    !tail.starts_with(u"..") && !tail.starts_with(u".-") && !tail.starts_with(u"-.")) continue;
                position+=spelling.size();
                return Text{value};
            }
        }
        if (at(u"u") && position + 1 < source.size() && View{u".-|"}.contains(source[position + 1])) {
            ++position;
            return Text{u"MD_PARENT"};
        }
        return {};
    }
    Text relationship_type() {
        horizontal();
        if (take(u"--") || keyword(u"to")) return u"IDENTIFYING";
        if (take(u"..") || take(u".-") || take(u"-.") || keyword(u"optionally to")) return u"NON_IDENTIFYING";
        fail("Expected ER relationship connector");
    }
    void declaration() {
        const auto name_a = identifier();
        horizontal();
        Text alias;
        if (take(u"[")) {
            alias = identifier();
            horizontal();
            if (!take(u"]")) fail("Unclosed ER entity alias");
        }
        const auto classes_a = inline_classes();
        horizontal();
        if (take(u"{")) {
            const auto index = entity(name_a, alias);
            attributes(index);
            assign_classes(name_a, classes_a);
            membership(name_a);
            return;
        }
        const auto card_a = cardinality();
        if (!card_a) {
            entity(name_a, alias);
            assign_classes(name_a, classes_a);
            membership(name_a);
            return;
        }
        if (!alias.empty()) fail("An ER relationship cannot declare an alias");
        const auto type = relationship_type();
        const auto card_b = cardinality();
        if (!card_b) fail("Expected ER target cardinality");
        const auto name_b = identifier();
        const auto classes_b = inline_classes();
        horizontal();
        if (!take(u":")) fail("An ER relationship requires a role");
        horizontal();
        // A role is display text, not an entity identifier. Keep the complete
        // unquoted line so the label layer receives TeX and mixed text intact.
        const auto label = at(u"\"") ? quoted() : line();
        end_line();
        const auto ia = entity(name_a), ib = entity(name_b);
        const auto endpoint = [&](View name, std::size_t index) {
            return group_indices.contains(name) ? Text{name} : result.entities[index].id;
        };
        if (result.relationships.size() >= limits.relationships) fail("ER relationship limit exceeded");
        result.relationships.push_back({endpoint(name_a, ia), label, endpoint(name_b, ib),
            type, *card_b, *card_a, label});
        assign_classes(name_a, classes_a);
        assign_classes(name_b, classes_b);
        membership(name_a);
        membership(name_b);
    }
    void open_group() {
        if (result.groups.size() + scopes.size() >= limits.groups) fail("ER subgraph limit exceeded");
        GroupScope scope;
        scope.group.id = identifier();
        scope.group.title = scope.group.id;
        horizontal();
        if (take(u"[")) {
            scope.group.title.clear();
            for (;;) {
                if (!scope.group.title.empty()) scope.group.title += u' ';
                scope.group.title += identifier();
                horizontal();
                if (take(u"]")) break;
                if (position == source.size() || source[position] == u'\n') fail("Unclosed ER subgraph title");
            }
        }
        end_line();
        scopes.push_back(std::move(scope));
    }
    void close_group() {
        if (scopes.empty()) fail("Unexpected ER subgraph end");
        auto scope = std::move(scopes.back());
        scopes.pop_back();
        std::set<Text, std::less<>> unique;
        for (const auto& name : scope.members) {
            if (unique.insert(name).second && assigned_members.insert(name).second)
                scope.group.members.push_back(name);
        }
        membership(scope.group.id);
        group_indices[scope.group.id] = result.groups.size();
        result.groups.push_back(std::move(scope.group));
    }
    void styles(bool definition) {
        const auto ids = names();
        auto text = line();
        if (text.ends_with(u';')) text.pop_back();
        const auto values = split(text, u',');
        if (values.empty()) fail("Expected ER style declarations");
        for (const auto& id : ids) {
            const auto append = [&](std::vector<Text>& into) { into.insert(into.end(), values.begin(), values.end()); };
            if (definition) append(class_definitions[id]);
            else {
                if (auto it = entity_indices.find(id); it != entity_indices.end()) append(result.entities[it->second].styles);
                if (auto it = group_indices.find(id); it != group_indices.end()) append(result.groups[it->second].styles);
            }
        }
    }
    void statement() {
        if (keyword(u"acctitle")) {
            if (!take(u":")) fail("Expected ER accessibility title colon");
            result.acc_title = line();
        } else if (keyword(u"accdescr")) {
            if (take(u":")) result.acc_description = line();
            else if (take(u"{")) {
                const auto end = source.find(u'}', position);
                if (end == View::npos) fail("Unclosed ER accessibility description");
                result.acc_description = trim(source.substr(position, end - position));
                position = end + 1;
            } else fail("Expected ER accessibility description");
        } else if (keyword(u"direction")) {
            const auto direction = identifier();
            const auto folded = lower(direction);
            if (folded != u"tb" && folded != u"bt" && folded != u"lr" && folded != u"rl") fail("Invalid ER direction");
            Text normalized = folded;
            for (auto& c : normalized) c -= u'a' - u'A';
            if (scopes.empty()) result.direction = normalized;
            else scopes.back().group.direction = normalized;
            end_line();
        } else if (keyword(u"subgraph")) open_group();
        else if (keyword(u"end")) close_group();
        else if (keyword(u"classdef")) styles(true);
        else if (keyword(u"style")) styles(false);
        else if (keyword(u"class")) {
            const auto ids = names(), classes = names();
            for (const auto& id : ids) assign_classes(id, classes);
            end_line();
        } else declaration();
    }
    void finish() {
        if (!scopes.empty()) fail("Unclosed ER subgraph");
        for (const auto& group : result.groups) for (const auto& name : group.members) {
            if (auto it = group_indices.find(name); it != group_indices.end()) result.groups[it->second].parent = group.id;
            else if (auto it = entity_indices.find(name); it != entity_indices.end()) result.entities[it->second].parent = group.id;
        }
        const auto compile = [&](const std::vector<Text>& classes) {
            std::vector<Text> values;
            for (const auto& name : classes) if (auto it = class_definitions.find(name); it != class_definitions.end()) {
                for (const auto& style : it->second) values.push_back(style);
                // ErDB exposes color declarations in both style and textStyle lists.
                for (const auto& style : it->second) if (style.find(u"color") != Text::npos) values.push_back(style);
            }
            return values;
        };
        for (auto& node : result.entities) node.compiled_styles = compile(split(node.classes, u' '));
        for (auto& group : result.groups) group.compiled_styles = compile(group.classes);
    }
public:
    explicit Parser(ErLimits value) : limits(std::move(value)) {}
    ErInput run(PreparedDiagram& prepared) {
        result.title = prepared.title.value_or(Text{});
        const auto encoded = encode_diagram_source(prepared.code.cleaned);
        source = encoded;
        whitespace();
        if (!keyword(u"erdiagram")) fail("Expected ER diagram header");
        while (true) {
            whitespace();
            if (position == source.size()) break;
            if (at(u"%%")) { line(); continue; }
            statement();
        }
        finish();
        result.effective_config = std::move(prepared.effective_config);
        return std::move(result);
    }
};
}

std::expected<ErInput, std::string> prepare_er(
    std::u16string_view source, const ConfigSession& session, ErLimits limits) {
    auto prepared = prepare_diagram(source, session, limits.frontend);
    if (!prepared) return std::unexpected(prepared.error());
    if (prepared->type != u"er") return std::unexpected("Expected an ER diagram");
    try { return Parser{std::move(limits)}.run(*prepared); }
    catch (const std::exception& error) { return std::unexpected(std::string{error.what()}); }
}
}
