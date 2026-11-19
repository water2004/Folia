module folia.mermaid.radar;
import std;
namespace folia::mermaid {
namespace {
bool space(char16_t c){return c==u' '||c==u'\t'||c==u'\r';}
bool digit(char16_t c){return c>=u'0'&&c<=u'9';}
bool id(char16_t c){return digit(c)||(c>=u'a'&&c<=u'z')||(c>=u'A'&&c<=u'Z')||c==u'_';}
std::u16string normalize(std::u16string_view s,bool multiline=false) {
    std::u16string out;bool pending=false;
    for(auto c:s){if(space(c)){pending=!out.empty()&&out.back()!=u'\n';continue;}if(c==u'\n'){pending=false;if(multiline&&!out.empty()&&out.back()!=u'\n')out+=c;continue;}if(pending)out+=u' ';pending=false;out+=c;}
    while(!out.empty()&&out.back()==u'\n')out.pop_back();return out;
}
enum class Kind {end,newline,word,number,string,punctuation};
struct Token {Kind kind=Kind::end;std::u16string text;};
struct Parser {
    std::u16string_view source;std::size_t position=0;Token token;
    explicit Parser(std::u16string_view s):source(s){next();}
    void next() {
        while(position<source.size()&&space(source[position]))++position;
        if(source.substr(position,2)==u"%%"){position=source.find(u'\n',position);if(position==source.npos)position=source.size();}
        if(position==source.size()){token={};return;}
        auto c=source[position++];token.text.clear();
        if(c==u'\n'){token={Kind::newline,u"\n"};return;}
        if(c==u'"'||c==u'\'') {
            token.kind=Kind::string;
            while(position<source.size()) {auto v=source[position++];if(v==c)return;if(v==u'\\'){if(position==source.size())break;v=source[position++];if(v==u'n')v=u'\n';else if(v==u'r')v=u'\r';else if(v==u't')v=u'\t';else if(v==u'b')v=u'\b';else if(v==u'f')v=u'\f';}token.text+=v;}
            throw std::runtime_error("Incomplete Radar string");
        }
        if(id(c)) {
            token.text+=c;while(position<source.size()&&(id(source[position])||source[position]==u'-'))token.text+=source[position++];
            if(std::ranges::all_of(token.text,digit)) {
                token.kind=Kind::number;
                if(position<source.size()&&source[position]==u'.'){token.text+=source[position++];auto start=position;while(position<source.size()&&digit(source[position]))token.text+=source[position++];if(position==start)throw std::runtime_error("Invalid Radar decimal");}
            }else {token.kind=Kind::word;if(token.text.back()==u'-')throw std::runtime_error("Invalid Radar identifier");}
            return;
        }
        token={Kind::punctuation,std::u16string(1,c)};
    }
    bool take(std::u16string_view value){if(token.text!=value)return false;next();return true;}
    void require(std::u16string_view value){if(!take(value))throw std::runtime_error("Unexpected Radar token");}
    void lines(){while(token.kind==Kind::newline)next();}
    double number(){if(token.kind!=Kind::number)throw std::runtime_error("Radar numeric value is required");std::string s(token.text.begin(),token.text.end());double out{};auto p=std::from_chars(s.data(),s.data()+s.size(),out);if(p.ec!=std::errc{}||p.ptr!=s.data()+s.size()||!std::isfinite(out))throw std::runtime_error("Invalid Radar number");next();return out;}
    RadarAxis named() {
        if(token.kind!=Kind::word&&token.kind!=Kind::number)throw std::runtime_error("Radar name is required");
        RadarAxis out{token.text,token.text};next();
        if(take(u"[")){if(token.kind!=Kind::string)throw std::runtime_error("Radar label must be quoted");out.label=token.text;next();require(u"]");}
        return out;
    }
    std::u16string metadata(bool description) {
        // Current token is the metadata keyword; its following source is not
        // tokenized, preserving the common grammar's complete line/body.
        auto start=position;while(start<source.size()&&space(source[start]))++start;
        bool block=description&&start<source.size()&&source[start]==u'{';
        if(block)++start;else if(start<source.size()&&source[start]==u':')++start;
        auto end=source.find(block?u'}':u'\n',start);if(end==source.npos){if(block)throw std::runtime_error("Incomplete Radar accessibility description");end=source.size();}
        auto text=source.substr(start,end-start);auto comment=text.find(u"%%");if(!block&&comment!=text.npos)text=text.substr(0,comment);
        position=end+(block?1:0);next();return normalize(text,block);
    }
};
struct RawCurve { RadarCurve curve;std::vector<std::u16string> references; };
}
std::expected<RadarDocument,std::string> parse_radar(std::u16string_view source) {
    try {
        Parser p(source);RadarDocument out;std::vector<RawCurve> curves;p.lines();p.require(u"radar-beta");p.take(u":");
        while(p.token.kind!=Kind::end) {
            if(p.token.kind==Kind::newline){p.next();continue;}
            if(p.token.text==u"title"){out.title=p.metadata(false);continue;}
            if(p.token.text==u"accTitle"){out.acc_title=p.metadata(false);continue;}
            if(p.token.text==u"accDescr"){out.acc_description=p.metadata(true);continue;}
            if(p.take(u"axis")) {do{out.axes.push_back(p.named());}while(p.take(u","));continue;}
            if(p.take(u"curve")) {
                do {
                    auto named=p.named();RawCurve raw;raw.curve.name=named.name;raw.curve.label=named.label;p.require(u"{");p.lines();
                    const bool referenced=p.token.kind==Kind::word;
                    do {
                        p.lines();
                        if(referenced){if(p.token.kind!=Kind::word)throw std::runtime_error("Radar axis reference is required");raw.references.push_back(p.token.text);p.next();p.take(u":");}
                        raw.curve.entries.push_back(p.number());p.lines();
                    }while(p.take(u","));
                    p.require(u"}");curves.push_back(std::move(raw));
                }while(p.take(u","));continue;
            }
            do {
                auto option=p.token.text;p.next();
                if(option==u"showLegend"){if(p.token.text!=u"true"&&p.token.text!=u"false")throw std::runtime_error("Radar showLegend requires boolean");out.options.show_legend=p.token.text==u"true";p.next();}
                else if(option==u"graticule"){if(p.token.text!=u"circle"&&p.token.text!=u"polygon")throw std::runtime_error("Unknown Radar graticule");out.options.polygon=p.token.text==u"polygon";p.next();}
                else if(option==u"ticks")out.options.ticks=p.number();
                else if(option==u"max")out.options.max=p.number();
                else if(option==u"min")out.options.min=p.number();
                else throw std::runtime_error("Unknown Radar statement");
            }while(p.take(u","));
        }
        out.options.ticks=std::min(32.0,out.options.ticks);
        for(auto& raw:curves) {
            if(!raw.references.empty()) {
                for(const auto& reference:raw.references)if(std::ranges::none_of(out.axes,[&](const auto& axis){return axis.name==reference;}))throw std::runtime_error("Unknown Radar axis reference");
                auto values=std::move(raw.curve.entries);
                for(const auto& axis:out.axes){auto found=std::ranges::find(raw.references,axis.name);if(found==raw.references.end())throw std::runtime_error("Missing Radar entry for axis");raw.curve.entries.push_back(values[static_cast<std::size_t>(found-raw.references.begin())]);}
            }
            out.curves.push_back(std::move(raw.curve));
        }
        return out;
    }catch(const std::runtime_error& e){return std::unexpected(e.what());}
}
}
