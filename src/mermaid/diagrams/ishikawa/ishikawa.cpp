module folia.mermaid.ishikawa;
import std;
namespace folia::mermaid {
namespace {
std::u16string trim(std::u16string_view s) {
    auto a=s.find_first_not_of(u" \t\r"); if(a==s.npos)return {};
    auto b=s.find_last_not_of(u" \t\r"); return std::u16string(s.substr(a,b-a+1));
}
struct Line { std::size_t indent; std::u16string text; };
}
std::expected<IshikawaDocument,std::string> parse_ishikawa(std::u16string_view source) {
    try {
        std::vector<Line> lines;
        std::size_t pos=0;
        while(pos<=source.size()) {
            auto end=source.find(u'\n',pos); if(end==source.npos)end=source.size();
            auto raw=source.substr(pos,end-pos); std::size_t indent=0;
            while(indent<raw.size()&&(raw[indent]==u' '||raw[indent]==u'\t'))++indent;
            auto body=trim(raw.substr(indent));
            if(!body.empty()&&!body.starts_with(u"%%")) lines.push_back({indent,std::move(body)});
            if(end==source.size())break; pos=end+1;
        }
        if(lines.empty())return std::unexpected("Ishikawa diagram has no effect label");
        auto header=lines.front().text;
        if(header!=u"ishikawa"&&header!=u"ishikawa-beta")return std::unexpected("Expected ishikawa diagram header");
        if(lines.size()<2)return std::unexpected("Ishikawa diagram has no effect label");
        IshikawaDocument doc; doc.root.text=lines[1].text;
        std::vector<std::pair<std::size_t,IshikawaNode*>> stack{{0,&doc.root}};
        std::optional<std::size_t> base;
        for(std::size_t i=2;i<lines.size();++i) {
            base=base.value_or(lines[i].indent);
            auto level=lines[i].indent>=*base?lines[i].indent-*base+1:1;
            while(stack.size()>1&&stack.back().first>=level)stack.pop_back();
            auto* parent=stack.back().second;
            parent->children.push_back({std::move(lines[i].text),{}});
            stack.emplace_back(level,&parent->children.back());
        }
        return doc;
    } catch(const std::exception& e) {return std::unexpected(e.what());}
}
}
