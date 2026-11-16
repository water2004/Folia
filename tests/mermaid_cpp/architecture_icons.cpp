#include "support/folia_test.hpp"
import folia.mermaid.architecture_icons;

namespace {
using namespace boost::ut;
suite architecture_icons = [] {
    "mermaid_architecture_builtin_namespace_and_unknown_artwork"_test = [] {
        using folia::mermaid::architecture_icon;
        for (const auto name : {u"database",u"server",u"disk",u"internet",u"cloud",u"blank"}) {
            const auto plain=architecture_icon(name);
            expect(fatal(plain.has_value()));
            const auto qualified=architecture_icon(std::u16string{u"mermaid-architecture:"}+name);
            expect(fatal(qualified.has_value()));
            expect(*plain==*qualified);
        }
        const auto unknown=architecture_icon(u"unknown");
        expect(fatal(unknown.has_value()));
        expect(unknown->find("<tspan x=\"0\" y=\"0\">?</tspan>")!=std::string_view::npos);
        expect(unknown->find("width=\"80\" height=\"80\"")!=std::string_view::npos);
        expect(!architecture_icon(u"unregistered:cloud"));
    };
};
}
