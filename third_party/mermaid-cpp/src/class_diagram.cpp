// Port of classDiagram.jison, ClassDB and ClassMember from Mermaid 12.2.0.
module folia.mermaid.class_diagram;
import std;

namespace folia::mermaid {
namespace {
using Text = std::u16string;
using View = std::u16string_view;
bool space(char16_t c) { return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r'; }
View trim(View s) {
    while (!s.empty() && space(s.front())) s.remove_prefix(1);
    while (!s.empty() && space(s.back())) s.remove_suffix(1);
    return s;
}
bool take(View& s, View prefix) {
    if (!s.starts_with(prefix)) return false;
    s = trim(s.substr(prefix.size())); return true;
}
Text generic_set(Text s) {
    const auto count = std::ranges::count(s,u'~');
    if (count < 2) return s;
    const bool leading = count % 2 && s.starts_with(u'~');
    if (leading) s.erase(0,1);
    for (;;) {
        const auto first = s.find(u'~'), last = s.rfind(u'~');
        if (first == s.npos || first == last) break;
        s[first] = u'<'; s[last] = u'>';
    }
    if (leading) s.insert(s.begin(),u'~');
    return s;
}
Text generics(View s) {
    std::vector<Text> sets;
    for (;;) {
        const auto comma = s.find(u','); sets.emplace_back(s.substr(0,comma));
        if (comma == s.npos) break;
        sets.emplace_back(u","); s.remove_prefix(comma+1);
    }
    std::vector<Text> output;
    for (std::size_t i = 0; i < sets.size(); ++i) {
        Text value = sets[i];
        if (value == u"," && i && i+1 < sets.size() &&
            std::ranges::count(sets[i-1],u'~') == 1 && std::ranges::count(sets[i+1],u'~') == 1) {
            value = sets[i-1]+u","+sets[i+1]; ++i; output.pop_back();
        }
        output.push_back(generic_set(std::move(value)));
    }
    Text result; for (const auto& value : output) result += value;
    return result;
}
Text html_angles(View value) {
    Text out;
    for (auto c : value) { if (c == u'<') out += u"&lt;"; else if (c == u'>') out += u"&gt;"; else out += c; }
    return out;
}
// ClassMember sanitizes its input before parsing/escaping angle brackets.
// HTML break tags therefore retain the canonical <br> spelling as literal
// member text, unlike the display breaks in class/relationship labels.
Text member_breaks(View value) {
    Text out;
    for(std::size_t i=0;i<value.size();) {
        if(value[i]==u'<') {
            auto end=value.find(u'>',i+1);
            if(end!=View::npos) {
                Text tag;
                for(auto c:value.substr(i+1,end-i-1))
                    if(c!=u'/'&&c!=u' '&&c!=u'\t'&&c!=u'\r'&&c!=u'\n')
                        tag+=c>=u'A'&&c<=u'Z'?c+u'a'-u'A':c;
                if(tag==u"br") {out+=u"<br>";i=end+1;continue;}
            }
        }
        out+=value[i++];
    }
    return out;
}
class Parser {
    ClassInput result;
    ClassLimits limits;
    std::map<Text,std::size_t,std::less<>> indices;
    std::map<Text,std::size_t,std::less<>> namespace_indices;
    struct NamespaceScope {
        std::size_t index;
        std::vector<std::size_t> classes, notes;
        bool has_statement = false;
    };
    std::vector<NamespaceScope> scopes;
    std::optional<std::size_t> body;
    std::size_t member_count = 0;
    [[noreturn]] static void fail(const char* message) { throw std::runtime_error(message); }
    static Text quoted(View& s, char16_t quote) {
        if (s.empty() || s.front() != quote) fail("Expected quoted Class text");
        const auto end = s.find(quote,1);
        if (end == s.npos) fail("Unclosed quoted Class text");
        Text value{s.substr(1,end-1)}; s = trim(s.substr(end+1)); return value;
    }
    static Text name(View& s) {
        s = trim(s); Text value;
        if (s.starts_with(u'`')) value = quoted(s,u'`');
        else {
            std::size_t n = 0;
            while (n < s.size()) {
                const auto c = s[n];
                if (!(c >= 128 || (c >= u'A' && c <= u'Z') || (c >= u'a' && c <= u'z') ||
                    (c >= u'0' && c <= u'9') || c == u'_' || c == u'-' || c == u'.')) break;
                if (s.substr(n).starts_with(u"--") || s.substr(n).starts_with(u"..")) break;
                ++n;
            }
            value = s.substr(0,n); s = trim(s.substr(n));
        }
        if (value.empty()) fail("Expected a Class identifier");
        if (s.starts_with(u'~')) { const auto type = quoted(s,u'~'); value += u"~"+type+u"~"; }
        return value;
    }
    static std::pair<Text,Text> split(View value) {
        const auto at = value.find(u'~');
        if (at == value.npos) return {Text{value},{}};
        return {Text{value.substr(0,at)},Text{value.substr(at+1,value.find(u'~',at+1)-at-1)}};
    }
    std::size_t add_class(View value) {
        auto [id,type] = split(value);
        if (auto found = indices.find(id); found != indices.end()) return found->second;
        if (result.classes.size() >= limits.classes) fail("Class count limit exceeded");
        const auto index = result.classes.size(); indices.emplace(id,index);
        result.classes.push_back({id,id,std::move(type)}); return index;
    }
    static void add_unique(std::vector<Text>& values,const Text& value) {
        if (std::ranges::find(values,value) == values.end()) values.push_back(value);
    }
    static std::vector<Text> style_list(View value) {
        std::vector<Text> output;
        for(;;) {
            const auto comma=value.find(u',');
            const auto item=trim(value.substr(0,comma));
            if(item.empty()) fail("Expected Class style declaration");
            output.emplace_back(item);
            if(comma==value.npos) break;
            value=trim(value.substr(comma+1));
        }
        return output;
    }
    void css_class(View ids,View value) {
        for(;;) {
            const auto comma=ids.find(u',');const auto id=split(trim(ids.substr(0,comma))).first;
            if(auto found=indices.find(id);found!=indices.end())
                result.classes[found->second].css_classes+=u" "+Text{value};
            if(comma==ids.npos) break;
            ids.remove_prefix(comma+1);
        }
    }
    void open_namespace(View rest) {
        const auto local_id = name(rest); Text label;
        if (take(rest,u"[")) {
            label = quoted(rest,u'"');
            if (!take(rest,u"]")) fail("Unclosed Class namespace label");
        }
        if (!take(rest,u"{") || !rest.empty()) fail("Expected Class namespace body");
        Text id = scopes.empty()?local_id:result.namespaces[scopes.back().index].id+u"."+local_id;
        Text parent; std::size_t start = 0, index = 0;
        for (;;) {
            const auto dot = id.find(u'.',start); const bool leaf = dot == id.npos;
            const Text current{id.substr(0,dot)};
            auto found = namespace_indices.find(current);
            if (found == namespace_indices.end()) {
                if (result.namespaces.size() >= limits.namespaces) fail("Class namespace limit exceeded");
                index = result.namespaces.size(); namespace_indices.emplace(current,index);
                result.namespaces.push_back({.id=current,.label=leaf&&!label.empty()?label:Text{id.substr(start,dot-start)},
                    .parent=parent,.explicit_declaration=leaf});
            } else {
                index = found->second;
                if (leaf) {
                    result.namespaces[index].explicit_declaration = true;
                    if (!label.empty()) result.namespaces[index].label = label;
                }
            }
            if (!parent.empty()) add_unique(result.namespaces[namespace_indices.at(parent)].children,current);
            if (leaf) break;
            parent = current; start = dot+1;
        }
        if (!scopes.empty()) scopes.back().has_statement = true;
        scopes.push_back({.index=index});
    }
    void close_namespace() {
        if (scopes.empty()) fail("Unexpected Class namespace closing brace");
        const auto scope = std::move(scopes.back()); scopes.pop_back();
        if (!scope.has_statement) fail("Expected Class namespace statements");
        auto& ns = result.namespaces[scope.index];
        for (const auto index : scope.classes) {
            auto& node = result.classes[index]; node.parent = ns.id; add_unique(ns.classes,node.id);
        }
        for (const auto index : scope.notes) {
            auto& note = result.notes[index]; note.parent = ns.id; add_unique(ns.notes,note.id);
        }
    }
    void annotation(std::size_t index, View value) {
        if (!value.starts_with(u"<<") || !value.ends_with(u">>")) fail("Expected Class annotation");
        result.classes[index].annotations.emplace_back(value.substr(2,value.size()-4));
    }
    void member(std::size_t index, View value) {
        const auto normalized=member_breaks(value);value=normalized;
        value = trim(value); if (value.empty()) return;
        if (++member_count > limits.members) fail("Class member limit exceeded");
        if (value.starts_with(u"<<") && value.ends_with(u">>")) { annotation(index,value); return; }
        ClassMember m;
        if (View{u"#+~-"}.find(value.front()) != View::npos) { m.visibility = value.front(); value.remove_prefix(1); }
        const auto close = value.rfind(u')');
        const bool method = close != value.npos && close > 0;
        if (method) {
            const auto open = value.rfind(u'(',close);
            if (open == value.npos || open == 0) fail("Expected Class method name and parentheses");
            m.id = value.substr(0,open); m.parameters = trim(value.substr(open+1,close-open-1));
            auto tail = value.substr(close+1);
            if (!tail.empty() && (tail.front() == u'$' || tail.front() == u'*')) {
                m.classifier = tail.front(); tail.remove_prefix(1);
            }
            m.return_type = trim(tail);
            if (m.classifier.empty() && !m.return_type.empty() &&
                (m.return_type.back() == u'$' || m.return_type.back() == u'*')) {
                m.classifier = m.return_type.back(); m.return_type.pop_back();
            }
        } else {
            if (!value.empty() && (value.back() == u'$' || value.back() == u'*')) {
                m.classifier = value.back(); value.remove_suffix(1);
            }
            m.id = value;
        }
        const bool leading = m.id.starts_with(u' ');
        m.id = Text{trim(m.id)}; if (leading) m.id.insert(m.id.begin(),u' ');
        Text display = m.visibility+generics(m.id);
        if (method) {
            display += u"("+generics(m.parameters)+u")";
            if (!m.return_type.empty()) display += u" : "+generics(m.return_type);
        }
        m.display_text = trim(display);
        m.text = html_angles((m.visibility.empty()?Text{}:Text{u"\\"})+display);
        if (m.classifier == u"*") m.css_style = u"font-style:italic;";
        else if (m.classifier == u"$") m.css_style = u"text-decoration:underline;";
        auto& node = result.classes[index];
        (method?node.methods:node.members).push_back(std::move(m));
    }
    static ClassMarker marker(View& s) {
        if (take(s,u"<|") || take(s,u"|>")) return ClassMarker::extension;
        if (take(s,u"*")) return ClassMarker::composition;
        if (take(s,u"o")) return ClassMarker::aggregation;
        if (take(s,u"<") || take(s,u">")) return ClassMarker::dependency;
        return ClassMarker::none;
    }
    void body_line(View line) {
        const auto end = line.find(u'}');
        auto value = trim(line.substr(0,end));
        if (value.find(u'{') != value.npos) fail("Nested Class body is not valid");
        member(*body,value);
        if (end != line.npos) {
            if (!trim(line.substr(end+1)).empty()) fail("Unexpected text after Class body");
            body.reset();
        }
    }
    void statement(View line) {
        if (body) { body_line(line); return; }
        View rest = line;
        if (line == u"}") { close_namespace(); return; }
        if (take(rest,u"namespace ")) { open_namespace(rest); return; }
        if (!scopes.empty() && !line.starts_with(u"class ") && !line.starts_with(u"note "))
            fail("Class namespaces contain class, note and namespace declarations");
        if (take(rest,u"style ")) {
            const auto id=name(rest);const auto styles=style_list(rest);
            if(auto found=indices.find(id);found!=indices.end()) {
                auto& output=result.classes[found->second].styles;
                output.insert(output.end(),styles.begin(),styles.end());
            }
            return;
        }
        if (take(rest,u"cssClass ")) {
            const auto ids=quoted(rest,u'"');const auto value=name(rest);
            if(!rest.empty()) fail("Unexpected text after Class cssClass");
            css_class(ids,value);return;
        }
        if (take(rest,u"classDef ")) {
            std::vector<Text> names{name(rest)};
            while(take(rest,u",")) names.push_back(name(rest));
            const auto styles=style_list(rest);
            // ClassDB.defineClass appends these declarations to currently
            // assigned classes, in statement order (including default).
            for(const auto& id:names) for(auto& node:result.classes)
                if(node.css_classes.find(id)!=Text::npos)
                    node.styles.insert(node.styles.end(),styles.begin(),styles.end());
            return;
        }
        if (take(rest,u"direction ")) {
            if (rest != u"LR" && rest != u"RL" && rest != u"TB" && rest != u"BT") fail("Unsupported Class direction");
            result.direction = rest; return;
        }
        if (take(rest,u"accTitle:")) { result.acc_title = rest; return; }
        if (take(rest,u"accDescr:")) { result.acc_description = rest; return; }
        if (take(rest,u"note ")) {
            ClassNote note;
            if (take(rest,u"for ")) note.class_id = name(rest);
            note.text = quoted(rest,u'"');
            if (!rest.empty()) fail("Unexpected text after Class note");
            if (result.notes.size() >= limits.notes) fail("Class note limit exceeded");
            const auto index = std::to_string(result.notes.size());
            note.id = u"note"+Text{index.begin(),index.end()};
            if (!scopes.empty()) { scopes.back().notes.push_back(result.notes.size()); scopes.back().has_statement = true; }
            result.notes.push_back(std::move(note)); return;
        }
        if (take(rest,u"class ")) {
            const auto index = add_class(name(rest));
            if (!scopes.empty()) { scopes.back().classes.push_back(index); scopes.back().has_statement = true; }
            if (take(rest,u"[")) {
                result.classes[index].label = quoted(rest,u'"');
                if (!take(rest,u"]")) fail("Unclosed Class display label");
            }
            if (take(rest,u":::")) css_class(result.classes[index].id,name(rest));
            if (rest.starts_with(u"<<")) {
                const auto end = rest.find(u">>");
                if (end == rest.npos) fail("Unclosed Class annotation");
                annotation(index,rest.substr(0,end+2)); rest = trim(rest.substr(end+2));
            }
            if (take(rest,u"{")) { body = index; body_line(rest); }
            else if (!rest.empty()) fail("Unsupported Class declaration");
            return;
        }
        if (rest.starts_with(u"<<")) {
            const auto end = rest.find(u">>");
            if (end == rest.npos) fail("Unclosed Class annotation");
            const auto value = rest.substr(0,end+2); rest = trim(rest.substr(end+2));
            const auto id = split(name(rest)).first;
            const auto found = indices.find(id);
            if (found == indices.end() || !rest.empty()) fail("Expected declared Class for annotation");
            annotation(found->second,value); return;
        }
        const auto from = name(rest);
        if (rest.empty()) return; // Original standalone className has no DB action.
        if (take(rest,u":")) { member(add_class(from),rest); return; }
        ClassRelation r;
        if (rest.starts_with(u'"')) r.start_label = trim(quoted(rest,u'"'));
        r.start = marker(rest);
        if (take(rest,u"..")) r.dotted = true;
        else if (!take(rest,u"--")) fail("Expected a supported Class relationship");
        r.end = marker(rest);
        if (rest.starts_with(u'"')) r.end_label = trim(quoted(rest,u'"'));
        const auto to = name(rest);
        if (take(rest,u":")) r.title = rest;
        else if (!rest.empty()) fail("Unexpected text after Class relationship");
        const auto from_index = add_class(from), to_index = add_class(to);
        r.from = result.classes[from_index].id; r.to = result.classes[to_index].id;
        if (result.relations.size() >= limits.relations) fail("Class relationship limit exceeded");
        result.relations.push_back(std::move(r));
    }
public:
    explicit Parser(ClassLimits value) : limits(std::move(value)) {}
    ClassInput run(PreparedDiagram& prepared) {
        result.title = prepared.title.value_or(Text{});
        const auto encoded = encode_diagram_source(prepared.code.cleaned);
        View source = encoded; bool header = false, content = false;
        while (!source.empty()) {
            auto end = source.find(u'\n'); auto line = trim(source.substr(0,end));
            if (!body && line.starts_with(u"note ")) {
                const auto open = source.find(u'"');
                if (open != source.npos && (end == source.npos || open < end)) {
                    const auto close = source.find(u'"',open+1);
                    if (close == source.npos) fail("Unclosed quoted Class note");
                    end = source.find(u'\n',close+1); line = trim(source.substr(0,end));
                }
            }
            if (!line.empty() && !line.starts_with(u"%%")) {
                if (!header) {
                    if (line != u"classDiagram" && line != u"classDiagram-v2") fail("Expected Class diagram header");
                    header = true;
                } else { statement(line); content = true; }
            }
            if (end == source.npos) break;
            source.remove_prefix(end+1);
        }
        if (!header || !content) fail("Expected Class diagram statements");
        if (body) fail("Unclosed Class body");
        if (!scopes.empty()) fail("Unclosed Class namespace body");
        result.effective_config = std::move(prepared.effective_config);
        return std::move(result);
    }
};
}
std::expected<ClassInput,std::string> prepare_class(
    std::u16string_view source, const ConfigSession& session, ClassLimits limits) {
    auto prepared = prepare_diagram(source,session,limits.frontend);
    if (!prepared) return std::unexpected(prepared.error());
    if (prepared->type != u"class" && prepared->type != u"classDiagram")
        return std::unexpected("Expected a Class diagram");
    try { return Parser{std::move(limits)}.run(*prepared); }
    catch (const std::exception& error) { return std::unexpected(std::string{error.what()}); }
}
}
