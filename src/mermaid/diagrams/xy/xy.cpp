module folia.mermaid.xy;
import std;
namespace folia::mermaid {
namespace {
bool ws(char16_t c){return c==u' '||c==u'\t'||c==u'\r';}
std::u16string trim(std::u16string_view s){while(!s.empty()&&(ws(s.front())||s.front()==u'\n'))s.remove_prefix(1);while(!s.empty()&&(ws(s.back())||s.back()==u'\n'))s.remove_suffix(1);return std::u16string(s);}
std::u16string lower(std::u16string s){for(auto& c:s)if(c>=u'A'&&c<=u'Z')c+=32;return s;}
struct Parser {
    std::u16string_view s;std::size_t pos=0;std::u16string t;bool quoted=false;
    explicit Parser(std::u16string_view source):s(source){next();}
    void next(){while(pos<s.size()&&ws(s[pos]))++pos;if(s.substr(pos,2)==u"%%"){auto e=s.find(u'\n',pos);pos=e==s.npos?s.size():e;}t.clear();quoted=false;if(pos==s.size())return;auto c=s[pos++];if(c==u'"'){quoted=true;auto e=s.find(u'"',pos);if(e==s.npos)throw std::runtime_error("Incomplete XY string");t=s.substr(pos,e-pos);pos=e+1;if(t.starts_with(u'`')&&t.ends_with(u'`'))t=t.substr(1,t.size()-2);return;}if(s.substr(pos-1,3)==u"-->"){t=u"-->";pos+=2;return;}if(c==u'['||c==u']'||c==u','||c==u'\n'||c==u';'){t+=c;return;}t+=c;while(pos<s.size()&&!ws(s[pos])&&s[pos]!=u'\n'&&s[pos]!=u';'&&s[pos]!=u'['&&s[pos]!=u']'&&s[pos]!=u','&&s[pos]!=u'"'&&s.substr(pos,3)!=u"-->")t+=s[pos++];}
    bool take(std::u16string_view v){if(quoted||lower(t)!=v)return false;next();return true;}
    void require(std::u16string_view v){if(!take(v))throw std::runtime_error("Unexpected XY token");}
    bool numeric()const{return !quoted&&!t.empty()&&(t[0]==u'+'||t[0]==u'-'||t[0]==u'.'||(t[0]>=u'0'&&t[0]<=u'9'));}
    double number(){if(!numeric())throw std::runtime_error("Expected XY number");std::string a(t.begin(),t.end());if(a.starts_with('+'))a.erase(0,1);double v;auto r=std::from_chars(a.data(),a.data()+a.size(),v);if(r.ec!=std::errc{}||r.ptr!=a.data()+a.size()||!std::isfinite(v))throw std::runtime_error("Invalid XY number");next();return v;}
    std::u16string text(bool axis=false){if(quoted){auto out=trim(t);next();return out;}std::u16string out;while(!t.empty()&&t!=u"\n"&&t!=u";"&&t!=u"["&&t!=u"]"&&t!=u","&&t!=u"-->"&&!(axis&&numeric())){auto l=lower(t);if(l==u"horizontal"||l==u"vertical"||l==u"bar"||l==u"line")break;out+=t;next();}if(out.empty())throw std::runtime_error("Expected XY text");return out;}
    std::u16string metadata(bool block){auto start=pos;while(start<s.size()&&ws(s[start]))++start;if(start<s.size()&&(s[start]==u':'||s[start]==u'{')){block=s[start]==u'{';++start;}auto e=s.find(block?u'}':u'\n',start);if(e==s.npos){if(block)throw std::runtime_error("Incomplete XY description");e=s.size();}auto out=trim(s.substr(start,e-start));pos=e+(block?1:0);next();return out;}
};
}
std::expected<XYDocument,std::string> parse_xy(std::u16string_view source){try{
    Parser p(source);XYDocument d;bool has_x=false,has_y=false;d.y.min=std::numeric_limits<double>::infinity();d.y.max=-d.y.min;
    while(p.take(u"\n")||p.take(u";")){}if(!p.take(u"xychart-beta")&&!p.take(u"xychart"))throw std::runtime_error("Expected XY chart");if(p.take(u"horizontal"))d.horizontal=true;else if(p.take(u"vertical"))d.horizontal=false;
    while(!p.t.empty()){
        if(p.take(u"\n")||p.take(u";"))continue;
        if(p.take(u"title")){d.title=p.text();continue;}
        if(lower(p.t).starts_with(u"acctitle")){d.acc_title=p.metadata(false);continue;}
        if(lower(p.t).starts_with(u"accdescr")){d.acc_description=p.metadata(false);continue;}
        if(lower(p.t)==u"x-axis"||lower(p.t)==u"y-axis"){
            bool x=lower(p.t)==u"x-axis";p.next();auto& a=x?d.x:d.y;std::u16string title;
            if(!p.numeric()&&p.t!=u"[")title=p.text(true);
            if(p.take(u"[")){if(!x)throw std::runtime_error("XY y axis requires numeric range");a.band=true;a.categories.clear();do{a.categories.push_back(p.text());}while(p.take(u","));p.require(u"]");has_x=true;}
            else if(p.numeric()){a.band=false;a.min=p.number();p.require(u"-->");a.max=p.number();if(x)has_x=true;else has_y=true;}
            a.title=std::move(title);continue;
        }
        if(lower(p.t)==u"bar"||lower(p.t)==u"line"){
            XYPlot plot;plot.bar=lower(p.t)==u"bar";p.next();if(p.t!=u"[")plot.title=p.text();p.require(u"[");
            do{XYPoint point;point.y=p.number();if(p.quoted){point.label=trim(p.t);p.next();}plot.points.push_back(std::move(point));}while(p.take(u","));p.require(u"]");
            if(!has_x){d.x.band=false;d.x.min=1;d.x.max=double(plot.points.size());has_x=true;}
            if(d.x.band&&plot.points.size()>d.x.categories.size())plot.points.resize(d.x.categories.size());
            for(std::size_t i=0;i<plot.points.size();++i){auto& point=plot.points[i];if(d.x.band)point.category=d.x.categories[i];else point.x=d.x.min+(plot.points.size()==1?0:i*(d.x.max-d.x.min)/(plot.points.size()-1));if(!has_y){d.y.min=std::min(d.y.min,point.y);d.y.max=std::max(d.y.max,point.y);}}
            d.plots.push_back(std::move(plot));continue;
        }
        throw std::runtime_error("Unknown XY statement");
    }return d;
}catch(const std::runtime_error& e){return std::unexpected(e.what());}}
}
