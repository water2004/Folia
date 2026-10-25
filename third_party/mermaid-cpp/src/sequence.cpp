// Port of Sequence Jison productions and SequenceDB actor/message operations.
// Original grammar also credits Andrew Brampton (2012-2013), BSD-2-Clause;
// see docs/mermaid-cpp/README.md for attribution and the supported boundary.
module folia.mermaid.sequence;
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
bool take(View& line, View keyword) {
    if (line.size() < keyword.size() || lower(line.substr(0,keyword.size())) != keyword ||
        (line.size() > keyword.size() && !space(line[keyword.size()]))) return false;
    line = trim(line.substr(keyword.size())); return true;
}
class Parser {
    SequenceInput result;
    SequenceLimits limits;
    std::map<Text,std::size_t,std::less<>> actor_index;
    std::map<Text,int,std::less<>> activations;
    struct Region { View keyword; SequenceKind start, end; View section; SequenceKind branch; };
    static constexpr Region region_types[] = {
        {u"loop",SequenceKind::loop_start,SequenceKind::loop_end,{},{}},
        {u"opt",SequenceKind::opt_start,SequenceKind::opt_end,{},{}},
        {u"alt",SequenceKind::alt_start,SequenceKind::alt_end,u"else",SequenceKind::alt_else},
        {u"par",SequenceKind::par_start,SequenceKind::par_end,u"and",SequenceKind::par_and},
        {u"par_over",SequenceKind::par_over_start,SequenceKind::par_end,u"and",SequenceKind::par_and},
        {u"critical",SequenceKind::critical_start,SequenceKind::critical_end,u"option",SequenceKind::critical_option},
        {u"break",SequenceKind::break_start,SequenceKind::break_end,{},{}},
        {u"rect",SequenceKind::rect_start,SequenceKind::rect_end,{},{}},
    };
    std::vector<const Region*> regions;
    bool auto_wrap = false;
    std::size_t offset = 0;
    [[noreturn]] void fail(const char* message) const {
        throw std::runtime_error(std::string{message}+" at prepared UTF-16 offset "+std::to_string(offset));
    }
    struct Label { Text text; bool wrap; };
    Label label(View value) const {
        value = trim(value); bool wrap = auto_wrap;
        View prefix = value;
        if (prefix.starts_with(u':')) prefix.remove_prefix(1);
        if (prefix.starts_with(u"wrap:")) { value = prefix.substr(5); wrap = true; }
        else if (prefix.starts_with(u"nowrap:")) { value = prefix.substr(7); wrap = false; }
        return {Text{trim(value)},wrap};
    }
    Text actor(View value, std::optional<Label> description = {}, View type = u"participant") {
        value = trim(value);
        if (value.empty() || value.find_first_of(u"<>:,+;@()\\/") != value.npos)
            fail("Expected a supported Sequence actor identifier");
        const auto found = actor_index.find(value);
        if (found != actor_index.end() && !description) return Text{value};
        SequenceActor a{Text{value},Text{value},description ? description->text : Text{value},
            Text{type},description ? description->wrap : auto_wrap};
        if (found != actor_index.end()) result.actors[found->second] = std::move(a);
        else {
            if (result.actors.size() >= limits.actors) fail("Sequence actor limit exceeded");
            actor_index.emplace(value,result.actors.size()); result.actors.push_back(std::move(a));
        }
        return Text{value};
    }
    void message(Text from, Text to, Label text, SequenceKind type, bool activate = false, int placement = -1) {
        if (result.messages.size() >= limits.messages) fail("Sequence message limit exceeded");
        if (type == SequenceKind::active_end && activations[from] < 1)
            fail("Trying to inactivate an inactive Sequence participant");
        if (type == SequenceKind::active_start) ++activations[from];
        if (type == SequenceKind::active_end) --activations[from];
        const auto id = std::to_string(result.messages.size());
        result.messages.push_back({Text{id.begin(),id.end()},std::move(from),std::move(to),
            std::move(text.text),type,text.wrap,activate,placement});
    }
    void statement(View line) {
        View rest = line;
        if (take(rest,u"autonumber")) {
            std::optional<double> start, step;
            const bool visible = lower(rest) != u"off";
            if (visible && !rest.empty()) {
                auto number = [&]() {
                    const auto end = rest.find_first_of(u" \t");
                    const auto token = rest.substr(0,end);
                    const auto dot = token.find(u'.');
                    if (token.empty() || token == u"." ||
                        token.find_first_not_of(u"0123456789.") != token.npos ||
                        (dot != token.npos && (token.find(u'.',dot+1) != token.npos ||
                            token.size()-dot-1 < 1 || token.size()-dot-1 > 2)))
                        fail("Expected a Sequence autonumber value with at most two decimal places");
                    double value = 0;
                    const std::string ascii{token.begin(),token.end()};
                    const auto parsed = std::from_chars(ascii.data(),ascii.data()+ascii.size(),value);
                    if (parsed.ec != std::errc{} || parsed.ptr != ascii.data()+ascii.size())
                        fail("Invalid Sequence autonumber value");
                    rest = end == rest.npos ? View{} : trim(rest.substr(end));
                    return value;
                };
                start = number(); step = rest.empty() ? 1 : number();
                if (!rest.empty()) fail("Unexpected Sequence autonumber argument");
            }
            message({},{},label({}),SequenceKind::autonumber);
            auto& event = result.messages.back();
            event.number_start=start; event.number_step=step; event.number_visible=visible;
            return;
        }
        for(const auto& region:region_types) {
            if(take(rest,region.keyword)) {
                message({},{},label(rest),region.start);
                regions.push_back(&region);
                return;
            }
        }
        if(take(rest,u"end")) {
            if(!rest.empty() || regions.empty()) fail("Unexpected Sequence region end");
            message({},{},label({}),regions.back()->end);
            regions.pop_back();
            return;
        }
        for(const auto keyword:{u"else",u"and",u"option"}) {
            if(take(rest,keyword)) {
                if(regions.empty() || regions.back()->section!=keyword)
                    fail("Unexpected Sequence region section");
                message({},{},label(rest),regions.back()->branch);
                return;
            }
        }
        bool human = take(rest,u"actor");
        if (human || take(rest,u"participant")) {
            const auto lowered = lower(rest);
            auto as = lowered.find(u" as ");
            if (as == Text::npos) actor(rest,{},human ? u"actor" : u"participant");
            else actor(rest.substr(0,as),label(rest.substr(as+4)),human ? u"actor" : u"participant");
            return;
        }
        if (take(rest,u"activate")) { message(Text{rest},{},label({}),SequenceKind::active_start); return; }
        if (take(rest,u"deactivate")) { message(Text{rest},{},label({}),SequenceKind::active_end); return; }
        if (take(rest,u"note")) {
            int placement = -1;
            if (take(rest,u"left of")) placement = 0;
            else if (take(rest,u"right of")) placement = 1;
            else if (take(rest,u"over")) placement = 2;
            else fail("Expected a Sequence note placement");
            auto colon = rest.find(u':');
            if (colon == rest.npos) fail("Expected a Sequence note colon");
            const auto ids = rest.substr(0,colon); const auto comma = ids.find(u',');
            if (comma != ids.npos && placement != 2) fail("Only over notes support two actors");
            auto from = actor(ids.substr(0,comma));
            auto to = comma == ids.npos ? from : actor(ids.substr(comma+1));
            message(std::move(from),std::move(to),label(rest.substr(colon+1)),SequenceKind::note,false,placement);
            return;
        }
        if (take(rest,u"title") || take(rest,u"title:")) { result.title = trim(rest); return; }
        const auto folded = lower(rest);
        if (folded.starts_with(u"acctitle:")) { result.acc_title = trim(rest.substr(9)); return; }
        if (folded.starts_with(u"accdescr:")) { result.acc_description = trim(rest.substr(9)); return; }
        struct Arrow { View spelling; SequenceKind kind; };
        static constexpr Arrow arrows[] = {
            {u"<<-->>",SequenceKind::bidirectional_dotted},{u"<<->>",SequenceKind::bidirectional_solid},
            {u"-->>",SequenceKind::dotted},{u"->>",SequenceKind::solid},
            {u"-->",SequenceKind::dotted_open},{u"->",SequenceKind::solid_open},
            {u"--x",SequenceKind::dotted_cross},{u"-x",SequenceKind::solid_cross},
            {u"--)",SequenceKind::dotted_point},{u"-)",SequenceKind::solid_point}};
        // Longest token at the first arrow position, as in the Jison lexer.
        auto colon = line.find(u':');
        if (colon == line.npos) fail("Expected a supported Sequence statement or message colon");
        const auto ends = line.substr(0,colon);
        std::size_t at = ends.npos; const Arrow* arrow = nullptr;
        for (const auto& candidate : arrows) {
            auto found = ends.find(candidate.spelling);
            if (found < at) { at = found; arrow = &candidate; }
        }
        if (!arrow) fail("Expected a supported Sequence arrow");
        auto from = actor(ends.substr(0,at));
        rest = trim(ends.substr(at+arrow->spelling.size()));
        char16_t activation = 0;
        if (!rest.empty() && (rest.front() == u'+' || rest.front() == u'-')) {
            activation = rest.front(); rest = trim(rest.substr(1));
        }
        auto to = actor(rest);
        message(from,to,label(line.substr(colon+1)),arrow->kind,activation == u'+');
        if (activation == u'+') message(to,{},label({}),SequenceKind::active_start);
        if (activation == u'-') message(from,{},label({}),SequenceKind::active_end);
    }
public:
    explicit Parser(SequenceLimits value) : limits(std::move(value)) {}
    SequenceInput run(PreparedDiagram& prepared) {
        result.title = prepared.title.value_or(Text{});
        const auto* cfg = prepared.effective_config.find(u"sequence");
        const auto* wrap = prepared.effective_config.find(u"wrap");
        if (!wrap && cfg) wrap = cfg->find(u"wrap");
        if (wrap) auto_wrap = wrap->kind == ConfigKind::boolean && wrap->boolean;
        const auto encoded = encode_diagram_source(prepared.code.cleaned);
        const View source = encoded;
        bool header = false;
        while (offset < source.size()) {
            const auto end = source.find_first_of(u"\n;",offset);
            View line = trim(source.substr(offset,end == source.npos ? source.size()-offset : end-offset));
            if (const auto comment = line.find(u'#'); comment != line.npos) line = trim(line.substr(0,comment));
            if (!line.empty() && !line.starts_with(u"%%")) {
                if (!header) {
                    if (lower(line) != u"sequencediagram") fail("Expected Sequence header");
                    header = true;
                } else statement(line);
            }
            if (end == source.npos) break;
            offset = end+1;
        }
        if (!header) fail("Expected Sequence header");
        if (!regions.empty()) fail("Unclosed Sequence control region");
        result.effective_config = std::move(prepared.effective_config);
        return std::move(result);
    }
};
}
std::expected<SequenceInput,std::string> prepare_sequence(
    std::u16string_view source, const ConfigSession& session, SequenceLimits limits) {
    auto prepared = prepare_diagram(source,session,limits.frontend);
    if (!prepared) return std::unexpected(prepared.error());
    if (prepared->type != u"sequence") return std::unexpected("Expected a Sequence diagram");
    try { return Parser{std::move(limits)}.run(*prepared); }
    catch (const std::exception& error) { return std::unexpected(std::string{error.what()}); }
}
}
