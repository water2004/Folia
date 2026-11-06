#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;

namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;

suite mermaid_venn = [] {
    "mermaid_venn_native_svg"_test = [] {
        const std::vector<std::pair<std::string, std::u16string>> cases{
            {"two", u"venn-beta\nset A\nset B\nunion A,B[Both]"},
            {"three", u"venn-beta\nset A[Alpha]\nset B[Beta]\nset C[Gamma]\n"
                u"union A,B[AB]\nunion A,C[AC]\nunion B,C[BC]\nunion A,B,C[All]"},
            {"implicit", u"venn-beta\nset A\nset B\nset C\nunion A,B,C[Shared]"},
            {"styled", u"venn-beta\ntitle Intersecting circles\nset \"A one\"[First] : 18\nset B[Second] : 11\n"
                u"union \"A one\",B[Common] : 4\nstyle \"A one\" fill:#ff0000,stroke:#804020,"
                u"stroke-width:4,fill-opacity:0.3,color:#290000\nstyle B,\"A one\" fill:#aabbcc,color:#121212"},
            {"text", u"---\nconfig:\n  venn:\n    width: 640\n    height: 420\n    padding: 20\n"
                u"    useDebugLayout: true\n    useMaxWidth: false\n---\nvenn-beta\nset A[One]\n"
                u"  text first[First item]\n  text second[Second item]\nset B[Two]\n"
                u"union A,B[Both]\ntext B,A item[Shared item]\nstyle item color:#ee6655"},
            {"disjoint", u"venn-beta\nset A: 14\nset B: 8\nset C: 5"},
            {"four", u"venn-beta\nset A: 12\nset B: 11\nset C: 9\nset D: 8\nunion A,B,C,D[Shared]"},
            {"four-mds", u"venn-beta\nset A: 15\nset B: 12\nset C: 11\nset D: 9\n"
                u"union A,B: 7\nunion A,C: 5\nunion A,D: 4\nunion B,C: 1\nunion B,D: 2\nunion C,D: 3"},
            {"five-disjoint", u"venn-beta\nset A: 15\nset B: 13\nset C: 10\nset D: 8\nset E: 6"},
            {"contained", u"venn-beta\nset A: 18\nset B: 9\nset C: 9\nunion A,B: 9\nunion A,C: 9\nunion B,C: 9"},
            {"empty-set", u"venn-beta\nset A: 12\nset B: 0\nset C: 9\nunion A,B,C\ntext B item[Zero]"},
            {"hand-drawn", u"---\nconfig:\n  look: handDrawn\n  handDrawnSeed: 1\n---\n"
                u"venn-beta\ntitle Sketch\nset A[Alpha]\nset B[Beta]\nunion A,B[Both]"},
            {"hand-drawn-styled", u"---\nconfig:\n  look: handDrawn\n  handDrawnSeed: 9\n"
                u"  venn:\n    padding: 23\n---\nvenn-beta\nset A\n  text first[First]\nset B\n"
                u"union A,B[Both]\nstyle A fill:#ff6b6b,stroke:#704040,stroke-width:\"4px\"\n"
                u"style B fill:#4ecdc4\nstyle A,B fill:#ffe66d"},
        };
        for (const auto& [theme, suffix] : {std::pair{u"default", "default"}, {u"dark", "dark"},
            {u"redux-color", "redux-color"}, {u"redux-dark-color", "redux-dark-color"}}) {
            auto session = ConfigSession::create();
            expect(fatal(session.has_value()));
            ConfigValue site{.kind=ConfigKind::mapping};
            for (auto [key, value] : {std::pair{u"theme", theme}, {u"fontFamily", u"Arial"},
                {u"look", u"classic"}}) {
                site.mapping.emplace(key, ConfigValue{.kind=ConfigKind::string, .string=value});
            }
            expect(fatal(session->save_initialize_config(site).has_value()));
            expect(fatal(session->set_site_config(site).has_value()));
            for (const auto& [name, source] : cases) {
                if (std::u16string_view(theme).starts_with(u"redux") && name != "styled") continue;
                auto result = render_mermaid(source, *session);
                expect(result.has_value()) << name << (result ? "" : result.error());
                if (!result) continue;
                expect(result->width == (name == "text" ? 640.0 : 800.0));
                expect(result->height == (name == "text" ? 420.0 : 450.0));
                if (auto directory = std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                    std::filesystem::create_directories(directory);
                    std::ofstream output(std::filesystem::path(directory) /
                        ("venn-" + name + "-" + suffix + ".svg"), std::ios::binary);
                    output << result->svg;
                    expect(output.good());
                }
            }
            expect(!render_mermaid(u"venn-beta\nset A\nunion A,B", *session));
        }
    };
};
}
