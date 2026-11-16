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
    out.theme.container_stroke=narrow(member(tv,u"flowContainerStroke"),str(member(tv,u"secondaryBorderColor"),u"#aaaa33"));
    out.theme.border_colors = strings(member(tv, u"borderColorArray"));
    out.theme.bkg_colors = strings(member(tv, u"bkgColorArray"));
    out.theme.font_size = numeric(member(tv, u"fontSize"), 16);
    out.theme.diagram_padding = numeric(member(cfg, u"diagramPadding"), 8);
    out.theme.min_node_width = numeric(member(cfg, u"minNodeWidth"), 120);
    out.theme.wrapping_width = numeric(member(cfg, u"wrappingWidth"), 120);
    const auto& html = member(p.effective_config, u"htmlLabels");
    out.theme.html_labels = html.kind == ConfigKind::null || config_truthy(html);
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
    const SvgTextMeasure &measure, const MathLabelMeasure &math_measure) {
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
        graph.theme.cluster_fill="none";
        graph.theme.cluster_border=in.theme.container_stroke;
        graph.theme.cluster_stroke_width=.75;
        graph.theme.cluster_radius=graph.theme.collapsed_radius=10;
        graph.theme.collapsed_indicator_fill=graph.theme.border;
        graph.theme.collapsed_indicator_opacity=.5;
        graph.theme.neo = in.theme.neo;
        graph.theme.elk_line_hops = in.theme.elk_line_hops;
        FlowchartMeasurements measured;
        const auto line_height = std::max(in.theme.font_size * 1.5, 1.0);
        const auto label = [&](std::u16string_view text) -> std::expected<FlowchartTextBox, std::string> {
            if (in.theme.html_labels && math_measure) {
                auto rich = math_measure(text, in.theme.font_family, in.theme.font_size, 400, false);
                if (!rich) return std::unexpected(rich.error());
                if (*rich) {
                    FlowchartTextBox box;
                    box.width = (**rich).width;
                    box.height = (**rich).height;
                    box.math_label = std::make_shared<const MathLabel>(std::move(**rich));
                    return box;
                }
            }
            auto metrics = measure(text, in.theme.font_size, 400);
            if (!metrics) return std::unexpected(metrics.error());
            FlowchartTextBox box;
            box.width = metrics->width;
            box.height = std::max(metrics->height, line_height);
            box.baseline = metrics->baseline + (box.height - metrics->height) / 2;
            box.lines = {std::u16string{text}};
            return box;
        };
        for (const auto &c : in.document.containers) {
            FlowchartNode group;
            group.id = c.id;
            group.label = c.label;
            group.parent = c.parent;
            const auto* view=c.metadata.find(u"view");
            const bool collapsed=view && view->kind==ConfigKind::string && view->string==u"collapsed";
            group.shape = collapsed?u"collapsedGroup":u"rect";
            group.group = !collapsed;
            const auto& stroke=in.theme.container_stroke;
            group.styles={u"stroke:"+std::u16string{stroke.begin(),stroke.end()},u"stroke-width:0.75",u"fill:none"};
            group.padding = 8;
            graph.document.nodes.push_back(std::move(group));

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

        }
        for (const auto &e : in.document.edges) {
            FlowchartEdge edge;
            edge.start = e.from;
            edge.end = e.to;
            edge.label = e.label;
            edge.stroke = e.dashed ? u"dotted" : u"normal";
            edge.arrow_end=e.arrow_end;
            graph.document.edges.push_back(std::move(edge));
        }
        if(auto projected=project_collapsed_flowchart_groups(graph.document);!projected)
            return std::unexpected(projected.error());
        for(const auto& node:graph.document.nodes) {
            auto box=label(node.label);
            if(!box) return std::unexpected(box.error());
            measured.nodes.push_back(std::move(*box));
        }
        for(const auto& edge:graph.document.edges) {
            FlowchartTextBox box;
            if(!edge.label.empty()) {
                auto measured_label=label(edge.label);
                if(!measured_label) return std::unexpected(measured_label.error());
                box=std::move(*measured_label);
            }
            measured.edges.push_back(std::move(box));
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
