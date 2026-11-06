#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;

namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;
suite mermaid_info = [] {
    "mermaid_info_measured_svg"_test = [] {
        const std::vector<std::pair<std::string,std::u16string>> cases = {
            {"basic",u"info"}, {"show-info",u"info showInfo"},
            {"leading-newlines",u"\n\ninfo\nshowInfo\n"}};
        for (const auto& [name,source] : cases) for (auto theme : {"default","dark"}) {
            auto session = ConfigSession::create();
            expect(fatal(session.has_value()));
            if (std::string_view(theme)=="dark") {
                ConfigValue value; value.kind=ConfigKind::mapping;
                ConfigValue dark; dark.kind=ConfigKind::string; dark.string=u"dark";
                value.mapping.emplace(u"theme",std::move(dark));
                auto configured=session->set_site_config(value);
                expect(fatal(configured.has_value()));
            }
            auto rendered=render_mermaid(source,*session);
            expect(fatal(rendered.has_value()))<<name<<" "<<theme<<(rendered?"":rendered.error());
            expect(rendered->width==400.0 && rendered->height==100.0);
            if(auto directory=std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                std::filesystem::create_directories(directory);
                std::ofstream output(std::filesystem::path(directory)/("info-"+name+"-"+theme+".svg"),std::ios::binary);
                output<<rendered->svg; expect(output.good());
            }
        }
        auto session=ConfigSession::create();
        expect(fatal(session.has_value()));
        auto invalid=render_mermaid(u"info unsupported",*session);
        expect(!invalid.has_value());
    };
};
}
