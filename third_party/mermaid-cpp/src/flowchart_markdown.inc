// Common inline emphasis from Mermaid handle-markdown-text::markdownToHTML.
// Delimiter runs retain unmatched markers and escapes as literal display text.
// This is the node/edge HTML-label boundary, not a general Markdown document API.
std::u16string markdown_label_html(std::u16string_view source) {
    const auto space=[](char16_t c){return c==0||c==u' '||c==u'\t'||c==u'\n'||c==u'\r';};
    const auto punctuation=[](char16_t c) {
        return c!=0 && std::u16string_view(u"!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~").find(c)!=std::u16string_view::npos;
    };
    struct Delimiter {std::size_t start,count,original;char16_t marker;bool open,close;};
    std::vector<Delimiter> stack;
    std::vector<std::optional<std::u16string>> replacements(source.size());
    for(std::size_t i=0;i<source.size();) {
        if(source[i]==u'\\' && i+1<source.size() && punctuation(source[i+1])) {
            const auto number=std::to_string(static_cast<unsigned>(source[i+1]));
            replacements[i]=u"&#"+std::u16string(number.begin(),number.end())+u";";
            replacements[i+1]=u"";i+=2;continue;
        }
        if(source[i]==u'<') {
            const auto end=source.find(u'>',i+1);
            if(end!=source.npos) {i=end+1;continue;}
        }
        const auto marker=source[i];
        if(marker!=u'*'&&marker!=u'_') {++i;continue;}
        const auto start=i;
        while(i<source.size()&&source[i]==marker) ++i;
        const auto before=start?source[start-1]:char16_t{},after=i<source.size()?source[i]:char16_t{};
        const bool left=!space(after)&&(!punctuation(after)||space(before)||punctuation(before));
        const bool right=!space(before)&&(!punctuation(before)||space(after)||punctuation(after));
        Delimiter current{start,i-start,i-start,marker,
            left&&(marker==u'*'||!right||punctuation(before)),
            right&&(marker==u'*'||!left||punctuation(after))};
        while(current.close && current.count) {
            auto match=stack.size();
            for(auto k=stack.size();k>0;--k) {
                const auto& candidate=stack[k-1];
                if(candidate.marker!=marker||!candidate.open||!candidate.count) continue;
                // A run that can both open and close obeys the rule of three.
                if((current.open||candidate.close) && (candidate.original+current.original)%3==0 &&
                    (candidate.original%3!=0||current.original%3!=0)) continue;
                match=k-1;break;
            }
            if(match==stack.size()) break;
            auto& opener=stack[match];
            const std::size_t count=opener.count>=2 && current.count>=2?2:1;
            const auto open=opener.start+opener.count-count,close=current.start;
            replacements[open]=count==2?u"<strong>":u"<em>";
            replacements[close]=count==2?u"</strong>":u"</em>";
            if(count==2) {replacements[open+1]=u"";replacements[close+1]=u"";}
            opener.count-=count;current.start+=count;current.count-=count;
            stack.resize(match+1);
        }
        if(current.count && current.open) stack.push_back(current);
    }
    std::u16string result;
    for(std::size_t i=0;i<source.size();++i) {
        if(replacements[i]) result+=*replacements[i];
        else result+=source[i];
    }
    return result;
}
