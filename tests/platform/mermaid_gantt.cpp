#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;
suite mermaid_gantt=[] {
    "mermaid_gantt_source_to_measured_svg"_test=[] {
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"dates",u"gantt\ndateFormat YYYY-MM-DD\ntodayMarker off\ntitle Delivery plan\nDesign :design, 2025-01-06, 3d\nBuild :build, 2025-01-09, 2025-01-15\nShip :2025-01-15, 1d"},
            {"dependencies",u"gantt\ndateFormat YYYY-MM-DD\ntodayMarker off\nFirst :first, 2025-02-03, 2d\nSecond :second, after first, 3d\nParallel :parallel, 2025-02-03, until second\nFinish :after first second, 1d"},
            {"statuses",u"gantt\ndateFormat YYYY-MM-DD\ntodayMarker off\nCompleted :done, a, 2025-03-03, 2d\nCurrent :active, crit, b, after a, 3d\nReview :crit, c, after b, 1d\nLaunch :milestone, m, after c, 0d"},
            {"sections",u"gantt\ndateFormat YYYY-MM-DD\ntodayMarker off\nsection Prepare\nResearch :2025-04-01, 3d\nPlan :2025-04-04, 2d\nsection Deliver\nImplement :2025-04-06, 4d\nReview :2025-04-10, 1d"},
            {"weekends",u"gantt\ndateFormat YYYY-MM-DD\ntodayMarker off\nexcludes weekends\nincludes 2025-01-11\nWork :w, 2025-01-09, 4d\nNext :after w, 2d"},
            {"hours",u"gantt\ndateFormat YYYY-MM-DD HH:mm\ntodayMarker off\naxisFormat %H:%M\ntickInterval 2hour\nPrepare :2025-05-01 08:00, 2h\nWork :2025-05-01 10:00, 4h\nCheck :2025-05-01 14:00, 90m"},
            {"compact",u"%%{init: {\"gantt\":{\"displayMode\":\"compact\"}}}%%\ngantt\ndateFormat YYYY-MM-DD\ntodayMarker off\nsection Team\nOne :2025-06-02, 2d\nTwo :2025-06-02, 3d\nThree :2025-06-05, 1d"},
            {"axis",u"%%{init: {\"gantt\":{\"topAxis\":true}}}%%\ngantt\ndateFormat YYYY-MM-DD\ntodayMarker off\ntitle Milestones\naccTitle: Work schedule\naccDescr: Inclusive dates\ninclusiveEndDates\naxisFormat %m-%d\ntickInterval 2day\nFirst :2025-07-01, 2025-07-03\nSecond :2025-07-04, 2025-07-08"}
        };
        for(const auto theme:{u"default",u"dark"}) {
            auto session=ConfigSession::create();expect(fatal(session.has_value()));ConfigValue site;site.kind=ConfigKind::mapping;
            for(auto [key,value]:{std::pair{u"theme",theme},{u"fontFamily",u"Arial"}})site.mapping.emplace(key,ConfigValue{.kind=ConfigKind::string,.string=value});
            ConfigValue gantt;gantt.kind=ConfigKind::mapping;gantt.mapping.emplace(u"useWidth",ConfigValue{.kind=ConfigKind::number,.number=800});site.mapping.emplace(u"gantt",gantt);
            expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
            for(const auto& [name,source]:cases){auto result=render_mermaid(source,*session);expect(result.has_value())<<name<<(result?"":result.error());if(!result)continue;expect(result->width>0&&result->height>0);
                if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")){std::filesystem::create_directories(directory);const auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";std::ofstream output(std::filesystem::path(directory)/("gantt-"+name+"-"+suffix+".svg"),std::ios::binary);output<<result->svg;expect(output.good());}
            }
        }
    };
};
}
