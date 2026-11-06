#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
using namespace folia::platform;

suite mermaid_error = [] {
    "mermaid_error_explicit_diagram_uses_theme"_test = [] {
        for (const auto theme : {u"default", u"dark"}) {
            auto session = ConfigSession::create();
            expect(fatal(session.has_value()));
            ConfigValue site{.kind = ConfigKind::mapping};
            site.mapping.emplace(u"theme", ConfigValue{.kind = ConfigKind::string, .string = theme});
            expect(fatal(session->set_site_config(site).has_value()));
            auto rendered = render_mermaid(u"  ErRoR\n", *session);
            expect(fatal(rendered.has_value())) << (rendered ? "" : rendered.error());
            expect(rendered->width == 2412.0);
            expect(rendered->height == 512.0);
            expect(rendered->svg.contains("Syntax error in text"));
            expect(rendered->svg.contains("mermaid version 12.2.0"));
            const bool dark = std::u16string_view{theme} == u"dark";
            expect(rendered->svg.contains(dark ? "#a44141" : "#552222"));
            if (const auto directory = std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                std::filesystem::create_directories(directory);
                const auto file = dark ? "error-dark.svg" : "error-default.svg";
                std::ofstream output(std::filesystem::path(directory) / file, std::ios::binary);
                output << rendered->svg;
                expect(output.good());
            }
        }
    };
};
}
