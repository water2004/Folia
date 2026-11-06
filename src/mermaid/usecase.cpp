// Mermaid 12.2.0 Usecase grammar and model projection. MIT; see THIRD_PARTY_NOTICES.md.
module folia.mermaid.usecase;
import std;

namespace folia::mermaid {
namespace {

using Text = std::u16string;
using View = std::u16string_view;

bool white(char16_t ch) { return ch == u' ' || ch == u'\t' || ch == u'\r' || ch == u'\n'; }
bool word(char16_t ch) {
    return ch == u'_' || (ch >= u'a' && ch <= u'z') || (ch >= u'A' && ch <= u'Z') ||
        (ch >= u'0' && ch <= u'9');
}
View trim(View value) {
    while (!value.empty() && white(value.front())) value.remove_prefix(1);
    while (!value.empty() && white(value.back())) value.remove_suffix(1);
    return value;
}

[[noreturn]] void fail(const char* message) { throw std::runtime_error(message); }

bool consume(View& input, View prefix) {
    if (!input.starts_with(prefix)) return false;
    input = trim(input.substr(prefix.size()));
    return true;
}

bool keyword(View& input, View value) {
    if (!input.starts_with(value) ||
        (input.size() > value.size() && word(input[value.size()]))) return false;
    input = trim(input.substr(value.size()));
    return true;
}

View take_id(View& input) {
    const auto end = std::ranges::find_if_not(input, word);
    const auto length = static_cast<std::size_t>(end - input.begin());
    if (!length) fail("Expected Use Case identifier");
    const auto id = input.substr(0, length);
    input = trim(input.substr(length));
    return id;
}

struct Label { Text text; bool markdown = false; };

bool is_quote(View input) { return input.starts_with(u'"') || input.starts_with(u'\''); }

Label take_quoted(View& input) {
    const bool markdown = input.starts_with(u"\"`");
    const auto end = markdown ? input.find(u"`\"", 2) : input.find(input.front(), 1);
    if (end == View::npos || (!markdown && input.substr(1, end - 1).find(u'\n') != View::npos))
        fail("Unclosed Use Case label");
    Label out{Text(input.substr(markdown ? 2 : 1, end - (markdown ? 2 : 1))), markdown};
    input = trim(input.substr(end + (markdown ? 2 : 1)));
    return out;
}

Label take_label(View& input, char16_t terminator = 0) {
    if (is_quote(input)) return take_quoted(input);
    const auto end = terminator ? input.find(terminator) : input.size();
    if (end == View::npos) fail("Unclosed Use Case label");
    const auto text = trim(input.substr(0, end));
    if (text.empty() || text.find_first_of(u"()[]{}\"'\r\n") != View::npos ||
        text.find(u"--") != View::npos || text.find(u"..>") != View::npos ||
        text.find(u":::") != View::npos || text.find(u"<<") != View::npos)
        fail("Invalid unquoted Use Case label");
    input = trim(input.substr(end));
    return {Text(text), false};
}

Text generated_id(View label) {
    Text id(label);
    for (auto& ch : id)
        if (!word(ch)) ch = u'_';
    return id;
}

using Properties = std::vector<std::pair<Text, Text>>;
Properties take_properties(View& input) {
    if (!consume(input, u"@{")) return {};
    Properties result;
    while (!input.empty() && !consume(input, u"}")) {
        Text key = is_quote(input) ? take_quoted(input).text : Text(take_id(input));
        if (!consume(input, u":")) fail("Expected Use Case metadata value");
        Text value = is_quote(input) ? take_quoted(input).text : Text(take_id(input));
        result.emplace_back(std::move(key), std::move(value));
        consume(input, u",");
    }
    return result;
}

void apply_property(UsecaseNode& node, View key, View value) {
    if (node.kind == UsecaseNodeKind::boundary && key == u"type") {
        if (value == u"package") node.shape = UsecaseShape::package;
        else if (value == u"rect") node.shape = UsecaseShape::rectangle;
        else fail("Invalid Use Case boundary type");
        return;
    }
    if (node.kind == UsecaseNodeKind::actor) {
        if (key == u"type") {
            if (value == u"normal") node.actor = UsecaseActor::normal;
            else if (value == u"hollow") node.actor = UsecaseActor::hollow;
            else if (value == u"awesome") node.actor = UsecaseActor::awesome;
            else fail("Invalid Use Case actor type");
            return;
        }
        if (key == u"icon") {
            node.icon = value;
            return;
        }
    }
    if ((node.kind == UsecaseNodeKind::actor || node.kind == UsecaseNodeKind::usecase) &&
        key == u"business" && (value == u"true" || value == u"false")) {
        node.business = value == u"true";
        return;
    }
    fail("Invalid Use Case node metadata");
}

void validate_node(UsecaseNode& node) {
    if (!node.icon.empty()) {
        if (node.actor != UsecaseActor::normal || node.business)
            fail("Use Case icon actor cannot combine icon with type or business");
        node.actor = UsecaseActor::icon;
    }
    if (node.business && ((node.kind == UsecaseNodeKind::actor && node.actor == UsecaseActor::awesome) ||
                         (node.kind == UsecaseNodeKind::usecase && node.shape != UsecaseShape::ellipse)))
        fail("Invalid Use Case business shape");
}

std::vector<Text> take_ids(View& input) {
    std::vector<Text> result;
    do result.emplace_back(take_id(input)); while (consume(input, u","));
    return result;
}

struct Endpoint {
    UsecaseNode node;
    bool explicit_declaration = false;
    Properties metadata;
};

Endpoint endpoint(View& input, UsecaseNodeKind kind) {
    Endpoint result;
    auto& node = result.node;
    node.kind = kind;
    if (kind == UsecaseNodeKind::boundary) node.shape = UsecaseShape::rectangle;
    result.explicit_declaration = kind != UsecaseNodeKind::usecase;
    if (is_quote(input)) {
        auto label = take_quoted(input);
        node.id = generated_id(label.text);
        node.label = std::move(label.text);
        node.markdown = label.markdown;
    } else {
        node.id = take_id(input);
        node.label = node.id;
        if (input.starts_with(u'(') || (kind != UsecaseNodeKind::actor && input.starts_with(u'['))) {
            const auto terminator = input.front() == u'(' ? u')' : u']';
            input = trim(input.substr(1));
            auto label = take_label(input, terminator);
            if (kind != UsecaseNodeKind::boundary && terminator == u']') node.shape = UsecaseShape::rectangle;
            if (!consume(input, View(&terminator, 1))) fail("Unclosed Use Case label");
            node.label = std::move(label.text);
            node.markdown = label.markdown;
            result.explicit_declaration = true;
        }
    }
    result.metadata = take_properties(input);
    if (kind != UsecaseNodeKind::boundary && consume(input, u"<<")) {
        const auto end = input.find(u">>");
        if (end == View::npos || trim(input.substr(0, end)).empty()) fail("Unclosed Use Case stereotype");
        node.stereotype = trim(input.substr(0, end));
        input = trim(input.substr(end + 2));
        result.explicit_declaration = true;
    }
    if (consume(input, u":::")) node.classes = take_ids(input);
    if (node.id.empty()) fail("Empty Use Case id");
    return result;
}

struct Operator {
    UsecaseRelationship kind = UsecaseRelationship::association;
    UsecaseArrow marker = UsecaseArrow::none;
    bool reverse = false;
    std::size_t length = 0, min_length = 1;
};

Operator read_operator(View input) {
    if (input.starts_with(u"--|>")) return {UsecaseRelationship::generalization, UsecaseArrow::point, false, 4};
    if (input.starts_with(u"..>")) return {UsecaseRelationship::include, UsecaseArrow::point, false, 3};
    if (input.starts_with(u"--o")) return {UsecaseRelationship::association, UsecaseArrow::circle, false, 3};
    if (input.starts_with(u"--x")) return {UsecaseRelationship::association, UsecaseArrow::cross, false, 3};
    if (input.starts_with(u"o--")) return {UsecaseRelationship::association, UsecaseArrow::circle, true, 3};
    if (input.starts_with(u"x--")) return {UsecaseRelationship::association, UsecaseArrow::cross, true, 3};
    const bool reverse = input.starts_with(u'<');
    if (reverse) input.remove_prefix(1);
    std::size_t dashes = 0;
    while (dashes < input.size() && input[dashes] == u'-') ++dashes;
    if (dashes < 2) return {};
    const bool forward = !reverse && input.substr(dashes).starts_with(u'>');
    return {UsecaseRelationship::association, reverse || forward ? UsecaseArrow::point : UsecaseArrow::none,
        reverse, dashes + (reverse || forward), dashes - 1};
}

std::pair<Operator, Label> take_operator(View& input) {
    auto op = read_operator(input);
    if (!op.length) fail("Expected Use Case relationship operator");
    input = trim(input.substr(op.length));
    if (op.kind == UsecaseRelationship::include) {
        if (!consume(input, u":")) fail("Use Case dependency requires include or extend");
        Text type(take_id(input));
        for (auto& ch : type) if (ch >= u'A' && ch <= u'Z') ch += 32;
        if (type == u"extend") op.kind = UsecaseRelationship::extend;
        else if (type != u"include") fail("Use Case dependency requires include or extend");
        return {op, {}};
    }
    if (op.kind != UsecaseRelationship::association || op.min_length != 1 ||
        (!op.reverse && op.marker != UsecaseArrow::none)) return {op, {}};
    if (is_quote(input)) {
        auto rest = input;
        auto label = take_quoted(rest);
        const auto right = read_operator(rest);
        if (!right.length || right.kind != UsecaseRelationship::association || right.reverse ||
            (op.reverse && right.marker != UsecaseArrow::none)) return {op, {}};
        input = trim(rest.substr(right.length));
        return {op.reverse ? op : right, std::move(label)};
    }
    const auto split = input.find(u"--");
    if (split == View::npos) return {op, {}};
    auto label_text = trim(input.substr(0, split));
    if (label_text.empty() || label_text.ends_with(u'<')) return {op, {}};
    const auto right = read_operator(input.substr(split));
    if (!right.length || right.kind != UsecaseRelationship::association ||
        (op.reverse && right.marker != UsecaseArrow::none)) return {op, {}};
    auto label = take_label(label_text);
    input = trim(input.substr(split + right.length));
    auto result = op.reverse ? op : right;
    if (result.marker == UsecaseArrow::point || result.marker == UsecaseArrow::none)
        result.min_length = right.min_length;
    return {result, std::move(label)};
}

std::vector<Text> styles(View input) {
    std::vector<Text> out;
    while (!input.empty()) {
        auto end = input.find(u',');
        while (end != View::npos && end != 0 && input[end - 1] == u'\\')
            end = input.find(u',', end + 1);
        const auto item = trim(input.substr(0, end));
        if (item.find(u':') == View::npos) fail("Invalid Use Case style");
        out.emplace_back(item);
        if (end == View::npos) break;
        input = trim(input.substr(end + 1));
    }
    return out;
}

struct Parser {
    UsecaseDocument document;
    std::map<Text, std::size_t, std::less<>> node_index, edge_index;
    std::vector<bool> declared;
    Text boundary;
    std::vector<std::pair<Text, Properties>> pending_metadata;
    struct Appearance { std::vector<Text> ids, classes, styles; };
    std::vector<Appearance> pending_appearance;
    struct Note { Text target; Label label; };
    std::vector<Note> notes;
    std::size_t anonymous = 0;

    void remember(Endpoint endpoint, bool declaration) {
        auto& node = endpoint.node;
        const auto [found, inserted] = node_index.try_emplace(node.id, document.nodes.size());
        if (declaration) node.parent = boundary;
        if (declaration)
            for (const auto& [key, value] : endpoint.metadata) apply_property(node, key, value);
        if (inserted) {
            document.nodes.push_back(std::move(node));
            declared.push_back(declaration);
            return;
        }
        if (!declaration) return;
        const auto index = found->second;
        auto& previous = document.nodes[index];
        if (!declared[index]) {
            previous = std::move(node);
            declared[index] = true;
            return;
        }
        if (previous.kind != node.kind || previous.parent != node.parent ||
            node.kind == UsecaseNodeKind::boundary)
            fail("Conflicting Use Case declaration");
        if (node.label != node.id && previous.label != previous.id && previous.label != node.label)
            fail("Conflicting Use Case labels");
        if (node.label != node.id) {
            previous.label = std::move(node.label);
            previous.markdown = node.markdown;
            previous.shape = node.shape;
        }
        if (!node.stereotype.empty()) previous.stereotype = std::move(node.stereotype);
        for (const auto& [key, value] : endpoint.metadata) apply_property(previous, key, value);
        previous.classes.insert(previous.classes.end(), node.classes.begin(), node.classes.end());
    }

    void relationship(Endpoint from, View tail) {
        if (!boundary.empty()) fail("Use Case relationships belong outside system boundaries");
        Text id;
        if (!tail.empty() && word(tail.front()) && !read_operator(tail).length) {
            auto rest = tail;
            const auto explicit_id = take_id(rest);
            if (consume(rest, u"@")) {
                id = explicit_id;
                tail = rest;
            }
        }
        auto [op, label] = take_operator(tail);
        auto to = endpoint(tail, UsecaseNodeKind::usecase);
        if (!tail.empty()) fail("Unexpected text after Use Case relationship");
        if (id.empty()) {
            const auto sequence = std::to_string(anonymous++);
            id = u"edge-" + Text(sequence.begin(), sequence.end());
        }
        if (!edge_index.emplace(id, document.edges.size()).second) fail("Duplicate Use Case edge id");
        document.edges.push_back({.id = std::move(id), .from = from.node.id, .to = to.node.id,
            .label = std::move(label.text), .kind = op.kind, .arrow = op.marker,
            .min_length = op.min_length, .reverse = op.reverse, .markdown = label.markdown});
        const bool from_declaration = from.explicit_declaration;
        const bool to_declaration = to.explicit_declaration;
        if ((!from_declaration && (!from.metadata.empty() || !from.node.classes.empty())) ||
            (!to_declaration && (!to.metadata.empty() || !to.node.classes.empty())))
            fail("Use Case relationship metadata requires a declaration");
        remember(std::move(from), from_declaration);
        remember(std::move(to), to_declaration);
    }

    void line(View input) {
        input = trim(input);
        if (input.empty() || input.starts_with(u"%%")) return;
        if (input == u"end") {
            if (boundary.empty()) fail("Unexpected Use Case boundary end");
            boundary.clear();
            return;
        }
        if (keyword(input, u"systemBoundary")) {
            if (!boundary.empty()) fail("Use Case boundaries cannot nest");
            auto item = endpoint(input, UsecaseNodeKind::boundary);
            if (!input.empty()) fail("Unexpected text after Use Case boundary title");
            const auto id = item.node.id;
            remember(std::move(item), true);
            boundary = id;
            return;
        }
        if (boundary.empty()) {
            if (keyword(input, u"direction")) {
                if (input != u"TD" && input != u"TB" && input != u"BT" && input != u"LR" && input != u"RL")
                    fail("Invalid Use Case direction");
                document.direction = input == u"TD" ? u"TB" : Text(input);
                return;
            }
            if (keyword(input, u"accTitle")) {
                if (!consume(input, u":")) fail("Expected Use Case accessibility title");
                document.acc_title = input;
                return;
            }
            if (keyword(input, u"accDescr")) {
                if (consume(input, u":")) document.acc_description = input;
                else if (input.starts_with(u'{') && input.ends_with(u'}'))
                    document.acc_description = trim(input.substr(1, input.size() - 2));
                else fail("Invalid Use Case accessibility description");
                return;
            }
            if (keyword(input, u"note")) {
                if (!keyword(input, u"for")) fail("Expected Use Case note target");
                Text target(take_id(input));
                auto label = take_label(input);
                if (!input.empty()) fail("Unexpected text after Use Case note");
                notes.push_back({std::move(target), std::move(label)});
                return;
            }
            if (keyword(input, u"classDef")) {
                auto ids = take_ids(input);
                const auto values = styles(input);
                for (auto& id : ids) document.class_styles[std::move(id)] = values;
                return;
            }
            if (keyword(input, u"class")) {
                auto ids = take_ids(input);
                auto classes = take_ids(input);
                if (!input.empty()) fail("Unexpected text after Use Case class list");
                pending_appearance.push_back({std::move(ids), std::move(classes), {}});
                return;
            }
            if (keyword(input, u"style")) {
                Text id(take_id(input));
                pending_appearance.push_back({{std::move(id)}, {}, styles(input)});
                return;
            }
        }
        const bool actor = keyword(input, u"actor");
        auto item = endpoint(input, actor ? UsecaseNodeKind::actor : UsecaseNodeKind::usecase);
        if (actor && input.starts_with(u',')) {
            remember(std::move(item), true);
            do remember(endpoint(input, UsecaseNodeKind::actor), true); while (consume(input, u","));
            if (!input.empty()) fail("Unexpected text after Use Case actor list");
            return;
        }
        if (!input.empty()) {
            relationship(std::move(item), input);
            return;
        }
        if (boundary.empty() && !item.explicit_declaration && !item.metadata.empty() &&
            item.node.stereotype.empty() && item.node.classes.empty()) {
            pending_metadata.emplace_back(std::move(item.node.id), std::move(item.metadata));
            return;
        }
        remember(std::move(item), true);
    }

    void finish() {
        if (!boundary.empty()) fail("Unclosed Use Case system boundary");
        for (const auto& [id, properties] : pending_metadata) {
            if (const auto node = node_index.find(id); node != node_index.end()) {
                for (const auto& [key, value] : properties)
                    apply_property(document.nodes[node->second], key, value);
            } else if (const auto edge = edge_index.find(id); edge != edge_index.end()) {
                auto& relationship = document.edges[edge->second];
                for (const auto& [key, value] : properties) {
                    if (key == u"animation" && (value == u"fast" || value == u"slow"))
                        relationship.animation = value;
                    else if (key == u"animate" && (value == u"true" || value == u"false"))
                        relationship.animation = value == u"true" ? u"fast" : u"";
                    else fail("Invalid Use Case edge metadata");
                }
            } else fail("Unknown Use Case metadata target");
        }
        for (const auto& appearance : pending_appearance) {
            for (const auto& id : appearance.ids) {
                const auto add = [&](auto& target) {
                    target.classes.insert(target.classes.end(), appearance.classes.begin(), appearance.classes.end());
                    target.styles.insert(target.styles.end(), appearance.styles.begin(), appearance.styles.end());
                };
                if (const auto node = node_index.find(id); node != node_index.end()) add(document.nodes[node->second]);
                else if (const auto edge = edge_index.find(id); edge != edge_index.end()) add(document.edges[edge->second]);
                else fail("Unknown Use Case style target");
            }
        }
        for (auto& node : document.nodes) validate_node(node);
        for (const auto& edge : document.edges) {
            const auto from = document.nodes[node_index.at(edge.from)].kind;
            const auto to = document.nodes[node_index.at(edge.to)].kind;
            if (from == UsecaseNodeKind::boundary || to == UsecaseNodeKind::boundary)
                fail("Use Case boundary cannot be a relationship endpoint");
            if ((edge.kind == UsecaseRelationship::include || edge.kind == UsecaseRelationship::extend) &&
                (from != UsecaseNodeKind::usecase || to != UsecaseNodeKind::usecase))
                fail("Use Case dependency requires two use cases");
            if (edge.kind == UsecaseRelationship::generalization && from != to)
                fail("Use Case generalization requires like endpoints");
            if (node_index.contains(edge.id) && !edge.id.starts_with(u"edge-"))
                fail("Duplicate Use Case declaration and edge id");
        }
        std::size_t note_sequence = 0;
        for (auto& note : notes) {
            const auto target = node_index.find(note.target);
            if (target == node_index.end() || document.nodes[target->second].kind == UsecaseNodeKind::boundary)
                fail("Unknown Use Case note target");
            const auto sequence = std::to_string(note_sequence++);
            const auto id = u"note-" + Text(sequence.begin(), sequence.end());
            document.nodes.push_back({.id = id, .label = std::move(note.label.text),
                .note_target = note.target, .kind = UsecaseNodeKind::note, .shape = UsecaseShape::rectangle,
                .markdown = note.label.markdown});
            document.edges.push_back({.id = id + u"-edge", .from = id, .to = note.target,
                .kind = UsecaseRelationship::note, .arrow = UsecaseArrow::none});
        }
    }
};

std::size_t statement_end(View source, std::size_t start) {
    std::size_t braces = 0;
    for (auto i = start; i < source.size(); ++i) {
        const auto ch = source[i];
        if (ch == u'"' || ch == u'\'') {
            if (source.substr(i).starts_with(u"\"`")) {
                const auto end = source.find(u"`\"", i + 2);
                if (end == View::npos) fail("Unclosed Use Case Markdown label");
                i = end + 1;
                continue;
            }
            const auto end = source.find(ch, i + 1);
            if (end != View::npos && source.substr(i, end - i).find(u'\n') == View::npos) {
                i = end;
                continue;
            }
        }
        if (ch == u'{' && (braces || (i > start && source[i - 1] == u'@') ||
                           trim(source.substr(start, i - start)) == u"accDescr")) ++braces;
        if (ch == u'}' && braces) --braces;
        if (ch == u'\n' && !braces) return i;
    }
    if (braces) fail("Unclosed Use Case metadata");
    return source.size();
}

}

std::expected<UsecaseDocument, std::string> parse_usecase(std::u16string_view source) {
    try {
        Parser parser;
        bool header = false;
        for (std::size_t start = 0; start < source.size();) {
            const auto rest = trim(source.substr(start));
            if (rest.empty()) break;
            const auto first_end = source.find(u'\n', start);
            const auto first = trim(source.substr(start, first_end == View::npos ? View::npos : first_end - start));
            if (first.empty() || first.starts_with(u"%%")) {
                start = first_end == View::npos ? source.size() : first_end + 1;
                continue;
            }
            const auto end = statement_end(source, start);
            const auto line = trim(source.substr(start, end - start));
            if (!header) {
                if (line != u"usecase-beta") fail("Expected usecase-beta diagram header");
                header = true;
            } else parser.line(line);
            start = end == source.size() ? end : end + 1;
        }
        if (!header) fail("Expected usecase-beta diagram header");
        parser.finish();
        return std::move(parser.document);
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
}

}
