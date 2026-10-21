#include "support/folia_test.hpp"
import folia.platform.mermaid_measurer;
import folia.platform.mermaid_pie;

namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;
suite mermaid_measurement = [] {
    "mermaid_real_font_measurement_has_no_estimate_fallback"_test=[] {
        MermaidMeasurer measurer;
        auto narrow=measurer.measure(u"iiii",{u"Arial",17});
        auto wide=measurer.measure(u"WWWW",{u"Arial",17});
        expect(fatal(narrow.has_value() && wide.has_value()));
        expect(wide->bounds_width > narrow->bounds_width*2);
        auto spaces=measurer.measure(u" A  B\tC\r\nD ",{u"Arial",17});
        auto normalized=measurer.measure(u"A B C D",{u"Arial",17});
        expect(fatal(spaces.has_value() && normalized.has_value()));
        expect(spaces->bounds_width==normalized->bounds_width);
        expect(!measurer.measure(u"x",{u"Folia Missing Family 792743",17}));
        expect(!measurer.measure(u"x",{u"Arial",0}));
        expect(!measurer.measure(std::u16string(1,0xdc00),{u"Arial",17}));
    };
    "mermaid_measured_pie_uses_resolved_font_for_title_and_deduplicated_showdata"_test=[] {
        PieDocument document;
        document.show_data=true;
        document.sections={{u"Wide WWW",3},{u"Wide WWW",9},{u"\u4e2d\u6587 \U0001f600",2}};
        document.title=std::u16string(100,u'W');
        PieSvgTheme theme;
        theme.colors.fill("#4477aa");
        theme.stroke_color=theme.outer_stroke_color=theme.title_color=
            theme.section_color=theme.legend_color="#112233";
        theme.font_family=u"Arial";
        auto svg=render_measured_pie(document,theme);
        expect(fatal(svg.has_value()));
        expect(svg->width>1000.0);
        expect(svg->svg.find("Wide WWW [3]")!=std::string::npos);
        expect(svg->svg.find("Wide WWW [9]")==std::string::npos);
        if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
            std::filesystem::create_directories(directory);
            std::ofstream output(std::filesystem::path(directory)/"measured-pie.svg",std::ios::binary);
            output<<svg->svg;
            expect(output.good());
        }
        theme.font_family=u"Folia Missing Family 792743";
        expect(!render_measured_pie(document,theme));
        theme.font_family=u"Arial";
        document.title=std::u16string(1,0xd800);
        expect(!render_measured_pie(document,theme));
    };
};
}
