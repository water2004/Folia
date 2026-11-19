module folia.mermaid.cynefin;
import std;
namespace folia::mermaid {
namespace {
std::u16string_view trim(std::u16string_view s) {
    auto first=s.find_first_not_of(u" \t\r\n");
    return first==s.npos?std::u16string_view{}:s.substr(first,s.find_last_not_of(u" \t\r\n")-first+1);
}
std::optional<CynefinDomainName> domain_name(std::u16string_view s) {
    if(s==u"complex")return CynefinDomainName::complex;
    if(s==u"complicated")return CynefinDomainName::complicated;
    if(s==u"clear")return CynefinDomainName::clear;
    if(s==u"chaotic")return CynefinDomainName::chaotic;
    if(s==u"confusion")return CynefinDomainName::confusion;
    return {};
}
std::u16string unquote(std::u16string_view s) {
    s=trim(s);if(s.size()<2||(s.front()!=u'"'&&s.front()!=u'\'')||s.back()!=s.front())throw std::runtime_error("Expected quoted Cynefin label");
    std::u16string out;for(std::size_t i=1;i+1<s.size();++i){if(s[i]==u'\\'&&i+2<s.size())++i;out+=s[i];}return out;
}
}
std::expected<CynefinDocument,std::string> parse_cynefin(std::u16string_view source) {
    try {
        CynefinDocument doc;std::optional<std::size_t> current;
        std::size_t start=0,line_no=0;bool header=false;
        while(start<=source.size()) {
            auto end=source.find(u'\n',start);if(end==source.npos)end=source.size();
            auto line=trim(source.substr(start,end-start));++line_no;start=end+1;
            if(line.empty()||line.starts_with(u"%%"))continue;
            if(!header){if(line==u"cynefin-beta"||line==u"cynefin-beta:"){header=true;continue;}throw std::runtime_error("Expected cynefin-beta header");}
            if(line.starts_with(u"title ")){doc.title=std::u16string(trim(line.substr(6)));current.reset();continue;}
            if(line.starts_with(u"accTitle:")){doc.acc_title=std::u16string(trim(line.substr(9)));current.reset();continue;}
            if(line.starts_with(u"accDescr:")){doc.acc_description=std::u16string(trim(line.substr(9)));current.reset();continue;}
            if(line.starts_with(u"accDescr {")&&line.back()==u'}'){doc.acc_description=std::u16string(trim(line.substr(10,line.size()-11)));current.reset();continue;}
            if(line.front()==u'"'||line.front()==u'\'') {
                if(!current)throw std::runtime_error("Cynefin label must follow a domain");
                doc.domains[*current].items.push_back({unquote(line)});continue;
            }
            auto arrow=line.find(u"-->");
            if(arrow!=line.npos) {
                auto from=domain_name(trim(line.substr(0,arrow)));auto rest=trim(line.substr(arrow+3));auto colon=rest.find(u':');
                auto to=domain_name(trim(rest.substr(0,colon)));
                if(!from||!to)throw std::runtime_error("Unknown Cynefin transition domain");
                std::optional<std::u16string> label;
                if(colon!=rest.npos)label=unquote(rest.substr(colon+1));
                if(*from!=*to)doc.transitions.push_back({*from,*to,std::move(label)});
                current.reset();continue;
            }
            if(auto name=domain_name(line)) {
                auto it=std::ranges::find(doc.domains,*name,&CynefinDomain::name);
                if(it==doc.domains.end()){doc.domains.push_back({*name,{}});current=doc.domains.size()-1;}
                else {it->items.clear();current=static_cast<std::size_t>(it-doc.domains.begin());}
                continue;
            }
            throw std::runtime_error("Unsupported Cynefin statement at line "+std::to_string(line_no));
        }
        if(!header)throw std::runtime_error("Expected cynefin-beta header");
        return doc;
    } catch(const std::exception& e){return std::unexpected(e.what());}
}
}
