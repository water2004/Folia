#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace { using namespace boost::ut; using namespace folia::platform; using namespace folia::mermaid;
suite mermaid_wardley_platform = [] {
 "mermaid_wardley_native_svg"_test = [] {
   const std::vector<std::pair<std::string,std::u16string>> cases{
     {"basic",u"wardley-beta\ntitle Example\nanchor User [1.0, 0.1]\ncomponent API [0.7, 0.65]\ncomponent Database [0.4, 0.9]\nAPI --> Database"},
     {"links",u"wardley-beta\ncomponent A [0.8, 0.2]\ncomponent B [0.5, 0.6]\nA -.-> B\nA +> B\nA +< B\nA +<> B"},
     {"evolution",u"wardley-beta\nevolution Genesis@0.15 -> Custom Built@0.42 -> Product@0.72 -> Commodity@1.0\ncomponent Service [0.8, 0.55] (buy) inertia\nevolve Service 0.9\nnote \"Market\" [0.4, 0.7]"},
     {"pipeline",u"wardley-beta\ncomponent Service [0.8, 0.55]\npipeline Service {\n  component Design [0.2]\n  component Build [0.45]\n  component Operate [0.7]\n}\nannotations [0.85, 0.12]\nannotation 1, [0.9, 0.8] \"Scale later\"\naccelerator \"Automation\" [0.6, 0.7]\ndeaccelerator \"Legacy\" [0.4, 0.25]"},
     {"strategies",u"wardley-beta\ncomponent Built [0.4, 0.2] (build)\ncomponent Bought [0.55, 0.4] (buy)\ncomponent Vendor [0.7, 0.6] (outsource)\ncomponent Market [0.85, 0.8] (market)"},
     {"quoted-labels",u"wardley-beta\ncomponent \"API Server\" [0.8, 0.35] label [-12,14]\ncomponent \"Data Store\" [0.5, 0.75]\n\"API Server\" +'customer'> \"Data Store\""},
   };
   for(auto theme:{std::u16string_view(u"default"),std::u16string_view(u"dark")}){
     auto session=ConfigSession::create();expect(fatal(session.has_value()));ConfigValue config;config.kind=ConfigKind::mapping;
     config.mapping.emplace(u"theme",ConfigValue{.kind=ConfigKind::string,.string=std::u16string(theme)});
     config.mapping.emplace(u"fontFamily",ConfigValue{.kind=ConfigKind::string,.string=u"Arial"});
     config.mapping.emplace(u"look",ConfigValue{.kind=ConfigKind::string,.string=u"classic"});
     expect(fatal(session->save_initialize_config(config).has_value()));expect(fatal(session->set_site_config(config).has_value()));
     for(const auto& [name,source]:cases){auto svg=render_mermaid(source,*session);expect(svg.has_value())<<name<<(svg?"":svg.error());if(!svg)continue;expect(svg->width==900.0);expect(svg->height==600.0);if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")){std::filesystem::create_directories(directory);auto suffix=theme==u"dark"?"dark":"default";std::ofstream output(std::filesystem::path(directory)/("wardley-"+name+"-"+suffix+".svg"),std::ios::binary);output<<svg->svg;expect(output.good());}}
   }
 };
}; }
