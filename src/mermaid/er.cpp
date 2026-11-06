// Mermaid 12.2.0 ER parser/DB projection.
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
bool take(View& s, View prefix) {
    if (!s.starts_with(prefix)) return false;
    s.remove_prefix(prefix.size()); return true;
}

class Parser {
    ErInput result;
    ErLimits limits;
    std::map<Text, std::size_t, std::less<>> entity_indices;
    std::size_t attribute_count = 0;
    [[noreturn]] static void fail(const char* message) { throw std::runtime_error(message); }
    static Text quoted(View& s) {
        s = trim(s);
        if (s.empty() || s.front() != u'"') fail("Expected quoted ER text");
        auto end = s.find(u'"', 1);
        if (end == View::npos) fail("Unclosed quoted ER text");
        Text out{s.substr(1, end - 1)};
        s = trim(s.substr(end + 1));
        return out;
    }
    static Text identifier(View& s) {
        s = trim(s);
        std::size_t n = 0;
        while (n < s.size()) {
            const auto c = s[n];
            if (space(c) || c == u'[' || c == u']' || c == u'{' || c == u'}' ||
                c == u':' || c == u'|' || c == u'<' || c == u'>' || c == u'.') break;
            ++n;
        }
        if (!n) fail("Expected an ER entity name");
        Text out{s.substr(0,n)}; s = trim(s.substr(n)); return out;
    }
    std::size_t entity(View name) {
        if (auto i = entity_indices.find(name); i != entity_indices.end()) return i->second;
        if (result.entities.size() >= limits.entities) fail("ER entity limit exceeded");
        const auto index = result.entities.size();
        entity_indices.emplace(Text{name}, index);
        const auto n = std::to_string(index);
        result.entities.push_back({Text{name}, u"entity-" + Text{name} + u"-" + Text{n.begin(),n.end()}, {}, {}});
        return index;
    }
    void declaration(View line) {
        auto name = identifier(line);
        const auto index = entity(name);
        if (take(line,u"[")) {
            result.entities[index].alias = quoted(line);
            if (!take(line,u"]")) fail("Unclosed ER entity alias");
            line = trim(line);
        }
        if (take(line,u"{")) {
            if (!line.empty()) fail("Unexpected text after ER attribute block opener");
            body = index;
            return;
        }
        if (!line.empty()) fail("Unexpected text after ER entity declaration");
    }
    std::optional<std::size_t> body;
    void attribute_line(View line) {
        if (line == u"}") { body.reset(); return; }
        if (line.find(u'}') != View::npos || line.find(u'{') != View::npos) fail("Invalid ER attribute block line");
        ErAttribute value;
        value.type = identifier(line);
        value.name = identifier(line);
        if (line.starts_with(u"PK") || line.starts_with(u"FK") || line.starts_with(u"UK")) {
            auto key = identifier(line);
            if (key != u"PK" && key != u"FK" && key != u"UK") fail("Invalid ER attribute key");
            value.keys.push_back(std::move(key));
        }
        if (!line.empty()) value.comment = quoted(line);
        if (!line.empty()) fail("Unexpected text after ER attribute");
        if (++attribute_count > limits.attributes) fail("ER attribute limit exceeded");
        result.entities[*body].attributes.push_back(std::move(value));
    }
    static std::pair<Text,Text> endpoint(View marker) {
        if (marker == u"||") return {u"ONLY_ONE",u"ONLY_ONE"};
        if (marker == u"|o" || marker == u"o|") return {u"ZERO_OR_ONE",u"ZERO_OR_ONE"};
        if (marker == u"}|" || marker == u"|{") return {u"ONE_OR_MORE",u"ONE_OR_MORE"};
        if (marker == u"}o" || marker == u"o{") return {u"ZERO_OR_MORE",u"ZERO_OR_MORE"};
        fail("Unsupported ER relationship cardinality");
    }
    void relationship(View line) {
        const auto name_a = identifier(line);
        View marker_a;
        const auto separator = line.find(u"--");
        const auto dashed = line.find(u"..");
        const auto at = std::min(separator, dashed);
        if (at == View::npos) fail("Expected ER relationship connector");
        marker_a = trim(line.substr(0,at));
        if (marker_a.size() != 2) fail("Expected ER cardinality marker");
        const bool identifying = separator < dashed;
        line = line.substr(at + 2);
        const auto end_marker = line.find_first_of(u" \t");
        View marker_b = line.substr(0,end_marker);
        if (marker_b.size() != 2) fail("Expected ER cardinality marker");
        line = end_marker == View::npos ? View{} : trim(line.substr(end_marker));
        const auto name_b = identifier(line);
        if (name_b.empty()) fail("ER relationship is missing its target entity");
        Text label;
        if (take(line,u":")) { label = Text{trim(line)}; line = {}; }
        if (!line.empty()) fail("Unexpected text after ER relationship");
        const auto [a_card,_a] = endpoint(marker_a);
        const auto [b_card,_b] = endpoint(marker_b);
        const auto ia = entity(name_a), ib = entity(name_b);
        if (result.relationships.size() >= limits.relationships) fail("ER relationship limit exceeded");
        // Mermaid's ERDB stores cardinalities in visual edge order: the
        // right-hand marker is cardA and the left-hand marker is cardB.
        result.relationships.push_back({result.entities[ia].id,label,result.entities[ib].id,
            identifying ? u"IDENTIFYING" : u"NON_IDENTIFYING", b_card, a_card, label});
    }
    void statement(View line) {
        if (body) { attribute_line(line); return; }
        if (line.starts_with(u"accTitle:")) { result.acc_title = trim(line.substr(9)); return; }
        if (line.starts_with(u"accDescr:")) { result.acc_description = trim(line.substr(9)); return; }
        if (take(line,u"direction ")) {
            if (line != u"TB" && line != u"BT" && line != u"LR" && line != u"RL") fail("Unsupported ER direction");
            result.direction = line; return;
        }
        // A colon denotes a relationship; otherwise this is an entity declaration.
        if (line.find(u"--") != View::npos || line.find(u"..") != View::npos) relationship(line);
        else declaration(line);
    }
public:
    explicit Parser(ErLimits value) : limits(std::move(value)) {}
    ErInput run(PreparedDiagram& prepared) {
        if (prepared.title) result.title = *prepared.title;
        const auto encoded = encode_diagram_source(prepared.code.cleaned);
        View source = encoded; bool header = false, content = false;
        while (!source.empty()) {
            const auto end = source.find(u'\n');
            const auto line = trim(source.substr(0,end));
            if (!line.empty() && !line.starts_with(u"%%")) {
                if (!header) {
                    if (line != u"erDiagram") fail("Expected ER diagram header");
                    header = true;
                } else { statement(line); content = true; }
            }
            if (end == View::npos) break;
            source.remove_prefix(end + 1);
        }
        if (!header || !content) fail("Expected ER diagram statements");
        if (body) fail("Unclosed ER attribute block");
        result.effective_config = std::move(prepared.effective_config);
        return std::move(result);
    }
};
}

std::expected<ErInput,std::string> prepare_er(
    std::u16string_view source, const ConfigSession& session, ErLimits limits) {
    auto prepared = prepare_diagram(source,session,limits.frontend);
    if (!prepared) return std::unexpected(prepared.error());
    if (prepared->type != u"er") return std::unexpected("Expected an ER diagram");
    try { return Parser{std::move(limits)}.run(*prepared); }
    catch (const std::exception& error) { return std::unexpected(std::string{error.what()}); }
}
}
