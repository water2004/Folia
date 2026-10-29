module folia.mermaid.quadrant;
import std;

namespace folia::mermaid {
namespace {
std::u16string_view trim(std::u16string_view s) {
    const auto first=s.find_first_not_of(u" \t\r\n");
    if(first==s.npos)return {};
    return s.substr(first,s.find_last_not_of(u" \t\r\n")-first+1);
}
std::u16string lower(std::u16string_view s) {
    std::u16string out{s};for(auto& c:out)if(c>=u'A'&&c<=u'Z')c+=u'a'-u'A';return out;
}
std::u16string label(std::u16string_view s) {
    s=trim(s);
    if(s.starts_with(u"\"`")&&s.ends_with(u"`\""))s=s.substr(2,s.size()-4);
    else if(s.starts_with(u'"')&&s.ends_with(u'"')&&s.size()>1)s=s.substr(1,s.size()-2);
    else if(s.find(u'"')!=s.npos)throw std::runtime_error("Incomplete Quadrant label");
    s=trim(s);
    if(s.empty())throw std::runtime_error("Quadrant label is required");
    // This renderer writes text nodes, just like the original Quadrant renderer.
    // Rich HTML sanitization is not part of the native plain-label contract.
    if(s.find(u'<')!=s.npos)throw std::runtime_error("Quadrant labels must be plain text");
    return std::u16string{s};
}
double decimal(std::u16string_view s) {
    s=trim(s);std::string bytes;
    for(auto c:s){if(c>127)throw std::runtime_error("Invalid Quadrant number");bytes+=static_cast<char>(c);}
    double value{};const auto p=std::from_chars(bytes.data(),bytes.data()+bytes.size(),value);
    if(p.ec!=std::errc{}||p.ptr!=bytes.data()+bytes.size()||!std::isfinite(value))throw std::runtime_error("Invalid Quadrant number");
    return value;
}
bool digits(std::u16string_view s){return !s.empty()&&std::ranges::all_of(s,[](auto c){return c>=u'0'&&c<=u'9';});}
QuadrantStyle styles(std::u16string_view s) {
    QuadrantStyle out;s=trim(s);
    while(!s.empty()) {
        auto comma=s.find(u',');auto item=trim(s.substr(0,comma));auto colon=item.find(u':');
        if(colon==item.npos)throw std::runtime_error("Invalid Quadrant point style");
        auto key=trim(item.substr(0,colon)),value=trim(item.substr(colon+1));
        if(key==u"radius") {
            if(!digits(value))throw std::runtime_error("Quadrant radius requires an integer");
            out.radius=decimal(value);
        } else if(key==u"stroke-width") {
            if(!value.ends_with(u"px")||!digits(value.substr(0,value.size()-2)))throw std::runtime_error("Quadrant stroke-width requires pixels");
            out.stroke_width=value;
        } else if(key==u"color"||key==u"stroke-color") {
            auto hex=value;if(hex.starts_with(u'#'))hex.remove_prefix(1);
            if((hex.size()!=3&&hex.size()!=6)||!std::ranges::all_of(hex,[](auto c){return (c>=u'0'&&c<=u'9')||(c>=u'A'&&c<=u'F')||(c>=u'a'&&c<=u'f');}))throw std::runtime_error("Quadrant color requires a hex code");
            (key==u"color"?out.color:out.stroke_color)=value;
        } else throw std::runtime_error("Unsupported Quadrant point style");
        if(comma==s.npos)break;s=trim(s.substr(comma+1));
    }
    return out;
}
void axis(std::array<std::u16string,2>& out,std::u16string_view s) {
    bool quoted=false;std::size_t arrow=s.npos,end=s.npos;
    for(std::size_t i=0;i<s.size();++i) {
        if(s[i]==u'"')quoted=!quoted;
        if(!quoted&&s.substr(i,2)==u"--") {auto j=i+2;while(j<s.size()&&s[j]==u'-')++j;if(j<s.size()&&s[j]==u'>'){arrow=i;end=j+1;break;}}
    }
    out[0]=label(s.substr(0,arrow));
    if(arrow!=s.npos) {
        // The grammar appends an arrow with spaces; textSanitizer trims it.
        if(trim(s.substr(end)).empty())out[0]+=u" ⟶";else out[1]=label(s.substr(end));
    }
}
}
std::expected<QuadrantDocument,std::string> parse_quadrant(std::u16string_view source) {
    try {
        QuadrantDocument out;bool header=false;
        for(std::size_t pos=0;pos<source.size();) {
            auto end=pos;bool quote=false;
            for(;end<source.size();++end){if(source[end]==u'"')quote=!quote;if(!quote&&(source[end]==u'\n'||source[end]==u';'))break;}
            auto line=trim(source.substr(pos,end-pos));pos=end<source.size()?end+1:end;
            if(line.empty()||line.starts_with(u"%%"))continue;
            const auto key=lower(line);
            if(!header) {if(key!=u"quadrantchart")throw std::runtime_error("Expected quadrantChart header");header=true;continue;}
            if(key.starts_with(u"title ")){out.title=trim(line.substr(6));continue;}
            if(key.starts_with(u"acctitle:")){out.acc_title=trim(line.substr(9));continue;}
            if(key.starts_with(u"accdescr:")){out.acc_description=trim(line.substr(9));continue;}
            if(key.starts_with(u"accdescr")) {
                auto open=line.find(u'{');if(open==line.npos)throw std::runtime_error("Invalid Quadrant accessibility description");
                const auto start=static_cast<std::size_t>(line.data()-source.data())+open+1;
                const auto close=source.find(u'}',start);if(close==source.npos)throw std::runtime_error("Incomplete Quadrant accessibility description");
                out.acc_description=trim(source.substr(start,close-start));pos=close+1;continue;
            }
            if(key.starts_with(u"x-axis ")){axis(out.x_axis,line.substr(7));continue;}
            if(key.starts_with(u"y-axis ")){axis(out.y_axis,line.substr(7));continue;}
            if(key.starts_with(u"quadrant-")&&key.size()>10&&key[9]>=u'1'&&key[9]<=u'4'&&key[10]==u' ') {
                out.quadrants[key[9]-u'1']=label(line.substr(11));continue;
            }
            if(key.starts_with(u"classdef ")) {
                auto rest=trim(line.substr(9));auto split=rest.find_first_of(u" \t");
                if(split==rest.npos)throw std::runtime_error("Quadrant classDef requires styles");
                out.classes[std::u16string(rest.substr(0,split))]=styles(rest.substr(split+1));continue;
            }
            const auto bracket=line.find(u'[');const auto close=line.find(u']',bracket);
            if(bracket==line.npos||close==line.npos)throw std::runtime_error("Expected Quadrant point coordinates");
            auto name=trim(line.substr(0,bracket));if(!name.ends_with(u':'))throw std::runtime_error("Expected colon before Quadrant point");name=trim(name.substr(0,name.size()-1));
            QuadrantPoint point;auto cls=name.rfind(u":::");
            if(cls!=name.npos){point.class_name=trim(name.substr(cls+3));name=trim(name.substr(0,cls));if(point.class_name.empty())throw std::runtime_error("Quadrant point class is required");}
            point.text=label(name);auto coords=line.substr(bracket+1,close-bracket-1);auto comma=coords.find(u',');
            if(comma==coords.npos)throw std::runtime_error("Quadrant point requires x and y");
            auto coordinate=[](std::u16string_view value){value=trim(value);if(value!=u"0"&&value!=u"1"&&!(value.starts_with(u"0.")&&digits(value.substr(2))))throw std::runtime_error("Quadrant coordinates require values from 0 to 1");return decimal(value);};
            point.x=coordinate(coords.substr(0,comma));point.y=coordinate(coords.substr(comma+1));point.style=styles(line.substr(close+1));
            out.points.insert(out.points.begin(),std::move(point));
        }
        if(!header)throw std::runtime_error("Expected quadrantChart header");return out;
    }catch(const std::runtime_error& e){return std::unexpected(e.what());}
}
}
