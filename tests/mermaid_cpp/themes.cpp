#include "support/folia_test.hpp"
import folia.mermaid.theme;
import folia.mermaid.directives;
import folia.mermaid.number;
namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"
suite themes = [] {
    "mermaid_default_dark_themes_match_official_derivation"_test = [] {
        const auto directory=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{directory?directory:FOLIA_MERMAID_FIXTURE_DIR}/"themes.tsv"};
        expect(fatal(input.is_open()));
        std::map<std::string,ConfigValue> baselines;
        std::string line; std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty() && line.back()=='\r') line.pop_back();
            if(line.empty() || line.front()=='#') continue;
            std::vector<std::string_view> fields; std::string_view rest=line;
            while(true) { auto tab=rest.find('\t'); fields.push_back(rest.substr(0,tab)); if(tab==rest.npos) break; rest.remove_prefix(tab+1); }
            expect(fatal(fields.size()==5_u));
            auto overrides=fields[2]=="U"?ConfigValue{}:json(decode(fields[2]));
            const auto before=snapshot(overrides);
            const auto name=fields[0]=="default"?MermaidTheme::default_theme:MermaidTheme::dark;
            const auto result=derive_theme_variables(name,fields[2]=="U"?nullptr:&overrides);
            expect(snapshot(overrides)==before) << fields[0] << fields[1] << "input mutated";
            if(fields[3].starts_with('E') || fields[4]!="-") {
                expect(!result) << fields[0] << fields[1] << "expected rejection";
            } else {
                ConfigValue expected;
                if(fields[1]=="baseline") {
                    expected=json(decode(fields[3])); baselines.emplace(fields[0],expected);
                } else {
                    expected=baselines.at(std::string(fields[0]));
                    const auto delta=json(decode(fields[3]));
                    for(const auto& [key,value]:field(delta,u"set").mapping) expected.mapping.insert_or_assign(key,value);
                    for(const auto& key:field(delta,u"remove").sequence) expected.mapping.erase(key.string);
                }
                expect(result.has_value()) << fields[0] << fields[1] << (result?"":result.error());
                if(result) {
                    expect(result->mapping.size()==expected.mapping.size()) << fields[0] << fields[1] << "key count";
                    for(const auto& [key,value]:expected.mapping) {
                        const auto actual=result->find(key);
                        expect(actual!=nullptr) << fields[0] << fields[1] << hex(key);
                        if(actual) expect(snapshot(*actual)==snapshot(value)) << fields[0] << fields[1] << hex(key) << snapshot(*actual) << snapshot(value);
                    }
                }
            }
            ++count;
        }
        expect(count==1218_u);
    };
    "mermaid_themes_own_values_and_bound_input_output_work"_test = [] {
        auto overrides=json(uR"({"xyChart":{"titleColor":"#abc","extra":{"a":[1]}}})");
        auto result=derive_theme_variables(MermaidTheme::default_theme,&overrides);
        expect(fatal(result.has_value()));
        overrides.mapping.clear();
        expect(result->find(u"xyChart")->find(u"extra")!=nullptr);
        expect(result->find(u"xyChart")->find(u"xAxisTitleColor")!=nullptr);
        for(auto name:{MermaidTheme::default_theme,MermaidTheme::dark}) {
            expect(!derive_theme_variables(name,nullptr,{.values=1}));
            expect(!derive_theme_variables(name,nullptr,{.string_units=1}));
            expect(!derive_theme_variables(name,nullptr,{.operations=1}));
            auto deep=json(uR"({"custom":{"inner":1}})");
            expect(!derive_theme_variables(name,&deep,{.depth=1}));
            ConfigValue malformed; malformed.mapping.emplace(u"hidden",deep);
            expect(!derive_theme_variables(name,&malformed));
            auto large=json(u"{}"); ConfigValue str; str.kind=ConfigKind::string; str.string=std::u16string(4097,u'a');
            large.mapping.emplace(u"primaryColor",str);
            expect(!derive_theme_variables(name,&large));
            auto second=derive_theme_variables(name);
            expect(fatal(second.has_value()));
            second->mapping.clear();
            expect(derive_theme_variables(name)->mapping.size()>200_u);
        }
        expect(!derive_theme_variables(static_cast<MermaidTheme>(99)));
    };
};
}
