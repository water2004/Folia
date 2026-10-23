module folia.mermaid.theme_color;
import std;
import folia.mermaid.number;

namespace folia::mermaid::theme_detail {
namespace {
#include "theme_color_names.inc"
double clamp(double value, double maximum) {
    return value >= maximum ? maximum : value < 0 ? 0 : value;
}
double round10(double value) { return std::floor(value * 10000000000.0 + 0.5) / 10000000000.0; }
std::string number(double value, bool rounded = true) {
    if (rounded) value = round10(value);
    if (std::isnan(value)) return "NaN";
    if (std::isinf(value)) return value < 0 ? "-Infinity" : "Infinity";
    return *format_number(value);
}
std::u16string widen(std::string_view text) { return {text.begin(), text.end()}; }
struct Color {
    double r=0, g=0, b=0, h=0, s=0, l=0, a=1;
    bool hsl=false, changed=false;
    std::u16string original;
    void to_hsl() {
        if (hsl) return;
        const auto red=r/255, green=g/255, blue=b/255;
        const auto high=std::max({red,green,blue}), low=std::min({red,green,blue});
        const auto light=(high+low)/2, d=high-low;
        l=light*100;
        s=high==low?0:(light>0.5?d/(2-high-low):d/(high+low))*100;
        h=high==low?0:high==red?((green-blue)/d+(green<blue?6:0))*60:
            high==green?((blue-red)/d+2)*60:((red-green)/d+4)*60;
        hsl=true;
    }
    static double hue(double p, double q, double t) {
        if (t<0) t+=1;
        if (t>1) t-=1;
        if (t<1.0/6) return p+(q-p)*6*t;
        if (t<0.5) return q;
        if (t<2.0/3) return p+(q-p)*(2.0/3-t)*6;
        return p;
    }
    void to_rgb() {
        if (!hsl) return;
        if (s==0) r=g=b=l*2.55;
        else {
            const auto hh=h/360, ss=s/100, ll=l/100;
            const auto q=ll<0.5?ll*(1+ss):(ll+ss)-(ll*ss), p=2*ll-q;
            r=hue(p,q,hh+1.0/3)*255; g=hue(p,q,hh)*255; b=hue(p,q,hh-1.0/3)*255;
        }
        hsl=false;
    }
    std::u16string stringify() const {
        if (!changed && !original.empty()) return original;
        if (hsl) return widen(std::string(a<1?"hsla(":"hsl(")+number(h)+", "+number(s)+"%, "+number(l)+"%"+
            (a<1?", "+number(a,false):"")+")");
        if (a<1 || std::trunc(r)!=r || std::trunc(g)!=g || std::trunc(b)!=b)
            return widen(std::string(a<1?"rgba(":"rgb(")+number(r)+", "+number(g)+", "+number(b)+
                (a<1?", "+number(a):"")+")");
        constexpr char hex[]="0123456789abcdef";
        std::string text="#";
        for (double c : {r,g,b}) {
            const auto n=static_cast<unsigned>(std::floor(c+0.5));
            if (n>255) throw std::runtime_error("Invalid RGB channel");
            text+=hex[n>>4]; text+=hex[n&15];
        }
        return widen(text);
    }
};
bool js_space(char16_t c) {
    return (c>=9 && c<=13) || c==32 || c==0xa0 || c==0x1680 ||
        (c>=0x2000 && c<=0x200a) || c==0x2028 || c==0x2029 || c==0x202f || c==0x205f || c==0x3000 || c==0xfeff;
}
Color parse(std::u16string_view input) {
    // Bound literal work and temporary storage before scanning malformed input.
    if (input.size()>4096) throw std::runtime_error("Theme color literal exceeds 4096 UTF-16 units");
    std::string text;
    for (char16_t c : input) {
        if (js_space(c)) text+=' ';
        else if(c<128) text+=static_cast<char>(c);
        else throw std::runtime_error("Unsupported color format");
    }
    Color result; result.original=input;
    auto hex=text;
    std::string lower=text;
    for(auto& c:lower) if(c>='A' && c<='Z') c=static_cast<char>(c+32);
    for(const auto& [name,value]:color_names) if(lower==name) {
        hex=value; result.original=widen(value); break;
    }
    if (!hex.empty() && hex[0]=='#') {
        const auto length=hex.size()-1;
        if(length!=3 && length!=4 && length!=6 && length!=8) throw std::runtime_error("Unsupported color format");
        std::uint32_t n{};
        const auto converted=std::from_chars(hex.data()+1,hex.data()+hex.size(),n,16);
        if(converted.ec!=std::errc{} || converted.ptr!=hex.data()+hex.size()) throw std::runtime_error("Unsupported color format");
        const bool alpha=length%4==0, full=length>4;
        const unsigned bits=full?8:4, mask=full?255:15, multiplier=full?1:17;
        const unsigned offset=alpha?1:0;
        result.r=((n>>(bits*(offset+2)))&mask)*multiplier;
        result.g=((n>>(bits*(offset+1)))&mask)*multiplier;
        result.b=((n>>(bits*offset))&mask)*multiplier;
        result.a=alpha?(n&mask)*multiplier/255.0:1;
        return result;
    }
    // Linear scanner for the exact Khroma regex grammar. Whitespace and
    // malformed numeric runs never recurse through a standard regex engine.
    const bool hsl=lower.starts_with("hsl(") || lower.starts_with("hsla(");
    const bool rgb=lower.starts_with("rgb(") || lower.starts_with("rgba(");
    if(!hsl && !rgb) throw std::runtime_error("Unsupported color format");
    std::size_t pos=text.find('(')+1;
    const auto fail=[] { throw std::runtime_error("Unsupported color format"); };
    const auto spaces=[&] { const auto first=pos; while(pos<text.size() && text[pos]==' ') ++pos; return pos!=first; };
    const auto digit=[](char c) { return c>='0' && c<='9'; };
    struct Component { double value; bool percent; std::string unit; };
    const auto component=[&](bool hue, bool alpha) -> Component {
        if(alpha && pos<text.size() && text[pos]=='+') ++pos;
        const auto first=pos;
        if(pos<text.size() && text[pos]=='-') ++pos;
        const auto digitsStart=pos;
        while(pos<text.size() && digit(text[pos])) ++pos;
        const bool integral=pos!=digitsStart;
        if(pos<text.size() && text[pos]=='.') {
            ++pos; const auto fractionalStart=pos;
            while(pos<text.size() && digit(text[pos])) ++pos;
            if(pos==fractionalStart) fail();
        } else if(!integral) fail();
        const auto mantissaEnd=pos;
        int exponent=0;
        if(pos<text.size() && (text[pos]=='e' || text[pos]=='E')) {
            ++pos; bool negative=false;
            if(hsl && pos<text.size() && text[pos]=='-') { negative=true; ++pos; }
            const auto exponentStart=pos;
            while(pos<text.size() && digit(text[pos])) {
                exponent=std::min(100000,exponent*10+text[pos]-'0'); ++pos;
            }
            if(pos==exponentStart) fail();
            if(negative) exponent=-exponent;
        }
        double value{};
        const auto parsed=std::from_chars(text.data()+first,text.data()+pos,value);
        if(parsed.ec==std::errc::result_out_of_range) {
            // Distinguish underflow from overflow without a locale-dependent
            // strtod fallback. Count the first significant decimal exponent.
            const auto begin=first+(text[first]=='-'?1:0);
            int beforeDot=0, firstSignificant=-1, digitsSeen=0;
            bool dot=false;
            for(auto i=begin;i<mantissaEnd;++i) {
                if(text[i]=='.') { dot=true; continue; }
                if(!dot) ++beforeDot;
                if(firstSignificant<0 && text[i]!='0') firstSignificant=digitsSeen;
                ++digitsSeen;
            }
            const bool tiny=firstSignificant<0 || beforeDot-firstSignificant-1+exponent<0;
            value=tiny?0:std::numeric_limits<double>::infinity();
            if(text[first]=='-') value=-value;
        } else if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+pos) fail();
        bool percent=false; std::string unit;
        if(hue) {
            for(const auto suffix:{"deg","grad","rad","turn"}) {
                const std::string_view view=suffix;
                if(std::string_view(lower).substr(pos).starts_with(view)) {
                    unit=text.substr(pos,view.size()); pos+=view.size(); break;
                }
            }
        } else if(pos<text.size() && text[pos]=='%') { percent=true; ++pos; }
        return {value,percent,unit};
    };
    const auto separator=[&] {
        const auto spaced=spaces();
        if(pos<text.size() && text[pos]==',') ++pos;
        else if(!spaced) fail();
        spaces();
    };
    spaces(); const auto first=component(hsl,false); separator();
    const auto second=component(false,false); separator();
    const auto third=component(false,false); spaces();
    double alpha=1;
    if(pos<text.size() && (text[pos]==',' || text[pos]=='/')) {
        ++pos; spaces(); const auto a=component(false,true);
        alpha=clamp(a.value/(a.percent?100:1),1); spaces();
    }
    if(pos>=text.size() || text[pos++]!=')' || pos!=text.size()) fail();
    if(hsl) {
        if(!second.percent || !third.percent) fail();
        result.h=first.value;
        // The upstream unit match is case-insensitive, but its switch is not.
        if(first.unit=="grad") result.h*=0.9;
        else if(first.unit=="rad") result.h=result.h*180/std::numbers::pi;
        else if(first.unit=="turn") result.h*=360;
        result.h=std::fmod(result.h,360);
        result.s=clamp(second.value,100); result.l=clamp(third.value,100); result.hsl=true;
    } else {
        result.r=clamp(first.value*(first.percent?2.55:1),255);
        result.g=clamp(second.value*(second.percent?2.55:1),255);
        result.b=clamp(third.value*(third.percent?2.55:1),255);
    }
    result.a=alpha;
    return result;
}
}
std::u16string color_adjust(std::u16string_view input, const std::map<char,double>& adjustments) {
    auto color=parse(input);
    bool rgb=false,hsl=false;
    for(const auto& [key,amount]:adjustments) if(amount!=0) {
        rgb|=key=='r'||key=='g'||key=='b'; hsl|=key=='h'||key=='s'||key=='l';
    }
    if(rgb&&hsl) throw std::runtime_error("Cannot change both RGB and HSL channels");
    if(rgb) color.to_rgb();
    if(hsl) color.to_hsl();
    for(const auto& [key,amount]:adjustments) {
        if(amount==0 || std::isnan(amount)) continue; // JS falsy amount.
        switch(key) {
        case 'r': color.r=clamp(color.r+amount,255); break;
        case 'g': color.g=clamp(color.g+amount,255); break;
        case 'b': color.b=clamp(color.b+amount,255); break;
        case 'h': color.h=std::fmod(color.h+amount,360); break;
        case 's': color.s=clamp(color.s+amount,100); break;
        case 'l': color.l=clamp(color.l+amount,100); break;
        case 'a': color.a=clamp(color.a+amount,1); break;
        default: throw std::runtime_error("Unknown color channel");
        }
        color.changed=true;
    }
    return color.stringify();
}
std::u16string color_lighten(std::u16string_view input,double amount) {
    auto color=parse(input); color.to_hsl();
    const auto next=clamp(color.l+amount,100);
    if(color.l!=next) { color.l=next; color.changed=true; }
    return color.stringify();
}
std::u16string color_rgba(double r,double g,double b,double a) {
    Color color; color.r=clamp(r,255); color.g=clamp(g,255); color.b=clamp(b,255); color.a=clamp(a,1);
    return color.stringify();
}
std::u16string color_invert(std::u16string_view input) {
    auto color=parse(input); color.to_rgb();
    return color_rgba(255-color.r,255-color.g,255-color.b,color.a);
}
bool color_is_dark(std::u16string_view input) {
    auto color=parse(input); color.to_rgb();
    const auto linear=[](double c) { const auto n=c/255; return c>0.03928?std::pow((n+0.055)/1.055,2.4):n/12.92; };
    return !(round10(0.2126*linear(color.r)+0.7152*linear(color.g)+0.0722*linear(color.b))>=0.5);
}
}
