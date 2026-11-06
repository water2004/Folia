#include "support/folia_test.hpp"
import folia.mermaid.render_entry;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"
void same(std::u16string_view actual,const ConfigValue& expected,std::u16string_view key) {
    expect(bool(actual==field(expected,key).string))<<hex(key)<<hex(actual)<<hex(field(expected,key).string);
}
void near(double actual,const ConfigValue& expected,std::u16string_view key) {
    expect(std::abs(actual-field(expected,key).number)<1e-8)<<hex(key)<<actual<<field(expected,key).number;
}
void text(const QuadrantText& a,const ConfigValue& e) {
    same(a.text,e,u"text");same(a.fill,e,u"fill");near(a.x,e,u"x");near(a.y,e,u"y");near(a.size,e,u"fontSize");near(a.rotation,e,u"rotation");
    expect(a.centered==(field(e,u"verticalPos").string==u"center"));expect(a.middle==(field(e,u"horizontalPos").string==u"middle"));
}
suite quadrant=[] {
    "mermaid_quadrant_public_entry_matches_original_layout_and_styles"_test=[] {
        const auto override_path=std::getenv("FOLIA_MERMAID_FIXTURES");
        std::ifstream input{std::filesystem::path{override_path?override_path:FOLIA_MERMAID_FIXTURE_DIR}/"quadrant.tsv"};
        expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            std::vector<std::string_view> fields;std::string_view rest{line};
            for(;;){auto tab=rest.find('\t');fields.push_back(rest.substr(0,tab));if(tab==rest.npos)break;rest.remove_prefix(tab+1);}
            expect(fatal(fields.size()==4_u));++count;
            auto session=ConfigSession::create();expect(fatal(session.has_value()));auto site=json(decode(fields[2]));
            expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
            auto expected=json(decode(fields[3]));auto actual=prepare_native_diagram(decode(fields[1]),*session);
            if(expected.kind==ConfigKind::null){expect(!actual)<<fields[0];continue;}
            expect(fatal(actual.has_value()))<<fields[0]<<(actual?"":actual.error());
            const auto* family=std::get_if<QuadrantRenderInput>(&actual->content);expect(fatal(family!=nullptr));
            same(family->document.title,expected,u"title");same(family->document.acc_title,expected,u"accTitle");same(family->document.acc_description,expected,u"accDescription");
            auto layout=layout_quadrant(*family);const auto& e=field(expected,u"layout");
            const auto& quadrants=field(e,u"quadrants").sequence;expect(fatal(quadrants.size()==4_u));
            for(std::size_t i=0;i<4;++i){const auto& a=layout.quadrants[i];const auto& q=quadrants[i];near(a.x,q,u"x");near(a.y,q,u"y");near(a.width,q,u"width");near(a.height,q,u"height");same(a.fill,q,u"fill");text(a.text,field(q,u"text"));}
            const auto& borders=field(e,u"borderLines").sequence;expect(fatal(borders.size()==6_u));
            for(std::size_t i=0;i<6;++i){const auto& a=layout.borders[i];const auto& b=borders[i];near(a.x1,b,u"x1");near(a.y1,b,u"y1");near(a.x2,b,u"x2");near(a.y2,b,u"y2");near(a.width,b,u"strokeWidth");same(a.fill,b,u"strokeFill");}
            const auto& points=field(e,u"points").sequence;expect(fatal(points.size()==layout.points.size()));
            for(std::size_t i=0;i<points.size();++i){const auto& a=layout.points[i];const auto& p=points[i];near(a.x,p,u"x");near(a.y,p,u"y");near(a.radius,p,u"radius");same(a.fill,p,u"fill");same(a.stroke,p,u"strokeColor");same(a.stroke_width,p,u"strokeWidth");text(a.text,field(p,u"text"));}
            const auto& axes=field(e,u"axisLabels").sequence;expect(fatal(axes.size()==layout.axes.size()));for(std::size_t i=0;i<axes.size();++i)text(layout.axes[i],axes[i]);
            expect(layout.title.has_value()==(e.find(u"title")!=nullptr));if(layout.title)text(*layout.title,field(e,u"title"));
            auto svg=render_native_diagram(*actual);expect(fatal(svg.has_value()))<<fields[0]<<(svg?"":svg.error());
            expect(svg->width==family->config.width);expect(svg->height==family->config.height);
            if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                std::filesystem::create_directories(directory);std::ofstream output(std::filesystem::path(directory)/("quadrant-"+std::string(fields[0])+".svg"),std::ios::binary);output<<svg->svg;expect(output.good());
            }
        }
        expect(count==28_u);
    };
};
}
