// Mermaid 12.2.0 Agentflow shape normalization and containment. MIT; see THIRD_PARTY_NOTICES.md.
module folia.mermaid.agentflow;
import std;
import folia.mermaid.frontmatter;

namespace folia::mermaid {
namespace {
using View = std::u16string_view;
using Text = std::u16string;
using Code = AgentflowDiagnosticCode;
using Severity = AgentflowDiagnosticSeverity;

constexpr View registered_shapes[] = {
#include "agentflow_shape_names.inc"
};
constexpr std::pair<View, View> shape_aliases[] = {
    {u"task", u"roundedRect"}, {u"tool", u"subroutine"}, {u"input", u"lean-right"},
    {u"decision", u"diamond"}, {u"refdoc", u"lin-doc"}, {u"action", u"hexagon"},
    {u"round", u"rect"},
};
constexpr View removed_shapes[] = {
    u"doc", u"stadium", u"terminal", u"circle", u"trapezoid", u"inv_trapezoid",
    u"inv-trapezoid", u"doublecircle", u"double-circle", u"typeDeclaration", u"procs",
    u"lean_left", u"lean-left", u"in-out", u"cylinder", u"ellipse", u"odd", u"tag-rect",
    u"tagged-rectangle", u"delay", u"half-rounded-rectangle", u"lin-rect",
    u"lined-rectangle", u"win-pane", u"window-pane", u"curv-trap", u"curved-trapezoid",
};
constexpr View allowed_shapes[] = {
    u"roundedRect", u"subroutine", u"subprocess", u"subproc", u"framed-rectangle",
    u"lean-right", u"diamond", u"lin-doc", u"lined-document", u"hexagon", u"hex",
    u"connector", u"collapsedGroup",
};

View trim(View text) {
    const auto first = text.find_first_not_of(u" \t\r\n");
    if (first == View::npos) return text.substr(text.size());
    return text.substr(first, text.find_last_not_of(u" \t\r\n") - first + 1);
}
[[noreturn]] void fail(std::string message) {
    throw std::runtime_error(std::move(message));
}
std::string ascii(View value) { return {value.begin(), value.end()}; }

View canonical_shape(View shape) {
    for (const auto& [alias, canonical] : shape_aliases)
        if (alias == shape) return canonical;
    return shape;
}

void strip_prototype_keys(ConfigValue& value) {
    for (const auto key : {u"__proto__", u"constructor", u"prototype"}) value.mapping.erase(key);
    for (auto& [_, child] : value.mapping) strip_prototype_keys(child);
    for (auto& child : value.sequence) strip_prototype_keys(child);
}

void merge_metadata(ConfigValue& target, ConfigValue source) {
    if (source.kind != ConfigKind::mapping) return;
    target.kind = ConfigKind::mapping;
    for (auto& [key, value] : source.mapping) target.mapping.insert_or_assign(key, std::move(value));
}

std::size_t metadata_end(View text, std::size_t start) {
    bool quoted = false;
    for (auto i = start; i < text.size(); ++i) {
        if (text[i] == u'"') quoted = !quoted;
        if (!quoted && text[i] == u'}') return i;
    }
    return View::npos;
}

std::size_t find_outside(View text, std::size_t start, std::initializer_list<View> tokens) {
    std::size_t depth = 0;
    bool quoted = false;
    for (auto i = start; i < text.size(); ++i) {
        const auto ch = text[i];
        if (ch == u'"') quoted = !quoted;
        if (quoted) continue;
        if (depth == 0) {
            for (const auto token : tokens) if (text.substr(i).starts_with(token)) return i;
            if (text.substr(i).starts_with(u"@{")) {
                const auto end = metadata_end(text, i + 2);
                if (end == View::npos) return end;
                i = end;
                continue;
            }
        }
        if (ch == u'(' || ch == u'[' || ch == u'{') ++depth;
        else if ((ch == u')' || ch == u']' || ch == u'}') && depth) --depth;
    }
    return View::npos;
}

ConfigValue parse_metadata(View raw) {
    const bool inline_body = raw.find(u'\n') == View::npos;
    auto yaml = inline_body ? u"{" + Text(raw) + u"}" : Text(raw);
    auto parsed = parse_config_yaml(yaml);
    if (!parsed && !inline_body) {
        Text stripped;
        for (std::size_t i = 0; i < raw.size();) {
            auto end = raw.find(u'\n', i);
            if (end == View::npos) end = raw.size();
            auto line = raw.substr(i, end - i);
            const auto last = line.find_last_not_of(u" \t\r");
            if (last != View::npos && line[last] == u',') line = line.substr(0, last);
            stripped += line;
            stripped += u'\n';
            i = end + 1;
        }
        if (auto retry = parse_config_yaml(stripped)) parsed = std::move(retry);
    }
    if (!parsed) fail(parsed.error());
    if (parsed->kind == ConfigKind::null) parsed->kind = ConfigKind::mapping;
    strip_prototype_keys(*parsed);
    return std::move(*parsed);
}

void normalize_metadata_shape(ConfigValue& metadata) {
    if (auto shape = metadata.mapping.find(u"shape");
        shape != metadata.mapping.end() && shape->second.kind == ConfigKind::string)
        shape->second.string = Text(canonical_shape(shape->second.string));
}

struct Vertex {
    View mapped_source, id, label, shape;
    ConfigValue metadata;
};
Vertex vertex(View statement) {
    statement = trim(statement);
    Vertex out{.mapped_source = statement};
    const auto marker = find_outside(statement, 0, {u"@{"});
    const auto core = trim(statement.substr(0, marker));
    if (marker != View::npos) {
        const auto end = metadata_end(statement, marker + 2);
        if (end == View::npos) fail("Unclosed Agentflow metadata block");
        if (!trim(statement.substr(end + 1)).empty()) fail("Unexpected text after Agentflow metadata");
        // Jison consumes the closing brace without emitting a SHAPE_DATA token.
        out.mapped_source = statement.substr(0, end);
        out.metadata = parse_metadata(statement.substr(marker + 2, end - marker - 2));
    }
    const auto open = core.find_first_of(u"[({>");
    out.id = trim(core.substr(0, open));
    if (out.id.empty() || out.id.find_first_of(u" \t\r\n") != View::npos)
        fail("Expected Agentflow node identifier");
    if (open == View::npos) return out;
    const auto bracket = core.substr(open);
    struct ShapeForm { View open, close, shape; };
    constexpr ShapeForm forms[] = {
        {u"(((", u")))", u"doublecircle"}, {u"((", u"))", u"circle"},
        {u"(-", u"-)", u"ellipse"}, {u"([", u"])", u"stadium"},
        {u"[[", u"]]", u"subroutine"}, {u"[(", u")]", u"cylinder"},
        {u"{{", u"}}", u"hexagon"}, {u"[/", u"\\]", u"trapezoid"},
        {u"[\\", u"/]", u"inv_trapezoid"}, {u"[/", u"/]", u"lean_right"},
        {u"[\\", u"\\]", u"lean_left"}, {u">", u"]", u"odd"},
        {u"[", u"]", u"square"}, {u"(", u")", u"round"}, {u"{", u"}", u"diamond"},
    };
    for (const auto& form : forms) {
        if (!bracket.starts_with(form.open) || !bracket.ends_with(form.close) ||
            bracket.size() < form.open.size() + form.close.size()) continue;
        out.shape = form.shape;
        out.label = trim(bracket.substr(form.open.size(), bracket.size() - form.open.size() - form.close.size()));
        if (out.label.size() >= 2 && out.label.front() == u'"' && out.label.back() == u'"')
            out.label = out.label.substr(1, out.label.size() - 2);
        return out;
    }
    fail("Unclosed Agentflow node label");
}

struct ContainerMembers {
    std::vector<Text> ids;
    std::set<Text, std::less<>> seen;
    void add(View id) {
        if (seen.emplace(id).second) ids.emplace_back(id);
    }
};

class Parser {
public:
    explicit Parser(View source) : source(source) {
        for (auto i = source.find(u'\n'); i != View::npos; i = source.find(u'\n', i + 1))
            line_starts.push_back(i + 1);
    }

    AgentflowDocument parse() {
        bool header = false;
        for (std::size_t start = 0; start < source.size();) {
            auto end = source.find(u'\n', start);
            if (end == View::npos) end = source.size();
            auto line = trim(source.substr(start, end - start));
            if (const auto marker = find_outside(line, 0, {u"@{"}); marker != View::npos &&
                metadata_end(line, marker + 2) == View::npos) {
                const auto absolute = static_cast<std::size_t>(line.data() - source.data()) + marker;
                const auto close = metadata_end(source, absolute + 2);
                if (close == View::npos) fail("Unclosed Agentflow metadata block");
                end = source.find(u'\n', close);
                if (end == View::npos) end = source.size();
                line = trim(source.substr(start, end - start));
            }
            start = end + 1;
            if (line.empty() || line.starts_with(u"%%")) continue;
            if (!header) {
                read_header(line);
                header = true;
            } else statement(line);
        }
        if (!header) fail("Expected agentflow-beta diagram header");
        if (!scopes.empty()) fail("Unclosed Agentflow flow container");
        resolve_containment();
        normalize_shapes();
        if (document.nodes.empty() && document.containers.empty()) fail("Agentflow diagram has no nodes");
        return std::move(document);
    }

private:
    struct Scope {
        std::optional<std::size_t> container;
        ContainerMembers members;
        View opening;
    };
    View source;
    AgentflowDocument document;
    std::vector<std::size_t> line_starts{0};
    std::map<Text, std::size_t, std::less<>> nodes, containers, edges;
    std::map<Text, AgentflowSourceSpan, std::less<>> positions;
    std::vector<Scope> scopes;
    std::vector<ContainerMembers> memberships;
    std::vector<std::size_t> completion_order;
    std::set<std::size_t> completed;
    std::set<Text, std::less<>> claimed, global;
    std::map<Text, Text, std::less<>> parents;

    void read_header(View line) {
        const auto split = line.find_first_of(u" \t");
        if (line.substr(0, split) != u"agentflow-beta") fail("Expected agentflow-beta diagram header");
        if (split == View::npos) return;
        auto direction = trim(line.substr(split + 1));
        if (direction == u"TD") direction = u"TB";
        if (direction != u"TB" && direction != u"BT" && direction != u"LR" && direction != u"RL")
            fail("Invalid Agentflow direction");
        document.direction = direction;
    }

    AgentflowSourceSpan span(View begin, View end) const {
        const auto coordinates = [&](const char16_t* address) {
            const auto offset = static_cast<std::size_t>(address - source.data());
            const auto line = std::upper_bound(line_starts.begin(), line_starts.end(), offset);
            return std::pair{static_cast<std::size_t>(line - line_starts.begin()), offset - *std::prev(line)};
        };
        const auto [first_line, first_col] = coordinates(begin.data());
        const auto [last_line, last_col] = coordinates(end.data() + end.size());
        return {first_line, first_col, last_line, last_col};
    }

    void emit(Code code, Severity severity, Text message, const Text& id) {
        const auto found = positions.find(id);
        document.diagnostics.push_back({code, severity, std::move(message), id,
            found == positions.end() ? std::optional<AgentflowSourceSpan>{} : found->second});
    }

    void track(View id) {
        if (!scopes.empty()) scopes.back().members.add(id);
    }

    Text add(Vertex parsed, bool connector = false) {
        Text id(parsed.id);
        positions.try_emplace(id, span(parsed.mapped_source, parsed.mapped_source));
        if (connector) {
            global.insert(id);
        } else track(id);
        if (auto found = containers.find(id); found != containers.end()) {
            auto& container = document.containers[found->second];
            normalize_metadata_shape(parsed.metadata);
            merge_metadata(container.metadata, std::move(parsed.metadata));
            return id;
        }
        if (auto found = edges.find(id); found != edges.end() && parsed.metadata.kind == ConfigKind::mapping) {
            normalize_metadata_shape(parsed.metadata);
            merge_metadata(document.edges[found->second].metadata, std::move(parsed.metadata));
            return id;
        }
        auto [entry, fresh] = nodes.try_emplace(id, document.nodes.size());
        if (fresh) document.nodes.push_back({.id = id, .label = id});
        auto& node = document.nodes[entry->second];
        if (!parsed.label.empty()) node.label = parsed.label;
        if (connector) node.shape = u"connector";
        else if (!parsed.shape.empty() && node.shape != u"connector") node.shape = parsed.shape;
        if (node.shape != u"connector") {
            if (const auto* shape = parsed.metadata.find(u"shape"); shape && config_truthy(*shape)) {
                if (shape->kind != ConfigKind::string) {
                    auto value = config_to_string(*shape);
                    fail("No such shape: " + (value ? ascii(*value) : std::string("metadata value")) + ".");
                }
                const View authored = shape->string;
                const auto canonical = canonical_shape(authored);
                if (authored.find(u'_') != View::npos || std::ranges::any_of(authored, [](auto ch) {
                        return ch >= u'A' && ch <= u'Z';
                    })) fail("No such shape: " + ascii(authored) + ". Shape names should be lowercase.");
                if (canonical == authored && !std::ranges::binary_search(registered_shapes, authored))
                    fail("No such shape: " + ascii(authored) + ".");
                node.shape = authored;
            }
            if (const auto* label = parsed.metadata.find(u"label");
                label && label->kind == ConfigKind::string && !label->string.empty()) node.label = label->string;
        }
        normalize_metadata_shape(parsed.metadata);
        merge_metadata(node.metadata, std::move(parsed.metadata));
        return id;
    }

    void begin_container(View line, View body) {
        auto parsed = vertex(body);
        Text id(parsed.id);
        auto [entry, fresh] = containers.try_emplace(id, document.containers.size());
        if (fresh) {
            document.containers.push_back({.id = id, .label = parsed.label.empty() ? id : Text(parsed.label)});
            memberships.emplace_back();
        }
        auto& container = document.containers[entry->second];
        if (!parsed.label.empty()) container.label = parsed.label;
        normalize_metadata_shape(parsed.metadata);
        merge_metadata(container.metadata, std::move(parsed.metadata));
        track(id);
        scopes.push_back({entry->second, {}, line});
    }

    void close_scope(View line) {
        if (scopes.empty()) fail("Unexpected Agentflow end");
        auto closed = std::move(scopes.back());
        scopes.pop_back();
        if (!closed.container) {
            for (const auto& id : closed.members.ids) global.insert(id);
            return;
        }
        const auto index = *closed.container;
        const auto& id = document.containers[index].id;
        positions.try_emplace(id, span(closed.opening, line));
        if (completed.insert(index).second) completion_order.push_back(index);
        for (const auto& child : closed.members.ids)
            if (child != id && !global.contains(child) && claimed.insert(child).second)
                memberships[index].add(child);
    }

    void statement(View line) {
        if (line == u"end") return close_scope(line);
        if (line == u"global") {
            scopes.push_back({std::nullopt, {}, line});
            return;
        }
        if (line.starts_with(u"flow ") || line.starts_with(u"subgraph "))
            return begin_container(line, line.substr(line.find(u' ') + 1));
        if (line.starts_with(u"connector ")) {
            add(vertex(line.substr(10)), true);
            return;
        }
        const auto first = find_outside(line, 0, {u"--", u"-.-"});
        if (first == View::npos) {
            add(vertex(line));
            return;
        }
        auto [left, edge_id] = edge_source(line.substr(0, first));
        auto from = add(vertex(left));
        auto pos = first;
        while (pos < line.size()) {
            const bool dashed = line.substr(pos).starts_with(u"-.-");
            View label;
            auto after = pos + 3;
            if (!dashed && !line.substr(pos).starts_with(u"-->")) {
                const auto close = find_outside(line, pos + 2, {u"-->"});
                if (close == View::npos) fail("Unclosed Agentflow edge label");
                label = trim(line.substr(pos + 2, close - pos - 2));
                if (label.size() >= 2 && label.front() == u'"' && label.back() == u'"')
                    label = label.substr(1, label.size() - 2);
                after = close + 3;
            }
            auto right = trim(line.substr(after));
            if (right.starts_with(u"|")) {
                const auto close = right.find(u'|', 1);
                if (close == View::npos) fail("Unclosed Agentflow edge label");
                label = right.substr(1, close - 1);
                right = trim(right.substr(close + 1));
            }
            const auto next = find_outside(right, 0, {u"--", u"-.-"});
            auto [target, next_id] = edge_source(right.substr(0, next));
            auto to = add(vertex(target));
            if (!edge_id.empty()) edges.try_emplace(Text(edge_id), document.edges.size());
            document.edges.push_back({from, to, Text(label), dashed, Text(edge_id)});
            if (next == View::npos) break;
            pos = static_cast<std::size_t>(right.data() - line.data()) + next;
            from = std::move(to);
            edge_id = next_id;
        }
    }

    static std::pair<View, View> edge_source(View text) {
        text = trim(text);
        if (!text.ends_with(u'@')) return {text, {}};
        const auto split = text.find_last_of(u" \t");
        if (split == View::npos) return {text, {}};
        const auto id = text.substr(split + 1, text.size() - split - 2);
        return {trim(text.substr(0, split)), id};
    }

    bool creates_cycle(View child, View parent) const {
        while (true) {
            if (child == parent) return true;
            const auto next = parents.find(parent);
            if (next == parents.end()) return false;
            parent = next->second;
        }
    }

    void resolve_containment() {
        for (auto entry = completion_order.rbegin(); entry != completion_order.rend(); ++entry) {
            const auto& parent = document.containers[*entry].id;
            for (const auto& child : memberships[*entry].ids) {
                if (global.contains(child)) continue;
                if (creates_cycle(child, parent)) {
                    emit(Code::containment_violation, Severity::warning, u"Container \"" + parent +
                        u"\" cannot contain \"" + child + u"\" because \"" + child +
                        u"\" already contains it. The nesting that would close the loop is dropped.", child);
                } else parents.insert_or_assign(child, parent);
            }
        }
        for (auto& container : document.containers)
            if (const auto found = parents.find(container.id); found != parents.end()) container.parent = found->second;
        std::erase_if(document.nodes, [&](const AgentflowNode& node) { return containers.contains(node.id); });
        for (auto& node : document.nodes)
            if (const auto found = parents.find(node.id); found != parents.end()) node.parent = found->second;
    }

    void normalize_shapes() {
        for (auto& node : document.nodes) {
            const auto normalized = canonical_shape(node.shape);
            if (normalized == u"square" || normalized == u"squareRect" || normalized == u"rect") {
                node.shape = u"task";
                continue;
            }
            const bool removed = std::ranges::find(removed_shapes, normalized) != std::end(removed_shapes);
            if (!removed && std::ranges::find(allowed_shapes, normalized) != std::end(allowed_shapes)) continue;
            const auto message = u"shape \"" + Text(normalized) +
                (removed ? u"\" was removed in v0.8.1, using \"roundedRect\"" :
                           u"\" is not supported, using \"roundedRect\"");
            emit(removed ? Code::shape_removed : Code::shape_unsupported,
                removed ? Severity::error : Severity::warning, message, node.id);
            node.shape = u"task";
        }
    }
};
}

std::expected<AgentflowDocument, std::string> parse_agentflow(std::u16string_view source) {
    try {
        return Parser(source).parse();
    } catch (const std::exception& error) {
        return std::unexpected(error.what());
    }
}

}
