#include "support/folia_test.hpp"
import folia.mermaid.theme_color;
import folia.mermaid.directives;
import folia.mermaid.number;
namespace {
using namespace boost::ut;
using namespace folia::mermaid;
using namespace folia::mermaid::theme_detail;
#include "theme_fixture.hpp"
suite theme_colors = [] {
    "mermaid_theme_colors_match_khroma"_test = [] {
        const auto directory=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{directory?directory:FOLIA_MERMAID_FIXTURE_DIR}/"theme-colors.tsv"};
        expect(fatal(input.is_open()));
        std::string line; std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty() && line.back()=='\r') line.pop_back();
            if(line.empty() || line.front()=='#') continue;
            std::vector<std::string_view> fields; std::string_view rest=line;
            while(true) { auto tab=rest.find('\t'); fields.push_back(rest.substr(0,tab)); if(tab==rest.npos) break; rest.remove_prefix(tab+1); }
            expect(fatal(fields.size()==4_u));
            const auto paint=json(decode(fields[1])).string;
            const auto arg=fields[2]=="U"?ConfigValue{}:json(decode(fields[2]));
            std::string result;
            try {
                ConfigValue value; value.kind=ConfigKind::string;
                if(fields[0]=="invert") value.string=color_invert(paint);
                else if(fields[0]=="isDark") { value.kind=ConfigKind::boolean; value.boolean=color_is_dark(paint); }
                else if(fields[0]=="adjust") {
                    std::map<char,double> adjustments;
                    for(const auto& [key,v]:arg.mapping) adjustments.emplace(static_cast<char>(key[0]),v.number);
                    value.string=color_adjust(paint,adjustments);
                } else if(fields[0]=="lighten" || fields[0]=="darken") {
                    auto amount=fields[2]=="U"?std::numeric_limits<double>::quiet_NaN():arg.number;
                    value.string=color_lighten(paint,fields[0]=="darken"?-amount:amount);
                } else throw std::logic_error("Unknown color fixture operation");
                result=snapshot(value);
            } catch(const std::runtime_error&) { result="E"; }
            expect(result==fields[3]) << count << fields[0] << hex(paint) << result << fields[3];
            ++count;
        }
        expect(count==2295_u);
    };
    "mermaid_theme_color_rejects_unbounded_literals"_test = [] {
        bool rejected=false;
        try { color_invert(std::u16string(4097,u' ')); } catch(const std::runtime_error&) { rejected=true; }
        expect(rejected);
        expect(bool(color_rgba(255,255,255,70)==u"#ffffff"));
    };
};
}
