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
    // Bound the standard regex engine, including malformed whitespace runs.
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
    // The expressions mirror Khroma's accepted grammar (including its exponent
    // and sign differences). Do not broaden these to browser CSS parsing.
    static const std::regex rgb(R"(^rgba?\(\s*?(-?(?:\d+(?:\.\d+)?|(?:\.\d+))(?:e\d+)?(%?))\s*?(?:,|\s)\s*?(-?(?:\d+(?:\.\d+)?|(?:\.\d+))(?:e\d+)?(%?))\s*?(?:,|\s)\s*?(-?(?:\d+(?:\.\d+)?|(?:\.\d+))(?:e\d+)?(%?))(?:\s*?(?:,|\/)\s*?\+?(-?(?:\d+(?:\.\d+)?|(?:\.\d+))(?:e\d+)?(%?)))?\s*?\)$)",std::regex::icase);
    static const std::regex hsl(R"(^hsla?\(\s*?(-?(?:\d+(?:\.\d+)?|(?:\.\d+))(?:e-?\d+)?(?:deg|grad|rad|turn)?)\s*?(?:,|\s)\s*?(-?(?:\d+(?:\.\d+)?|(?:\.\d+))(?:e-?\d+)?%)\s*?(?:,|\s)\s*?(-?(?:\d+(?:\.\d+)?|(?:\.\d+))(?:e-?\d+)?%)(?:\s*?(?:,|\/)\s*?\+?(-?(?:\d+(?:\.\d+)?|(?:\.\d+))(?:e-?\d+)?(%)?))?\s*?\)$)",std::regex::icase);
    std::smatch match;
    const auto numeric=[](const std::ssub_match& m) {
        auto str=m.str();
        // from_chars has locale-independent binary64 conversion; suffixes are
        // intentional (parseFloat). Overflow has JS's infinity semantics.
        double d{}; auto p=std::from_chars(str.data(),str.data()+str.size(),d);
        if(p.ec==std::errc::result_out_of_range) return str.starts_with('-')?-std::numeric_limits<double>::infinity():std::numeric_limits<double>::infinity();
        if(p.ec!=std::errc{}) throw std::runtime_error("Invalid color number");
        return d;
    };
    if(std::regex_match(text,match,rgb)) {
        result.r=clamp(numeric(match[1])*(match[2].length()?2.55:1),255);
        result.g=clamp(numeric(match[3])*(match[4].length()?2.55:1),255);
        result.b=clamp(numeric(match[5])*(match[6].length()?2.55:1),255);
        result.a=match[7].matched?clamp(numeric(match[7])/(match[8].length()?100:1),1):1;
        return result;
    }
    if(std::regex_match(text,match,hsl)) {
        result.h=numeric(match[1]); const auto hue=match[1].str();
        // Upstream regex is case-insensitive but its unit switch is not.
        if(hue.ends_with("grad")) result.h*=0.9;
        else if(hue.ends_with("rad")) result.h=result.h*180/std::numbers::pi;
        else if(hue.ends_with("turn")) result.h*=360;
        result.h=std::fmod(result.h,360);
        result.s=clamp(numeric(match[2]),100); result.l=clamp(numeric(match[3]),100);
        result.a=match[4].matched?clamp(numeric(match[4])/(match[5].length()?100:1),1):1;
        result.hsl=true;
        return result;
    }
    throw std::runtime_error("Unsupported color format");
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
