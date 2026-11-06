#include "support/folia_test.hpp"
import folia.mermaid.info;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;
suite mermaid_info_parse = [] {
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
