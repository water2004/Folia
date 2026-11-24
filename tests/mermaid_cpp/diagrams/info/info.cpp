#include "support/folia_test.hpp"
import folia.mermaid.info_svg;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
suite mermaid_info_parse = [] {
    "mermaid_info_theme_font_overrides_global_font"_test = [] {
        const auto session = ConfigSession::create();
        expect(fatal(session.has_value()));
        const auto prepared = prepare_diagram(uR"(---
config:
  theme: base
  fontFamily: monospace
  fontSize: 24
  themeVariables:
    fontFamily: serif
    textColor: '#8b2252'
---
info showInfo
)", *session);
        expect(fatal(prepared.has_value()));
        const auto input = prepare_info_render(*prepared);
        expect(fatal(input.has_value()));
        expect(input->theme.font_family == u"serif");
        expect(input->theme.text_color == u"#8b2252");
        const auto svg = render_info_svg(*input);
        expect(fatal(svg.has_value()));
        expect(svg->svg.find("font-family=\"serif\"") != std::string::npos);
        expect(svg->svg.find("font-size=\"32\"") != std::string::npos);
    };
    "info_accepts_supported_forms"_test = [] {
        for (auto source : {u"info", u"\n info", u"info showInfo", u"info\nshowInfo\n"}) {
            auto parsed = parse_info(source);
            expect(fatal(parsed.has_value()));
            expect(parsed->version == info_version);
            expect(parsed->show_info == (std::u16string_view(source).find(u"showInfo") != std::u16string_view::npos));
        }
    };
    "info_rejects_unknown_syntax"_test = [] {
        auto parsed = parse_info(u"info unsupported");
        expect(!parsed.has_value());
        expect(parsed.error().offset == std::size_t{5});
        auto bad_title = parse_info(u"info\ntitleGarbage");
        expect(!bad_title.has_value());
    };
};
}
