#include "support/folia_test.hpp"
import folia.mermaid.render_entry;
import folia.mermaid.number;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
#include "theme_fixture.hpp"
void same(std::u16string_view actual,const ConfigValue& expected,std::u16string_view key){expect(bool(actual==field(expected,key).string))<<hex(key)<<hex(actual)<<hex(field(expected,key).string);}
void near(double actual,const ConfigValue& expected,std::u16string_view key){expect(std::abs(actual-field(expected,key).number)<1e-8)<<hex(key)<<actual<<field(expected,key).number;}
std::vector<double> path_numbers(std::string value) {
    for(auto& c:value)if(c=='M'||c=='C'||c=='Z'||c==',')c=' ';
    std::istringstream input(value);std::vector<double> out;double v;while(input>>v)out.push_back(v);return out;
}
suite radar=[] {
    "mermaid_radar_public_entry_matches_original_parser_polar_curves_and_config"_test=[] {
        const auto override_path=std::getenv("FOLIA_MERMAID_FIXTURES");std::ifstream input{std::filesystem::path{override_path?override_path:FOLIA_MERMAID_FIXTURE_DIR}/"radar.tsv"};
        expect(fatal(input.is_open()));std::string line;std::size_t count=0;
        while(std::getline(input,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()||line.front()=='#')continue;
            std::vector<std::string_view> fields;std::string_view rest{line};for(;;){auto tab=rest.find('\t');fields.push_back(rest.substr(0,tab));if(tab==rest.npos)break;rest.remove_prefix(tab+1);}
            expect(fatal(fields.size()==4_u));++count;auto session=ConfigSession::create();expect(fatal(session.has_value()));auto site=json(decode(fields[2]));
            expect(fatal(session->save_initialize_config(site).has_value()));expect(fatal(session->set_site_config(site).has_value()));
            auto expected=json(decode(fields[3]));auto actual=prepare_native_diagram(decode(fields[1]),*session);
            if(expected.kind==ConfigKind::null){expect(!actual)<<fields[0];continue;}
            expect(fatal(actual.has_value()))<<fields[0]<<(actual?"":actual.error());const auto* family=std::get_if<RadarRenderInput>(&actual->content);expect(fatal(family!=nullptr));
            const auto& d=family->document;same(d.title,expected,u"title");same(d.acc_title,expected,u"accTitle");same(d.acc_description,expected,u"accDescription");
            const auto& axes=field(expected,u"axes").sequence;expect(fatal(axes.size()==d.axes.size()));for(std::size_t i=0;i<axes.size();++i){same(d.axes[i].name,axes[i],u"name");same(d.axes[i].label,axes[i],u"label");}
            const auto& curves=field(expected,u"curves").sequence;expect(fatal(curves.size()==d.curves.size()));
            for(std::size_t i=0;i<curves.size();++i){same(d.curves[i].name,curves[i],u"name");same(d.curves[i].label,curves[i],u"label");const auto& entries=field(curves[i],u"entries").sequence;expect(fatal(entries.size()==d.curves[i].entries.size()));for(std::size_t j=0;j<entries.size();++j)expect(d.curves[i].entries[j]==entries[j].number);}
            const auto& options=field(expected,u"options");expect(d.options.show_legend==field(options,u"showLegend").boolean);expect(d.options.polygon==(field(options,u"graticule").string==u"polygon"));near(d.options.ticks,options,u"ticks");near(d.options.min,options,u"min");expect(d.options.max.has_value()==(field(options,u"max").kind!=ConfigKind::null));if(d.options.max)near(*d.options.max,options,u"max");
            const auto& c=family->config;const auto& cfg=field(expected,u"config");
            for(auto [key,value]:{std::pair{u"width",c.width},{u"height",c.height},{u"marginTop",c.top},{u"marginRight",c.right},{u"marginBottom",c.bottom},{u"marginLeft",c.left},{u"axisScaleFactor",c.axis_scale},{u"axisLabelFactor",c.label_scale},{u"curveTension",c.tension}})near(value,cfg,key);
            const auto& t=family->theme;const auto& theme=field(expected,u"theme");same(t.text,theme,u"textColor");same(t.title,theme,u"titleColor");const auto& style=field(theme,u"radar");same(t.axis,style,u"axisColor");same(t.graticule,style,u"graticuleColor");
            for(auto [key,value]:{std::pair{u"axisStrokeWidth",t.axis_width},{u"axisLabelFontSize",t.axis_size},{u"curveOpacity",t.curve_opacity},{u"curveStrokeWidth",t.curve_width},{u"graticuleOpacity",t.graticule_opacity},{u"graticuleStrokeWidth",t.graticule_width},{u"legendFontSize",t.legend_size}})near(value,style,key);
            const auto& colors=field(theme,u"colors").sequence;expect(fatal(colors.size()==t.colors.size()));for(std::size_t i=0;i<colors.size();++i)expect(bool(colors[i].string==t.colors[i]));
            auto geometry=radar_curve_geometry(*family);expect(fatal(geometry.has_value()));const auto& expected_geometry=field(expected,u"geometry").sequence;std::size_t drawn=0;
            for(std::size_t i=0;i<expected_geometry.size();++i) {
                const auto& e=expected_geometry[i];if(e.kind==ConfigKind::null)continue;expect(fatal(drawn<geometry->size()));const auto& a=(*geometry)[drawn++];expect(a.index==i);
                const auto& points=field(e,u"points").sequence;expect(fatal(points.size()==a.points.size()));for(std::size_t j=0;j<points.size();++j){near(a.points[j].x,points[j],u"x");near(a.points[j].y,points[j],u"y");}
                const auto& path=field(e,u"path");if(path.kind==ConfigKind::null)expect(a.path.empty());else {auto an=path_numbers(a.path),en=path_numbers(std::string(path.string.begin(),path.string.end()));expect(fatal(an.size()==en.size()));for(std::size_t j=0;j<an.size();++j)expect(std::abs(an[j]-en[j])<1e-8);}
            }
            expect(drawn==geometry->size());auto svg=render_native_diagram(*actual);expect(fatal(svg.has_value()))<<fields[0]<<(svg?"":svg.error());expect(svg->width==c.width+c.left+c.right);expect(svg->height==c.height+c.top+c.bottom);
            if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")){std::filesystem::create_directories(directory);std::ofstream output(std::filesystem::path(directory)/("radar-"+std::string(fields[0])+".svg"),std::ios::binary);output<<svg->svg;expect(output.good());}
        }
        expect(count==22_u);
    };
};
}
