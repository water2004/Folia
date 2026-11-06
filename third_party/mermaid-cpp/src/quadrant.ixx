// Mermaid 12.2.0 quadrant.jison / quadrantDb.ts. MIT; see ../LICENSE.
export module folia.mermaid.quadrant;
export import folia.mermaid.frontend;
import std;

export namespace folia::mermaid {
struct QuadrantStyle {
    std::optional<double> radius;
    std::optional<std::u16string> color, stroke_color, stroke_width;
};
struct QuadrantPoint {
    std::u16string text, class_name;
    double x{}, y{};
    QuadrantStyle style;
};
struct QuadrantDocument {
    std::u16string title, acc_title, acc_description;
    std::array<std::u16string,4> quadrants;
    std::array<std::u16string,2> x_axis, y_axis;
    std::vector<QuadrantPoint> points;
    std::map<std::u16string,QuadrantStyle> classes;
};
std::expected<QuadrantDocument,std::string> parse_quadrant(std::u16string_view);
}
