#include "support/folia_test.hpp"
import folia.platform.mermaid_renderer;

namespace {
using namespace boost::ut;
using namespace folia::platform;
using namespace folia::mermaid;

struct TreeViewCase {
    std::string_view name;
    std::u16string_view source;
    bool show_icons{};
};

constexpr std::array cases{
    TreeViewCase{"indent-basic", u"treeView-beta\nproject/\n  src/\n    main.cpp\n    util.cpp\n  README.md\n"},
    TreeViewCase{"annotations", u"treeView-beta\nproject/\n  src/ :::highlight icon(folder) ## Source code\n    main.cpp icon(file) ## Entry point\n"},
    TreeViewCase{"quoted-names", u"treeView-beta\n\"My project/\"\n  \"file with spaces.txt\"\n  README.md\n"},
    TreeViewCase{"box-drawing", u"treeView-beta\nRepository/\n├── src/\n│   ├── main.cpp\n│   └── util.cpp\n└── README.md\n"},
    TreeViewCase{"box-heavy", u"treeView-beta\nRepository/\n┣━━ src/\n┃   ┗━━ main.cpp\n┗━━ README.md\n"},
    TreeViewCase{"icons", u"treeView-beta\nproject/\n  app.ts\n  package.json\n", true},
};

suite mermaid_tree_view = [] {
    "mermaid_tree_view_measured_svg_matches_browser_cases"_test = [] {
        for (const auto theme : {std::u16string_view(u"default"), std::u16string_view(u"dark")}) {
            for (const auto& test_case : cases) {
                auto session = ConfigSession::create();
                expect(fatal(session.has_value()));
                ConfigValue site;
                site.kind = ConfigKind::mapping;
                site.mapping.emplace(u"fontFamily", ConfigValue{.kind=ConfigKind::string,.string=u"Arial"});
                site.mapping.emplace(u"theme", ConfigValue{.kind=ConfigKind::string,.string=std::u16string(theme)});
                ConfigValue tree_config;
                tree_config.kind = ConfigKind::mapping;
                tree_config.mapping[u"showIcons"] = ConfigValue{
                    .kind=ConfigKind::boolean,.boolean=test_case.show_icons};
                site.mapping[u"treeView"] = tree_config;
                expect(fatal(session->save_initialize_config(site).has_value()));
                expect(fatal(session->set_site_config(site).has_value()));

                auto result = render_mermaid(test_case.source, *session);
                expect(result.has_value()) << test_case.name << (result ? "" : result.error());
                if (!result) continue;
                expect(result->width > 0 && result->height > 0) << test_case.name;
                expect(result->svg.find("tree-view") != std::string::npos) << test_case.name;

                if (auto directory = std::getenv("FOLIA_MERMAID_SVG_OUTPUT")) {
                    std::filesystem::create_directories(directory);
                    std::string output_name(test_case.name);
                    if (theme == u"dark") output_name += "-dark";
                    std::ofstream output(std::filesystem::path(directory) /
                        ("tree-view-" + output_name + ".svg"), std::ios::binary);
                    output << result->svg;
                    expect(output.good()) << output_name;
                }
            }
        }
    };
};
}
