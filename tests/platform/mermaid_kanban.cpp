#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;
suite mermaid_kanban=[] {
    "mermaid_kanban_source_to_measured_svg"_test=[] {
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"columns",u"kanban\n  todo[To do]\n    a[Design]\n    b[Build]\n  done[Done]\n    c[Release]"},
            {"metadata",u"kanban\n  doing[Doing]\n    task[Review]@{ assigned: \"Ada\", ticket: \"PROJ-12\", priority: \"High\" }\n    next[Ship]@{ assigned: \"Lin\", priority: \"Low\" }"},
            {"priorities",u"kanban\n  queue[Queue]\n    a[One]@{ priority: \"Very High\" }\n    b[Two]@{ priority: \"Medium\" }\n    c[Three]@{ priority: \"Very Low\" }"},
            {"empty-column",u"kanban\n  todo[Todo]\n  done[Done]\n    task[Ready]"},
            {"wrapped",u"kanban\n  backlog[A longer backlog heading]\n    task[A card description that wraps across several lines in the column]"},
            {"configured",u"%%{init: {\"kanban\":{\"sectionWidth\":250}}}%%\nkanban\n  work[Work]\n    task[Configured width]\n  done[Done]"}
        };
        for(const auto theme:{u"default",u"dark"}) {
            auto session=ConfigSession::create();expect(fatal(session.has_value()));ConfigValue site;site.kind=ConfigKind::mapping;
            for(auto [key,value]:{std::pair{u"theme",theme},{u"fontFamily",u"Arial"}})site.mapping.emplace(key,ConfigValue{.kind=ConfigKind::string,.string=value});
            expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
            for(const auto& [name,source]:cases){auto result=render_mermaid(source,*session);expect(result.has_value())<<name<<(result?"":result.error());if(!result)continue;expect(result->width>0&&result->height>0);
                if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")){std::filesystem::create_directories(directory);const auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";std::ofstream output(std::filesystem::path(directory)/("kanban-"+name+"-"+suffix+".svg"),std::ios::binary);output<<result->svg;expect(output.good());}
            }
        }
    };
};
}
