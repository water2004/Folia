#include "support/folia_test.hpp"
import folia.mermaid.tree_view;

namespace {
using namespace boost::ut;
using namespace folia::mermaid;

suite tree_view_regressions = [] {
    "mermaid_tree_view_resolves_icon_precedence_and_missing_pack_references"_test = [] {
        TreeViewSvgConfig config;
        config.show_icons = true;
        config.default_icon_pack = u"material-icon-theme";
        config.filename_icons[u"Dockerfile"] = u"docker";
        config.filename_icons[u"notes.txt"] = u"folder";
        config.extension_icons[u".ts"] = u"typescript";
        config.extension_icons[u"txt"] = u"none";
        TreeViewNode node;
        node.name = u"Dockerfile";
        expect(resolve_tree_view_icon(node, config) == u"material-icon-theme:docker");
        node.name = u"app.TS";
        expect(resolve_tree_view_icon(node, config) == u"material-icon-theme:typescript");
        node.name = u"notes.txt";
        expect(resolve_tree_view_icon(node, config) == u"mermaid-treeview:folder");
        node.name = u"other.txt";
        expect(resolve_tree_view_icon(node, config).empty());
        node.icon = u"logos:react";
        expect(resolve_tree_view_icon(node, config) == u"logos:react");
        config.show_icons = false;
        expect(resolve_tree_view_icon(node, config) == u"logos:react");
        node.icon = u"none";
        expect(resolve_tree_view_icon(node, config).empty());
        node.icon.clear();
        expect(resolve_tree_view_icon(node, config).empty());
    };

    "mermaid_tree_view_reads_theme_variables_separately_from_layout_config"_test = [] {
        const auto session = ConfigSession::create();
        expect(fatal(session.has_value()));
        const auto prepared = prepare_diagram(uR"(---
config:
  treeView:
    rowIndent: 80
    lineThickness: 3
    showIcons: true
    extensionIcons: { .ts: none }
  themeVariables:
    treeView:
      labelFontSize: '20px'
      labelColor: '#ff0000'
      lineColor: '#00ff00'
---
treeView-beta
  src/
    index.ts
)",
                                              *session);
        expect(fatal(prepared.has_value())) << (prepared ? "" : prepared.error());
        const auto input = prepare_tree_view_render(*prepared);
        expect(fatal(input.has_value())) << (input ? "" : input.error());
        expect(input->config.row_indent == 80.0_d);
        expect(input->config.line_thickness == 3.0_d);
        expect(input->theme.label_font_size == 20.0_d);
        expect(input->theme.label_color == "#ff0000");
        expect(input->theme.line_color == "#00ff00");
        expect(input->config.extension_icons.at(u".ts") == u"none");
    };
};
} // namespace
