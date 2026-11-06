module folia.mermaid.agentflow_svg;
import std;
import folia.mermaid.flowchart_svg;
namespace folia::mermaid {
namespace {
const ConfigValue &member(const ConfigValue &v, std::u16string_view k) {
    static const ConfigValue empty{};
    auto p = v.find(k);
    return p ? *p : empty;
}
std::u16string str(const ConfigValue &v, std::u16string_view fallback) {
    return v.kind == ConfigKind::string ? v.string : std::u16string(fallback);
}
std::string narrow(const ConfigValue &v, std::u16string_view fallback) {
    const auto s = str(v, fallback);
    return {s.begin(), s.end()};
}
std::vector<std::string> strings(const ConfigValue &v) {
    std::vector<std::string> out;
    if (v.kind == ConfigKind::sequence)
        for (const auto &x : v.sequence)
            if (x.kind == ConfigKind::string)
                out.emplace_back(x.string.begin(), x.string.end());
    return out;
}
double numeric(const ConfigValue &v, double fallback) {
    return v.kind == ConfigKind::number ? v.number : fallback;
}
} // namespace
std::expected<AgentflowRenderInput, std::string> prepare_agentflow_render(const PreparedDiagram &p) {
    auto d = parse_agentflow(p.code.cleaned);
    if (!d)
        return std::unexpected(d.error());
    AgentflowRenderInput out;
    out.document = std::move(*d);
    const auto &tv = member(p.effective_config, u"themeVariables");
    const auto &cfg = member(p.effective_config, u"agentflow");
    out.theme.font_family = str(member(p.effective_config, u"fontFamily"), u"Arial");
    out.theme.text = narrow(member(tv, u"textColor"), u"#333333");
    out.theme.background = narrow(member(tv, u"mainBkg"), u"#ffffff");
    out.theme.line = narrow(member(tv, u"lineColor"), u"#333333");
    out.theme.border_colors = strings(member(tv, u"borderColorArray"));
    out.theme.bkg_colors = strings(member(tv, u"bkgColorArray"));
    out.theme.font_size = numeric(member(tv, u"fontSize"), 16);
    out.theme.diagram_padding = numeric(member(cfg, u"diagramPadding"), 8);
    out.theme.min_node_width = numeric(member(cfg, u"minNodeWidth"), 120);
    out.theme.wrapping_width = numeric(member(cfg, u"wrappingWidth"), 120);
    const auto look = str(member(cfg, u"look"), str(member(p.effective_config, u"look"), u"neo"));
    out.theme.neo = look == u"neo";
    const auto& hops = member(member(p.effective_config, u"elk"), u"lineHops");
    if (hops.kind == ConfigKind::boolean && !hops.boolean)
        out.theme.elk_line_hops = FlowchartElkLineHops::off;
    else if (hops.kind == ConfigKind::string && hops.string == u"gap")
        out.theme.elk_line_hops = FlowchartElkLineHops::gap;
    return out;
}
std::expected<AgentflowSvg, std::string> render_agentflow_svg(const AgentflowRenderInput &in,
                                                              const SvgTextMeasure &measure) {
    try {
        if (!measure)
            return std::unexpected("Agentflow rendering requires platform text measurement");
        FlowchartRenderInput graph;
        graph.document.direction = in.document.direction;
        graph.document.effective_config.kind = ConfigKind::mapping;
        graph.document.effective_config.mapping.emplace(
            u"layout", ConfigValue{.kind = ConfigKind::string, .string = u"elk"});
        graph.theme.font_family = in.theme.font_family;
        graph.theme.font_size = in.theme.font_size;
        graph.theme.minimum_label_width = in.theme.min_node_width;
        graph.theme.padding = in.theme.diagram_padding;
        graph.theme.wrapping_width = in.theme.wrapping_width;
        graph.theme.fill = in.theme.background;
        graph.theme.border = in.theme.line;
        graph.theme.text = in.theme.text;
        graph.theme.line = in.theme.line;
        graph.theme.neo = in.theme.neo;
        graph.theme.elk_line_hops = in.theme.elk_line_hops;
        FlowchartMeasurements measured;
        const auto line_height = std::max(in.theme.font_size * 1.5, 1.0);
        for (const auto &c : in.document.containers) {
            FlowchartNode group;
            group.id = c.id;
            group.label = c.label;
            group.parent = c.parent;
            group.shape = u"rect";
            group.group = true;
            group.padding = 8;
            graph.document.nodes.push_back(std::move(group));
            auto m = measure(c.label, in.theme.font_size, 400);
            if (!m)
                return std::unexpected(m.error());
            FlowchartTextBox b;
            b.width = m->width;
            b.height = std::max(m->height, line_height);
            b.baseline = m->baseline + (b.height - m->height) / 2;
            b.lines = {c.label};
            measured.nodes.push_back(std::move(b));
        }
        for (const auto &n : in.document.nodes) {
            FlowchartNode node;
            node.id = n.id;
            node.label = n.label;
            node.parent = n.parent;
            node.padding = 15;
            node.shape = n.shape == u"tool" || n.shape == u"subroutine" || n.shape == u"subproc" ||
                                 n.shape == u"subprocess" || n.shape == u"framed-rectangle"
                             ? u"subroutine"
                         : n.shape == u"decision" || n.shape == u"diamond"                    ? u"diamond"
                         : n.shape == u"action" || n.shape == u"hexagon" || n.shape == u"hex" ? u"hexagon"
                         : n.shape == u"input" || n.shape == u"lean-right" || n.shape == u"lean_right"
                             ? u"lean_right"
                         : n.shape == u"refdoc" || n.shape == u"lin-doc" || n.shape == u"lined-document"
                             ? u"lin-doc"
                             : u"roundedRect";
            if (!in.theme.border_colors.empty()) {
                const std::size_t slot = n.shape == u"tool"        ? 0
                                         : n.shape == u"task"      ? 1
                                         : n.shape == u"decision"  ? 2
                                         : n.shape == u"input"     ? 3
                                         : n.shape == u"refdoc"    ? 4
                                         : n.shape == u"connector" ? 5
                                                                   : 6;
                auto border = in.theme.border_colors[slot % in.theme.border_colors.size()];
                node.styles.push_back(u"stroke:" + std::u16string(border.begin(), border.end()));
                if (!in.theme.bkg_colors.empty()) {
                    auto fill = in.theme.bkg_colors[slot % in.theme.bkg_colors.size()];
                    node.styles.push_back(u"fill:" + std::u16string(fill.begin(), fill.end()));
                }
            }
            graph.document.nodes.push_back(std::move(node));
            auto m = measure(n.label, in.theme.font_size, 400);
            if (!m)
                return std::unexpected(m.error());
            FlowchartTextBox b;
            b.width = m->width;
            b.height = std::max(m->height, line_height);
            b.baseline = m->baseline + (b.height - m->height) / 2;
            b.lines = {n.label};
            measured.nodes.push_back(std::move(b));
        }
        for (const auto &e : in.document.edges) {
            FlowchartEdge edge;
            edge.start = e.from;
            edge.end = e.to;
            edge.label = e.label;
            edge.stroke = e.dashed ? u"dotted" : u"normal";
            if (e.dashed)
                edge.arrow_end = u"none";
            graph.document.edges.push_back(std::move(edge));
            FlowchartTextBox b;
            if (!e.label.empty()) {
                auto m = measure(e.label, in.theme.font_size, 400);
                if (!m)
                    return std::unexpected(m.error());
                b.width = m->width;
                b.height = std::max(m->height, line_height);
                b.baseline = m->baseline + (b.height - m->height) / 2;
                b.lines = {e.label};
            }
            measured.edges.push_back(std::move(b));
        }
        auto svg = render_flowchart_svg(graph, measured);
        if (!svg)
            return std::unexpected(svg.error());
        return AgentflowSvg{std::move(svg->svg), svg->width, svg->height};
    } catch (const std::exception &e) {
        return std::unexpected(e.what());
    }
}
} // namespace folia::mermaid
