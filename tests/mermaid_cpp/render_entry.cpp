#include "support/folia_test.hpp"
import folia.mermaid.render_entry;
import folia.mermaid.number;
import folia.mermaid.svg_text;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"
std::string ascii(std::u16string_view text) { return {text.begin(),text.end()}; }
double dimension(const ConfigValue& value) {
    return value.kind==ConfigKind::number ? value.number : std::stod(ascii(value.string));
}
std::string compact(std::string text) {
    std::erase_if(text,[](char c){ return c==' '||c=='\t'||c=='\r'||c=='\n'; }); return text;
}
suite render_entry = [] {
    "mermaid_render_entry_matches_original_family_configuration"_test=[] {
        const auto override=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override?override:FOLIA_MERMAID_FIXTURE_DIR}/"render-entry.tsv"};
        expect(fatal(input.is_open()));
        std::string line; std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r') line.pop_back();
            if(line.empty()||line.front()=='#') continue;
            std::vector<std::string_view> fields;
            std::string_view rest{line};
            for(;;) { auto tab=rest.find('\t'); fields.push_back(rest.substr(0,tab)); if(tab==rest.npos) break; rest.remove_prefix(tab+1); }
            expect(fatal(fields.size()==4_u)); ++count;
            auto session=ConfigSession::create(); expect(fatal(session.has_value()));
            auto site=json(decode(fields[2]));
            expect(fatal(session->save_initialize_config(site).has_value()));
            expect(fatal(session->set_site_config(site).has_value()));
            auto observed=json(decode(fields[3]));
            auto native=prepare_native_diagram(decode(fields[1]),*session);
            if(observed.kind==ConfigKind::null) { expect(!native)<<fields[0]; continue; }
            expect(native.has_value())<<fields[0]<<(native?"":native.error());
            expect(fatal(native.has_value()));
            const auto& config=field(observed,u"config"); const auto& theme=field(observed,u"theme");
            PieMeasurements widths;
            if(auto packet=std::get_if<PacketRenderInput>(&native->content)) {
                const auto& c=packet->layout.config;
                expect(c.row_height==field(config,u"rowHeight").number);
                expect(c.bit_width==field(config,u"bitWidth").number);
                expect(c.bits_per_row==field(config,u"bitsPerRow").number);
                expect(c.padding_x==field(config,u"paddingX").number);
                expect(c.padding_y+(c.show_bits?10:0)==field(config,u"paddingY").number);
                expect(c.show_bits==field(config,u"showBits").boolean);
                expect(c.use_max_width==field(config,u"useMaxWidth").boolean);
                expect(c.descending==(field(config,u"bitOrder").string==u"descending"));
                std::size_t index=0;
                for(const auto& row:packet->layout.words) for(const auto& block:row) {
                    auto display=diagram_display_text(block.label); expect(fatal(display.has_value()));
                    expect(bool(*display==field(observed,u"labels").sequence.at(index++).string));
                }
            } else {
                const auto& pie=std::get<PieRenderInput>(native->content);
                expect(pie.document.show_data==field(observed,u"showData").boolean);
                expect(pie.config.layout.geometry.donut_hole==field(config,u"donutHole").number);
                expect(pie.config.layout.geometry.text_position==field(config,u"textPosition").number);
                expect(pie.config.use_max_width==field(config,u"useMaxWidth").boolean);
                expect(bool(pie.config.highlight_slice==field(config,u"highlightSlice").string));
                const std::array names{u"right",u"left",u"top",u"bottom",u"center"};
                expect(bool(field(config,u"legendPosition").string==names[static_cast<int>(pie.config.layout.legend_position)]));
                expect(bool(pie.theme.font_family==field(theme,u"fontFamily").string));
                const auto& t=pie.theme;
                for(auto [key,value] : {std::pair{u"pieStrokeWidth",t.stroke_width}, {u"pieOuterStrokeWidth",t.outer_stroke_width},
                    {u"pieOpacity",t.opacity},{u"pieTitleTextSize",t.title_size},{u"pieSectionTextSize",t.section_size},{u"pieLegendTextSize",t.legend_size}})
                    expect(value==dimension(field(theme,key)))<<fields[0];
                for(auto [key,value] : {std::pair{u"pieStrokeColor",t.stroke_color},{u"pieOuterStrokeColor",t.outer_stroke_color},
                    {u"pieTitleTextColor",t.title_color},{u"pieSectionTextColor",t.section_color},{u"pieLegendTextColor",t.legend_color}})
                    expect(value==ascii(field(theme,key).string))<<fields[0];
                for(std::size_t i=0;i<12;++i) {
                    const auto suffix=std::to_string(i+1);
                    auto paint=theme.find(u"pie"+std::u16string(suffix.begin(),suffix.end()));
                    if(paint) expect(t.colors[i]==ascii(paint->string));
                    else expect(!t.colors[i]);
                }
                for(std::size_t i=0;i<pie.document.sections.size();++i) {
                    auto display=diagram_display_text(pie.document.sections[i].label); expect(fatal(display.has_value()));
                    expect(bool(*display==field(observed,u"labels").sequence.at(i).string));
                    widths.legend_widths.push_back(40+i*7.25); // Explicit test input, not a measurement estimate.
                }
                widths.title_width=100;
            }
            std::visit([&](const auto& content) {
                if constexpr(!std::is_same_v<std::decay_t<decltype(content)>,SankeyRenderInput>) {
                const auto& doc=[&]() -> const auto& {
                    if constexpr(std::is_same_v<std::decay_t<decltype(content)>,PacketRenderInput>) return content.layout.document;
                    else return content.document;
                }();
                expect(bool(doc.title==field(observed,u"title").string))<<fields[0];
                expect(bool(doc.acc_title==field(observed,u"accTitle").string));
                expect(bool(doc.acc_description==field(observed,u"accDescription").string));
                }
            },native->content);
            auto svg=render_native_diagram(*native,widths);
            expect(svg.has_value())<<fields[0]<<(svg?"":svg.error());
            expect(fatal(svg.has_value()));
            expect(svg->width>0 && svg->height>=0);
            expect(std::abs(svg->width-field(observed,u"width").number)<1e-10)<<fields[0];
            expect(std::abs(svg->height-field(observed,u"height").number)<1e-10)<<fields[0];
            if(std::holds_alternative<PacketRenderInput>(native->content)) {
                auto start=svg->svg.find("<style>")+7, end=svg->svg.find("</style>");
                expect(compact(svg->svg.substr(start,end-start))==ascii(field(observed,u"style").string))<<fields[0];
            }
            auto title=escape_svg_text(field(observed,u"displayTitle").string,u"test");
            expect(fatal(title.has_value())); expect(svg->svg.find(*title)!=std::string::npos);
            if(fields[0]!="pie-front-entity") expect(svg->svg.find("#quot;")==std::string::npos);
            if(fields[0]=="packet-title") expect(svg->svg.find("<title>Accessible</title><desc>Details</desc>")!=std::string::npos);
            if(fields[0]=="pie-dark-palette") {
                auto start=svg->svg.rfind("<path "), end=svg->svg.find('>',start);
                expect(svg->svg.substr(start,end-start).find("fill=")==std::string::npos);
                expect(svg->svg.find("<rect width=\"18\" height=\"18\"/>")!=std::string::npos);
            }
        }
        expect(count==18_u);
    };
    "mermaid_render_entry_errors_are_isolated"_test=[] {
        auto session=ConfigSession::create(); expect(fatal(session.has_value()));
        expect(!prepare_native_diagram(u"flowchart TD\nA-->B",*session));
        expect(!prepare_native_diagram(u"pie\n\"unfinished",*session));
        expect(!prepare_native_diagram(u"pie\ntitle <b>HTML</b>",*session));
        auto result=prepare_native_diagram(u"pie\n\"Tea\":1",*session); expect(fatal(result.has_value()));
        auto& pie=std::get<PieRenderInput>(result->content);
        pie.theme.colors[0]="url(https://example.invalid/paint)";
        expect(!render_native_diagram(*result,{{40},0}));
        auto packet=prepare_native_diagram(u"packet\n+8: \"Data\"",*session); expect(fatal(packet.has_value()));
        expect(render_native_diagram(*packet).has_value());
    };
};
}
