#include "support/folia_test.hpp"
import folia.platform.mermaid_measurer;
import folia.platform.mermaid_pie;
import folia.platform.mermaid_renderer;

namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;
suite mermaid_measurement = [] {
    "mermaid_flowchart_measured_source_to_svg"_test=[] {
        auto session=ConfigSession::create();expect(fatal(session.has_value()));
        const std::vector<std::pair<std::string,std::u16string>> cases={
            {"chain",u"flowchart TD\nA[Start] --> B[Finish]"},
            {"decision",u"flowchart TD\nA[Read] --> B{Valid?}\nB -->|yes| C(Done)\nB -->|no| A"},
            {"horizontal",u"flowchart LR\nA((Input)) --> B[Process]\nA --> C[Store]\nB --> D(Output)\nC --> D"},
            {"reverse",u"flowchart RL\nA[First] -->|next| B[Second]"},
            {"upward",u"flowchart BT\nA[First] --> B[Second]"},
            {"parallel",u"flowchart TD\nA[Source] -->|one| B[Target]\nA -. two .-> B"},
            {"unicode",u"flowchart LR\nA[\u4e2d\u6587] --> B[Caf\u00e9 \U0001f600]"},
        };
        for(const auto theme:{u"default",u"dark"}) for(const auto& [name,body]:cases) {
            const auto source=std::u16string(u"---\nconfig:\n  theme: ")+theme+
                u"\n  look: classic\n  layout: dagre\n  fontFamily: Arial\n  themeVariables: {fontFamily: Arial}\n  flowchart: {htmlLabels: true}\n---\n"+body;
            const auto svg=render_mermaid(source,*session);
            expect(svg.has_value())<<name<<(svg?"":svg.error());
            if(!svg) continue;
            expect(svg->width>0 && svg->height>0);
            expect(svg->svg.find("font-family=\"Arial\"")!=std::string::npos);
            expect(svg->svg.find("class=\"flowchart-link\"")!=std::string::npos);
            if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                std::filesystem::create_directories(directory);
                const auto suffix=theme==std::u16string_view(u"dark")?"dark":"default";
                std::ofstream out(std::filesystem::path(directory)/("flowchart-"+name+"-"+suffix+".svg"),std::ios::binary);
                out<<svg->svg;expect(out.good());
            }
        }
        const std::u16string prefix=u"---\nconfig: {theme: default, look: classic, layout: dagre, fontFamily: Arial}\n---\n";
        expect(!render_mermaid(prefix+u"flowchart TD\nA-->",*session));
        expect(!render_mermaid(prefix+u"flowchart TD\nsubgraph G\nA-->B\nend",*session));
        expect(!render_mermaid(prefix+u"flowchart TD\nA-->A",*session));
        expect(render_mermaid(prefix+u"flowchart TD\nA-->B",*session).has_value());
    };
    "mermaid_source_to_svg_resolves_fonts_and_measures_display_text"_test=[] {
        MermaidMeasurer measurer;
        auto family=measurer.resolve_family(u"'Folia Missing Family 792743', Arial, sans-serif");
        expect(fatal(family.has_value())); expect(bool(*family==u"Arial"));
        expect(!measurer.resolve_family(u"'Folia Missing Family 792743'"));
        auto session=ConfigSession::create(); expect(fatal(session.has_value()));
        for(const auto theme:{u"default",u"dark"}) {
            auto source=std::u16string(u"---\ntitle: Drinks\nconfig:\n  theme: ")+theme+
                u"\n  themeVariables:\n    fontFamily: Arial\n  pie:\n    legendPosition: bottom\n    donutHole: 0.5\n---\npie showData\n\"#quot;Tea#quot; #9733;\":3\n\"Coffee\":2";
            auto svg=render_mermaid(source,*session);
            expect(svg.has_value())<<(svg?"":svg.error());
            expect(fatal(svg.has_value()));
            expect(svg->svg.find("font-family=\"Arial\"")!=std::string::npos);
            expect(svg->svg.find("#quot;")==std::string::npos);
            expect(svg->svg.find("[3]")!=std::string::npos);
            if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                std::filesystem::create_directories(directory);
                std::ofstream out(std::filesystem::path(directory)/(theme==std::u16string_view(u"dark")?"entry-dark.svg":"entry-default.svg"),std::ios::binary);
                out<<svg->svg; expect(out.good());
            }
        }
        auto packet=render_mermaid(u"---\nconfig: {packet: {showBits: false, bitsPerRow: 8}}\n---\npacket\n+8: \"Data\"",*session);
        expect(fatal(packet.has_value())); expect(packet->width==258.0);
        expect(packet->svg.find("class=\"packetByte")==std::string::npos);
        expect(!render_mermaid(u"pie\n\"Tea\":",*session));
        expect(render_mermaid(u"pie\n\"Tea\":1",*session).has_value());
    };
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
