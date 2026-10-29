module folia.mermaid.flowchart_label;
import std;

namespace folia::mermaid {
namespace {
#include "flowchart_markdown.inc"
std::u16string decode_entity(std::u16string_view name) {
    int base=10;
    if(name.starts_with(u'#')) {
        name.remove_prefix(1);
        if(name.starts_with(u'x')||name.starts_with(u'X')) {base=16;name.remove_prefix(1);}
    }
    if(!name.empty() && (name.front()>=u'0'&&name.front()<=u'9' || base==16)) {
        std::string digits(name.begin(),name.end());std::uint32_t cp{};
        auto parsed=std::from_chars(digits.data(),digits.data()+digits.size(),cp,base);
        if(parsed.ec!=std::errc{}||parsed.ptr!=digits.data()+digits.size()||cp==0||cp>0x10ffff||
            (cp>=0xd800&&cp<=0xdfff)) throw std::runtime_error("Invalid Flowchart character entity");
        if(cp<0x10000) return std::u16string(1,static_cast<char16_t>(cp));
        cp-=0x10000;return {static_cast<char16_t>(0xd800+(cp>>10)),static_cast<char16_t>(0xdc00+(cp&1023))};
    }
    for(auto [key,value]:{std::pair{u"quot",u"\""},{u"amp",u"&"},{u"apos",u"'"},
        {u"lt",u"<"},{u"gt",u">"},{u"nbsp",u"\u00a0"},{u"copy",u"\u00a9"},{u"reg",u"\u00ae"}})
        if(name==key) return value;
    throw std::runtime_error("Unsupported named Flowchart entity; use its decimal code point");
}
}
static std::expected<std::vector<FlowchartLabelLine>,std::string> label_runs(
    std::u16string_view label,std::u16string_view type,bool svg) {
    try {
        if(type!=u"text"&&type!=u"string"&&type!=u"markdown")
            return std::unexpected("Flowchart styled labels require the rich-text adapter");
        std::u16string markdown;
        if(type==u"markdown") {markdown=markdown_label_html(label);label=markdown;}
        std::vector<FlowchartLabelLine> lines(1);
        std::vector<std::u16string> tags;
        bool strong=false,emphasis=false;
        bool pending_space=false;
        bool space_strong=false,space_emphasis=false;
        auto character=[&](char16_t c,bool bold,bool italic) {
            auto& line=lines.back();
            if(line.empty()||line.back().strong!=bold||line.back().emphasis!=italic)
                line.push_back({{},bold,italic});
            line.back().text+=c;
        };
        auto append=[&](std::u16string_view text) {
            for(auto c:text) {
                if(c==u' '||c==u'\t'||c==u'\r'||c==u'\f') {
                    if(!pending_space) {space_strong=strong;space_emphasis=emphasis;}
                    pending_space=!lines.back().empty();continue;
                }
                if(pending_space) character(u' ',svg?strong:space_strong,svg?emphasis:space_emphasis);
                pending_space=false;character(c,strong,emphasis);
            }
        };
        for(std::size_t i=0;i<label.size();) {
            auto rest=label.substr(i);std::size_t break_size=0;
            if(rest.starts_with(u'\n')) break_size=1;
            else if(type!=u"markdown"&&rest.starts_with(u"\\n")) break_size=2;
            else if(rest.starts_with(u'<')) {
                const auto end=rest.find(u'>');
                if(end!=rest.npos) {
                    std::u16string tag;
                    for(auto c:rest.substr(1,end-1)) if(c!=u' '&&c!=u'\t'&&c!=u'/')
                        tag+=c>=u'A'&&c<=u'Z'?c+u'a'-u'A':c;
                    if(tag==u"br") break_size=end+1;
                    else if(svg && type!=u"markdown") {
                        // nonMarkdownToLines keeps tags as literal separate words.
                        pending_space=!lines.back().empty();append(rest.substr(0,end+1));
                        pending_space=true;i+=end+1;continue;
                    } else if(tag==u"b"||tag==u"strong"||tag==u"i"||tag==u"em") {
                        if(rest[1]==u'/') {
                            if(tags.empty()||tags.back()!=tag)
                                return std::unexpected("Unbalanced Flowchart label formatting");
                            tags.pop_back();
                        } else tags.push_back(tag);
                        strong=std::ranges::any_of(tags,[](const auto& t){return t==u"b"||t==u"strong";});
                        emphasis=std::ranges::any_of(tags,[](const auto& t){return t==u"i"||t==u"em";});
                        if(svg) {
                            strong=!tags.empty() && (tags.back()==u"b"||tags.back()==u"strong");
                            emphasis=!tags.empty() && (tags.back()==u"i"||tags.back()==u"em");
                            pending_space=!lines.back().empty();
                        }
                        i+=end+1;continue;
                    } else return std::unexpected("Unsupported Flowchart label HTML tag");
                }
            }
            if(break_size) {lines.emplace_back();pending_space=false;i+=break_size;continue;}
            if(rest.starts_with(u'&')||rest.starts_with(u'#')) {
                auto end=rest.find(u';');
                if(end!=rest.npos && rest.substr(1,end-1).find_first_of(u" \t\n&<#")==rest.npos) {
                    append(decode_entity(rest.substr(1,end-1)));i+=end+1;continue;
                }
                if(rest.starts_with(u"&#") && end!=rest.npos) {
                    append(decode_entity(rest.substr(1,end-1)));i+=end+1;continue;
                }
            }
            append(rest.substr(0,1));++i;
        }
        if(!tags.empty()) return std::unexpected("Unclosed Flowchart label formatting");
        return lines;
    } catch(const std::runtime_error& e) {return std::unexpected(e.what());}
}
std::expected<std::vector<FlowchartLabelLine>,std::string> flowchart_label_runs(
    std::u16string_view label,std::u16string_view type) {return label_runs(label,type,false);}
std::expected<std::vector<FlowchartLabelLine>,std::string> flowchart_svg_label_runs(
    std::u16string_view label,std::u16string_view type) {return label_runs(label,type,true);}
std::expected<std::vector<std::u16string>,std::string> flowchart_label_lines(
    std::u16string_view label,std::u16string_view type) {
    auto runs=flowchart_label_runs(label,type);
    if(!runs) return std::unexpected(runs.error());
    std::vector<std::u16string> lines;
    for(const auto& line:*runs) {
        std::u16string text;
        for(const auto& run:line) text+=run.text;
        lines.push_back(std::move(text));
    }
    return lines;
}
}
