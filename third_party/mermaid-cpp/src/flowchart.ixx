// Common Flowchart grammar/FlowDB/getData from Mermaid 12.2.0,
// commit 1fb7869866dd619168d840291c9c54b4f129758b.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see ../LICENSE.
export module folia.mermaid.flowchart;
import std;
export import folia.mermaid.frontend;

export namespace folia::mermaid {
struct FlowchartNode {
    std::u16string id, label, label_type = u"text", shape = u"squareRect";
    std::u16string parent, direction, classes;
    bool group = false;
    double padding = 15;
    std::vector<std::u16string> styles, compiled_styles;
};
struct FlowchartEdge {
    std::u16string id, start, end, label, label_type = u"text";
    std::u16string type = u"arrow_point", stroke = u"normal";
    std::u16string arrow_start = u"none", arrow_end = u"arrow_point", curve;
    std::size_t min_length = 1;
    std::vector<std::u16string> styles, compiled_styles;
};
struct FlowchartInput {
    std::u16string direction = u"TB", title, acc_title, acc_description;
    double node_spacing = 50, rank_spacing = 50;
    // Groups precede vertices; compound parents and edge endpoints are ids.
    // Labels/styles are owned source data, not executable HTML or CSS.
    std::vector<FlowchartNode> nodes;
    std::vector<FlowchartEdge> edges;
    ConfigValue effective_config;
};
struct FlowchartLimits {
    FrontendLimits frontend;
    std::size_t nodes = 10000, edges = 500, subgraph_depth = 64;
};
// Shared preparation -> common classic Flowchart grammar -> FlowDB layout
// input. No coordinates, text measurement, SVG, or renderer registration yet.
// Supports classic shapes, text/quoted labels, link chains/groups, subgraphs,
// directions, classDef/class/style/linkStyle. Unsupported syntax fails rather
// than producing a partial graph. Errors include offsets into prepared UTF-16
// text (frontmatter preprocessing can change offsets from the original source).
// Request state is private; failure never changes the session or returns data.
// As in Folia's render entry, initialize the session with an explicit default
// (light) or dark theme. The upstream family's redux-color default is unported.
std::expected<FlowchartInput, std::string> prepare_flowchart(
    std::u16string_view source, const ConfigSession& session, FlowchartLimits limits = {});
}
