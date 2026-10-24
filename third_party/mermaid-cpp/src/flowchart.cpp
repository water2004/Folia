module folia.mermaid.flowchart;
import std;

namespace folia::mermaid {
namespace {
using Text = std::u16string;
using View = std::u16string_view;
using Texts = std::vector<Text>;
bool space(char16_t c) { return c == u' ' || c == u'\t' || c == u'\r'; }
Text trim(View s) {
    while (!s.empty() && (space(s.front()) || s.front() == u'\n')) s.remove_prefix(1);
    while (!s.empty() && (space(s.back()) || s.back() == u'\n')) s.remove_suffix(1);
    return Text{s};
}
Texts split(View s, char16_t delimiter) {
    Texts out;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= s.size(); ++i) {
        if (i < s.size() && s[i] == u'\\' && i + 1 < s.size()) { ++i; continue; }
        if (i == s.size() || s[i] == delimiter) { out.push_back(trim(s.substr(start, i-start))); start = i+1; }
    }
    return out;
}
Text join(const Texts& values) {
    Text out;
    for (const auto& value : values) { if (!out.empty()) out += u' '; out += value; }
    return out;
}
Text digits(std::size_t n) { auto s = std::to_string(n); return Text{s.begin(),s.end()}; }
void append(Texts& target, const Texts& source) { target.insert(target.end(),source.begin(),source.end()); }
struct Vertex { FlowchartNode node; Texts classes; };
struct Group { FlowchartNode node; Texts members, classes; };
struct Class { Texts styles, text_styles; };
struct Label { Text text, type = u"text"; };

class Parser {
    View source;
    std::size_t pos = 0, group_counter = 0;
    FlowchartLimits limits;
    FlowchartInput result;
    std::vector<Vertex> vertices;
    std::map<Text, std::size_t, std::less<>> vertex_index;
    std::vector<Group> groups;
    std::map<Text, Class, std::less<>> classes;
    Texts default_edge_styles;
    Text curve = u"basis";
    double padding = 15;
    bool inherit_direction = false;

    [[noreturn]] void fail(const char* message) const {
        throw std::runtime_error(std::string{message} + " at prepared UTF-16 offset " + std::to_string(pos));
    }
    bool at(View value) const { return source.substr(pos).starts_with(value); }
    bool take(View value) { if (!at(value)) return false; pos += value.size(); return true; }
    void horizontal() { while (pos < source.size() && space(source[pos])) ++pos; }
    bool end_statement() const { return pos == source.size() || source[pos] == u'\n' || source[pos] == u';'; }
    void separator() {
        horizontal();
        if (!end_statement()) fail("Expected a statement separator or supported Flowchart syntax");
        if (pos < source.size()) ++pos;
    }
    bool keyword(View value) const {
        return at(value) && (pos + value.size() == source.size() || space(source[pos+value.size()]) ||
            source[pos+value.size()] == u'\n' || source[pos+value.size()] == u';');
    }
    Text word() {
        horizontal(); auto start = pos;
        while (pos < source.size() && !space(source[pos]) && !end_statement()) ++pos;
        if (pos == start) fail("Expected a word");
        return Text{source.substr(start,pos-start)};
    }
    Text rest() {
        horizontal(); auto start = pos;
        while (!end_statement()) ++pos;
        return trim(source.substr(start,pos-start));
    }
    // Bracket and quoted strings have lexical states; separators inside a
    // quoted label are text, never graph statements.
    Label label(View close) {
        horizontal(); Label value;
        if (take(u"\"")) {
            value.type = u"string";
            const auto start = pos;
            while (pos < source.size() && source[pos] != u'"') ++pos;
            if (pos == source.size()) fail("Unclosed quoted label");
            value.text = trim(source.substr(start,pos-start)); ++pos; horizontal();
            if (value.text.size()>=2 && value.text.starts_with(u'`') && value.text.ends_with(u'`')) {
                value.type=u"markdown";value.text=value.text.substr(1,value.text.size()-2);
            }
        } else {
            const auto start = pos;
            while (pos < source.size() && !at(close)) {
                if (source[pos] == u'\n' || source[pos] == u'"' || source[pos] == u';' ||
                    source[pos] == u'[' || source[pos] == u'(' || source[pos] == u'{' ||
                    source[pos] == u']' || source[pos] == u')' || source[pos] == u'}')
                    fail("Quote punctuation in Flowchart labels");
                ++pos;
            }
            value.text = trim(source.substr(start,pos-start));
        }
        if (value.text.empty() || !take(close)) fail("Unclosed or empty Flowchart label");
        return value;
    }
    bool link_start(std::size_t p) const {
        auto s = source.substr(p);
        if (!s.empty() && (s[0] == u'<' || s[0] == u'o' || s[0] == u'x')) s.remove_prefix(1);
        // The dotted-edge lexer also accepts a dot-leading ending (.->),
        // especially after an infix label: A -. text .-> B.
        if (s.starts_with(u'.')) {
            while (s.starts_with(u'.')) s.remove_prefix(1);
            return s.starts_with(u'-');
        }
        return s.starts_with(u"--") || s.starts_with(u"==") || s.starts_with(u"-.") || s.starts_with(u"~~");
    }
    Text identifier() {
        horizontal(); auto start = pos;
        while (pos < source.size()) {
            auto c = source[pos];
            if (space(c) || c == u'\n' || c == u';' || c == u'[' || c == u'(' || c == u'{' ||
                c == u'>' || c == u'<' || c == u':' || c == u'&' || c == u'|' || c == u'@' ||
                c == u']' || c == u')' || c == u'}' || c == u'"' || c == u',' || link_start(pos)) break;
            ++pos;
        }
        if (pos == start) fail("Expected a node id");
        auto id = Text{source.substr(start,pos-start)};
        if (id == u"end") fail("Reserved node id end");
        return id;
    }
    Vertex& vertex(const Text& id) {
        if (auto it = vertex_index.find(id); it != vertex_index.end()) return vertices[it->second];
        if (vertices.size() + groups.size() >= limits.nodes) fail("Flowchart node limit exceeded");
        vertex_index.emplace(id,vertices.size());
        Vertex v; v.node.id = id; v.node.label = id; v.node.padding = padding;
        vertices.push_back(std::move(v)); return vertices.back();
    }
    Text node() {
        auto id = identifier(); auto& v = vertex(id); horizontal();
        struct Shape { View open, close, shape; };
        static constexpr Shape shapes[] = {
            {u"(((",u")))",u"doublecircle"},{u"((",u"))",u"circle"},
            {u"([",u"])",u"stadium"},{u"[[",u"]]",u"subroutine"},
            {u"[(",u")]",u"cylinder"},{u"{{",u"}}",u"hexagon"},
            {u"[",u"]",u"squareRect"},{u"(",u")",u"roundedRect"},
            {u"{",u"}",u"diamond"},{u">",u"]",u"odd"}};
        if (at(u"[/") || at(u"[\\")) {
            const bool left_slash = source[pos+1] == u'/'; pos += 2;
            const auto start = pos;
            while (pos+1 < source.size() && !at(u"/]") && !at(u"\\]")) ++pos;
            if (pos+1 == source.size()) fail("Unclosed slanted node");
            const bool right_slash = source[pos] == u'/';
            const auto close = source.substr(pos,2); pos = start;
            auto text = label(close); v.node.label = std::move(text.text); v.node.label_type = std::move(text.type);
            v.node.shape = left_slash ? (right_slash ? u"lean_right" : u"trapezoid") :
                (right_slash ? u"inv_trapezoid" : u"lean_left");
        } else for (const auto& shape : shapes) if (take(shape.open)) {
            auto text = label(shape.close); v.node.label = std::move(text.text);
            v.node.label_type = std::move(text.type); v.node.shape = shape.shape; break;
        }
        horizontal();
        if (take(u":::")) v.classes.push_back(identifier());
        return id;
    }
    Texts node_list() {
        Texts out{node()}; horizontal();
        while (take(u"&")) { out.push_back(node()); horizontal(); }
        return out;
    }
    Text link_token() {
        horizontal(); auto start = pos;
        if (pos < source.size() && (source[pos] == u'<' || source[pos] == u'x' || source[pos] == u'o')) ++pos;
        while (pos < source.size() && (source[pos] == u'-' || source[pos] == u'=' || source[pos] == u'.' || source[pos] == u'~')) ++pos;
        if (pos < source.size() && (source[pos] == u'>' || source[pos] == u'x' || source[pos] == u'o')) ++pos;
        return Text{source.substr(start,pos-start)};
    }
    FlowchartEdge link() {
        auto token = link_token(); FlowchartEdge edge; horizontal();
        Text prefix;
        if (token == u"--" || token == u"==" || token == u"-.") {
            prefix = token; const auto start = pos;
            bool quoted = false;
            while (!end_statement()) {
                if (source[pos] == u'"') quoted = !quoted;
                if (!quoted && link_start(pos)) break;
                ++pos;
            }
            if (end_statement()) fail("Unfinished labelled edge");
            const auto end = pos; token = link_token();
            auto text = trim(source.substr(start,end-start));
            if (text.starts_with(u'"') && text.ends_with(u'"')) {
                edge.label_type = u"string"; text = text.substr(1,text.size()-2);
                if(text.size()>=2 && text.starts_with(u'`') && text.ends_with(u'`')) {
                    edge.label_type=u"markdown";text=text.substr(1,text.size()-2);
                }
            }
            edge.label = std::move(text);
        }
        if (token.size() < 3 && token != u"-.-") fail("Incomplete edge");
        View line = token; auto last = line.back(); line.remove_suffix(1);
        auto arrow = [](char16_t c) -> View { return c == u'x' ? u"arrow_cross" : c == u'o' ? u"arrow_circle" : u"arrow_point"; };
        if (last == u'>' || last == u'x' || last == u'o') {
            edge.type = arrow(last);
            if (token.front() == (last == u'>' ? u'<' : last)) { edge.type = u"double_" + edge.type; line.remove_prefix(1); }
        } else edge.type = u"arrow_open";
        edge.min_length = line.size()-1;
        if (line.starts_with(u'=')) edge.stroke = u"thick";
        if (line.starts_with(u'~')) edge.stroke = u"invisible";
        const auto dots = std::ranges::count(line,u'.');
        if (dots) { edge.stroke = u"dotted"; edge.min_length = static_cast<std::size_t>(dots); }
        if (!prefix.empty() && ((prefix == u"==" && edge.stroke != u"thick") ||
            (prefix == u"-." && edge.stroke != u"dotted") || (prefix == u"--" && edge.stroke != u"normal")))
            fail("Mismatched labelled edge strokes");
        edge.min_length = std::min<std::size_t>(edge.min_length,10);
        horizontal();
        if (take(u"|")) { auto text = label(u"|"); edge.label = std::move(text.text); edge.label_type = std::move(text.type); }
        edge.curve = curve;
        if (edge.type.starts_with(u"double_")) edge.arrow_start = edge.arrow_end = edge.type.substr(7);
        else edge.arrow_end = edge.type;
        if (edge.stroke == u"invisible" || edge.type == u"arrow_open") edge.arrow_start = edge.arrow_end = u"none";
        return edge;
    }
    void add_edges(const Texts& from, const Texts& to, const FlowchartEdge& prototype) {
        for (const auto& start : from) for (const auto& end : to) {
            if (result.edges.size() >= limits.edges) fail("Flowchart edge limit exceeded");
            auto edge = prototype; edge.start = start; edge.end = end;
            auto count = std::ranges::count_if(result.edges,[&](const auto& e){return e.start == start && e.end == end;});
            edge.id = u"L_" + start + u"_" + end + u"_" + digits(count ? count+1 : 0);
            result.edges.push_back(std::move(edge));
        }
    }
    Texts chain() {
        auto from = node_list(); auto members = from; horizontal();
        while (link_start(pos)) {
            auto edge = link(); auto to = node_list(); add_edges(from,to,edge);
            members.insert(members.begin(),to.begin(),to.end()); from = std::move(to); horizontal();
        }
        separator(); return members;
    }
    void set_class(const Text& ids, const Text& name) {
        for (const auto& id : split(ids,u',')) {
            if (auto it = vertex_index.find(id); it != vertex_index.end()) vertices[it->second].classes.push_back(name);
            for (auto& g : groups) if (g.node.id == id) g.classes.push_back(name);
        }
    }
    void style_statement(const Text& command) {
        const auto ids = word(); const auto value = rest();
        if (value.empty()) fail("Missing style or class");
        if (command == u"linkStyle" && value.starts_with(u"interpolate "))
            fail("Per-edge interpolation commands are not supported by this stage");
        auto styles = split(value,u',');
        if (command == u"class") set_class(ids,value);
        else if (command == u"classDef") {
            for (const auto& id : split(ids,u',')) for (const auto& style : styles) {
                classes[id].styles.push_back(style);
                if (style.find(u"color") != Text::npos) {
                    auto text_style = style;
                    if (auto p = text_style.find(u"fill"); p != Text::npos) text_style.replace(p,4,u"bgFill");
                    classes[id].text_styles.push_back(std::move(text_style));
                }
            }
        } else if (command == u"style") append(vertex(ids).node.styles,styles);
        else if (ids == u"default") default_edge_styles = std::move(styles);
        else for (const auto& id : split(ids,u',')) {
            std::size_t index = 0;
            for (auto c : id) {
                if (c < u'0' || c > u'9' || index > result.edges.size()) fail("Invalid linkStyle index");
                index = index*10 + (c-u'0');
            }
            if (index >= result.edges.size()) fail("linkStyle index out of bounds");
            auto& target = result.edges[index].styles; target = styles;
            if (!std::ranges::any_of(target,[](const auto& s){return s.starts_with(u"fill");})) target.push_back(u"fill:none");
        }
        separator();
    }
    Text subgraph(std::size_t depth) {
        pos += 8; horizontal(); const auto start = pos;
        while (!end_statement() && source[pos] != u'[') ++pos;
        auto name = trim(source.substr(start,pos-start));
        Group group; group.node.id = name; group.node.label = name;
        group.node.shape = u"rect"; group.node.group = true; group.node.padding = 8;
        bool implicit = name.empty() || name.find_first_of(u" \t") != Text::npos;
        if (take(u"[")) {
            auto title = label(u"]"); group.node.label = std::move(title.text);
            group.node.label_type = std::move(title.type); implicit = false;
        }
        separator(); group.members = block(depth+1,group.node.direction);
        if (implicit) group.node.id = u"subGraph" + digits(group_counter);
        ++group_counter;
        if (group.node.direction.empty() && inherit_direction) group.node.direction = result.direction;
        Texts unique;
        for (const auto& id : group.members) {
            if (id == group.node.id || std::ranges::find(unique,id) != unique.end()) continue;
            if (std::ranges::any_of(groups,[&](const auto& g){return std::ranges::find(g.members,id) != g.members.end();})) continue;
            unique.push_back(id);
        }
        group.members = std::move(unique);
        for (auto& existing : groups) if (existing.node.id == group.node.id) {
            append(existing.members,group.members); return existing.node.id;
        }
        if (vertices.size() + groups.size() >= limits.nodes) fail("Flowchart node limit exceeded");
        auto id = group.node.id; groups.push_back(std::move(group)); return id;
    }
    Texts block(std::size_t depth, Text& direction) {
        if (depth > limits.subgraph_depth) fail("Flowchart subgraph nesting limit exceeded");
        Texts members;
        while (pos < source.size()) {
            horizontal(); if (end_statement()) { separator(); continue; }
            if (keyword(u"end")) {
                if (!depth) fail("Unexpected subgraph end");
                pos += 3; separator(); return members;
            }
            if (keyword(u"subgraph")) { members.push_back(subgraph(depth)); continue; }
            if (keyword(u"direction")) { pos += 9; direction = word(); separator(); continue; }
            if (at(u"accTitle:") || at(u"accDescr:")) {
                bool title = at(u"accTitle:"); pos += 9;
                (title ? result.acc_title : result.acc_description) = rest(); separator(); continue;
            }
            if (keyword(u"classDef") || keyword(u"class") || keyword(u"style") || keyword(u"linkStyle")) {
                auto command = word(); style_statement(command); continue;
            }
            if (keyword(u"click")) fail("Flowchart browser callbacks are not supported");
            append(members,chain());
        }
        if (depth) fail("Unclosed subgraph");
        return members;
    }
    Texts compiled(const Texts& names) const {
        Texts out;
        for (const auto& name : names) if (auto it = classes.find(name); it != classes.end()) {
            append(out,it->second.styles); append(out,it->second.text_styles);
        }
        return out;
    }
    void layout_input() {
        std::map<Text,Text,std::less<>> parents;
        for (auto it = groups.rbegin(); it != groups.rend(); ++it)
            for (const auto& id : it->members) parents[id] = it->node.id;
        for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
            auto node = it->node; node.parent = parents[node.id]; node.classes = join(it->classes);
            node.compiled_styles = compiled(it->classes); result.nodes.push_back(std::move(node));
        }
        for (auto& vertex : vertices) {
            auto existing = std::ranges::find(result.nodes,vertex.node.id,&FlowchartNode::id);
            if (existing != result.nodes.end()) {
                existing->styles = std::move(vertex.node.styles); existing->classes = join(vertex.classes);
                existing->compiled_styles = compiled(vertex.classes);
            } else {
                auto node = std::move(vertex.node); node.parent = parents[node.id]; node.classes = u"default " + join(vertex.classes);
                Texts names{u"default",u"node"}; append(names,vertex.classes); node.compiled_styles = compiled(names);
                result.nodes.push_back(std::move(node));
            }
        }
        for (auto& edge : result.edges) edge.styles.insert(edge.styles.begin(),default_edge_styles.begin(),default_edge_styles.end());
    }
public:
    Parser(View source_, FlowchartLimits limits_) : source(source_), limits(std::move(limits_)) {}
    FlowchartInput run(PreparedDiagram& prepared) {
        const auto* cfg = prepared.effective_config.find(u"flowchart");
        auto numeric = [&](View key, double fallback) {
            const auto* value = cfg ? cfg->find(key) : nullptr;
            if (!value) return fallback;
            if (value->kind != ConfigKind::number || !std::isfinite(value->number) || value->number < 0)
                fail("Flowchart spacing and padding must be finite nonnegative numbers");
            return value->number;
        };
        result.node_spacing = numeric(u"nodeSpacing",50); result.rank_spacing = numeric(u"rankSpacing",50);
        padding = numeric(u"padding",15); if (!padding) padding = 8;
        if (cfg) {
            if (auto p = cfg->find(u"curve")) { if (p->kind != ConfigKind::string) fail("Invalid Flowchart curve"); curve = p->string; }
            if (auto p = cfg->find(u"inheritDir")) inherit_direction = p->kind == ConfigKind::boolean && p->boolean;
        }
        if (auto max = prepared.effective_config.find(u"maxEdges"); max && max->kind == ConfigKind::number && std::isfinite(max->number)) {
            if (max->number < 0) fail("Invalid maxEdges");
            if (max->number < static_cast<double>(limits.edges)) limits.edges = static_cast<std::size_t>(max->number);
        }
        while (pos < source.size() && (space(source[pos]) || source[pos] == u'\n')) ++pos;
        const auto header = word();
        if (header != u"graph" && header != u"flowchart") fail("Unsupported Flowchart header");
        horizontal();
        if (!end_statement()) result.direction = word();
        if (result.direction == u"TD") result.direction = u"TB";
        if (result.direction != u"TB" && result.direction != u"BT" && result.direction != u"LR" && result.direction != u"RL") fail("Unsupported Flowchart direction");
        separator(); Text unused;
        block(0,unused); layout_input();
        result.title = prepared.title.value_or(Text{});
        result.effective_config = std::move(prepared.effective_config);
        return std::move(result);
    }
};
}
std::expected<FlowchartInput,std::string> prepare_flowchart(
    std::u16string_view source, const ConfigSession& session, FlowchartLimits limits) {
    auto prepared = prepare_diagram(source,session,limits.frontend);
    if (!prepared) return std::unexpected(prepared.error());
    if (prepared->type != u"flowchart-v2" && prepared->type != u"flowchart")
        return std::unexpected("Expected a supported Flowchart diagram");
    try {
        // Keep prepared source storage in place, including short-string storage.
        Parser parser{prepared->code.cleaned,std::move(limits)};
        return parser.run(*prepared);
    } catch (const std::exception& error) { return std::unexpected(std::string{error.what()}); }
}
}
