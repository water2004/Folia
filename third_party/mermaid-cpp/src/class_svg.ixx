// Original Class shapeUtil/classBox and unified Dagre adapter, Mermaid 12.2.0.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see ../LICENSE.
export module folia.mermaid.class_svg;
import std;
export import folia.mermaid.class_diagram;
export import folia.mermaid.dagre_rank;

export namespace folia::mermaid {
struct ClassTextBox {
    std::u16string text;
    double width = 0, height = 0, baseline = 0;
};
struct ClassNodeMeasurements {
    ClassTextBox label, annotation;
    std::vector<ClassTextBox> members, methods;
};
struct ClassEdgeMeasurements { ClassTextBox label, start, end; };
struct ClassNoteMeasurements {
    std::vector<ClassTextBox> lines;
    double width = 0, height = 0;
};
struct ClassMeasurements {
    ClassTextBox title;
    std::vector<ClassNodeMeasurements> nodes;
    std::vector<ClassEdgeMeasurements> edges;
    std::vector<ClassNoteMeasurements> notes;
    std::vector<ClassTextBox> namespaces;
};
struct ClassSvgTheme {
    std::u16string font_family = u"Arial";
    double font_size = 16, stroke_width = 1;
    std::string fill = "#ECECFF", border = "#9370DB", text = "#131300", line = "#333333", label_background = "#e8e8e8", title_color = "#333333";
    std::string note_fill = "#fff5ad", note_border = "#aaaa33", note_text = "#333333";
    std::string cluster_fill = "#ffffde", cluster_border = "#aaaa33", cluster_text = "#333333";
};
struct ClassSvgConfig {
    double padding = 12, note_padding = 6, node_spacing = 50, rank_spacing = 50, title_top_margin = 25;
    bool hide_empty_members = false;
    double group_padding = 16, group_title_top = 0, group_title_bottom = 0;
};
struct ClassRenderInput { ClassInput document; ClassSvgTheme theme; ClassSvgConfig config; };
struct ClassSvg { std::string svg; double width = 0, height = 0; };
// Explicit classic/Dagre configuration; the official default ELK remains a
// separate layout boundary. Inputs contain real final display measurements.
std::expected<ClassSvg,std::string> render_class_svg(const ClassRenderInput&,const ClassMeasurements&);
}
