#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;
namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;
suite mermaid_xy = [] {
    "mermaid_xy_source_to_measured_svg"_test = [] {
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"category-bars",u"xychart-beta\ntitle Monthly output\nx-axis Month [Jan, Feb, Mar]\ny-axis Count 0 --> 100\nbar [20, 60, 40]"},
            {"numeric-line",u"xychart-beta\nx-axis Time 0 --> 10\ny-axis Value -10 --> 10\nline [-5, 0, 8, 3]"},
            {"automatic",u"xychart-beta\nbar [5, 12, 8]\nline [7, 9, 11]"},
            {"horizontal",u"xychart-beta horizontal\ntitle \"Horizontal\"\nx-axis [One, Two, Three]\ny-axis Amount 0 --> 30\nbar [10, 25, 20]\nline [12, 20, 18]"},
            {"legend",u"%%{init: {\"xyChart\":{\"showLegend\":true}}}%%\nxychart-beta\nx-axis [A, B, C]\nbar Sales [10, 30, 20]\nline Trend [15, 25, 18]"},
            {"configured",u"%%{init: {\"xyChart\":{\"width\":640,\"height\":400,\"titleFontSize\":24,\"plotReservedSpacePercent\":60,\"xAxis\":{\"showTitle\":false,\"labelFontSize\":14,\"tickLength\":8},\"yAxis\":{\"showTick\":false,\"showAxisLine\":false}},\"themeVariables\":{\"xyChart\":{\"plotColorPalette\":\"#cc3300,#0066aa\"}}}}%%\nxychart-beta\ntitle Configured\nx-axis Month [Jan, Feb, Mar]\ny-axis Total 0 --> 40\nbar [10, 30, 20]\nline [15, 25, 18]"},
            {"labels-vertical",u"%%{init: {\"xyChart\":{\"showDataLabel\":true}}}%%\nxychart-beta\nx-axis [A, B, C]\ny-axis 0 --> 100\nbar [20, 60, 40]"},
            {"labels-horizontal",u"%%{init: {\"xyChart\":{\"showDataLabel\":true,\"showDataLabelOutsideBar\":true}}}%%\nxychart-beta horizontal\nx-axis [A, B, C]\ny-axis 0 --> 100\nbar [20, 60, 40]"},
            {"point-labels",u"xychart-beta\nx-axis [A, B, C]\nline [20 \"First\", 60, 40 \"Last\"]"},
            {"negative",u"xychart-beta\nx-axis [A, B, C]\ny-axis -20 --> 20\nbar [-10, 0, 15]"},
            {"truncated",u"xychart-beta\nx-axis [A, B]\nbar [2, 5, 100]"},
            {"unicode-metadata",u"---\ntitle: 项目统计\n---\nxychart-beta\naccTitle: Chart\naccDescr {\nEditor output\nby month\n}\nx-axis [\"一月\", \"二月\", \"三月\"]\nline [3, 7, 5]"},
        };
        for(const auto theme:{u"default",u"dark"}) {
            auto session=ConfigSession::create();expect(fatal(session.has_value()));
            ConfigValue site;site.kind=ConfigKind::mapping;
            for(auto [key,value]:{std::pair{u"theme",theme},{u"fontFamily",u"Arial"}})
                site.mapping.emplace(key,ConfigValue{.kind=ConfigKind::string,.string=value});
            expect(fatal(session->save_initialize_config(site).has_value()));
            expect(fatal(session->set_site_config(site).has_value()));
            for(const auto& [name,source]:cases) {
                auto result=render_mermaid(source,*session);
                expect(result.has_value())<<name<<(result?"":result.error());
                if(!result) continue;
                expect(result->width>0 && result->height>0);
                if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                    std::filesystem::create_directories(directory);
                    const auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";
                    std::ofstream output(std::filesystem::path(directory)/("xy-"+name+"-"+suffix+".svg"),std::ios::binary);
                    output<<result->svg;expect(output.good());
                }
            }
        }
    };
};
}
