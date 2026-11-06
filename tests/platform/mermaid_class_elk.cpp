#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;
suite mermaid_class_elk=[] {
    "mermaid_class_elk_source_to_measured_svg"_test=[] {
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"elk-members",u"classDiagram\nclass Document {\n+title String\n+save() bool\n}\nDocument --> Store : saves"},
            {"elk-inheritance",u"classDiagram\nBase <|-- Reader\nBase <|-- Writer\nReader --> Writer : sends"},
            {"elk-multiplicity",u"classDiagram\ndirection LR\nLibrary \"1\" o-- \"*\" Book : contains"},
            {"elk-feedback",u"classDiagram\ndirection LR\nReader \"1\" --> \"*\" Store : writes\nStore \"1\" --> \"1\" Reader : refreshes"},
            {"elk-namespace",u"classDiagram\nnamespace Input {\nclass Reader\nclass Source\n}\nnamespace Output {\nclass Writer\n}\nSource --> Reader : reads\nReader \"1\" --> \"*\" Writer : sends"},
        };
        for(const auto theme:{u"default",u"dark"})for(const auto& sample:cases) {
            const auto& name=sample.first;
            const auto& body=sample.second;
            auto session=ConfigSession::create();expect(fatal(session.has_value()));
            ConfigValue site;site.kind=ConfigKind::mapping;
            const std::array settings={std::pair{u"theme",theme},std::pair{u"fontFamily",u"Arial"},
                std::pair{u"look",u"classic"},std::pair{u"layout",u"elk"}};
            for(const auto& setting:settings)
                site.mapping.emplace(setting.first,ConfigValue{.kind=ConfigKind::string,.string=setting.second});
            ConfigValue variables;variables.kind=ConfigKind::mapping;
            variables.mapping.emplace(u"fontFamily",ConfigValue{.kind=ConfigKind::string,.string=u"Arial"});
            site.mapping.emplace(u"themeVariables",std::move(variables));
            expect(fatal(session->save_initialize_config(site).has_value()));
            expect(fatal(session->set_site_config(site).has_value()));
            const auto source=std::u16string(u"---\nconfig:\n  theme: ")+theme+
                u"\n  look: classic\n  layout: elk\n  fontFamily: Arial\n  themeVariables: {fontFamily: Arial}\n  htmlLabels: true\n---\n"+body;
            const auto rendered=render_mermaid(source,*session);
            expect(rendered.has_value())<<name<<(rendered?"":rendered.error());
            if(!rendered)continue;
            expect(rendered->width>0&&rendered->height>0);
            if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                std::filesystem::create_directories(directory);
                const auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";
                std::ofstream out(std::filesystem::path(directory)/("class-"+name+"-"+suffix+".svg"),std::ios::binary);
                out<<rendered->svg;expect(out.good());
            }
        }
    };
};
}
