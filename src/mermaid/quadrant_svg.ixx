// Mermaid 12.2.0 quadrantBuilder.ts / quadrantRenderer.ts. MIT; see ../LICENSE.
export module folia.mermaid.quadrant_svg;
export import folia.mermaid.quadrant;
import std;

export namespace folia::mermaid {
struct QuadrantConfig {
    double width=500,height=500,title_padding=10,title_size=20,padding=5;
    double x_padding=5,y_padding=5,x_size=16,y_size=16,quadrant_size=16;
    double quadrant_top_padding=5,point_padding=5,point_size=12,point_radius=5;
    double inner_border=1,outer_border=2;
    bool x_top=true,y_left=true,use_max_width=true;
};
struct QuadrantTheme {
    std::array<std::u16string,4> fill,text;
    std::u16string point,point_text,x_text,y_text,title,inner_border,outer_border,font_family,inherited_text;
};
struct QuadrantRenderInput { QuadrantDocument document;QuadrantConfig config;QuadrantTheme theme; };
struct QuadrantText {
    std::u16string text,fill;
    double x{},y{},size{},rotation{};
    bool centered=true,middle=false;
};
struct QuadrantRectangle { double x{},y{},width{},height{};std::u16string fill;QuadrantText text; };
struct QuadrantCircle { double x{},y{},radius{};std::u16string fill,stroke,stroke_width;QuadrantText text; };
struct QuadrantBorder { double x1{},y1{},x2{},y2{},width{};std::u16string fill; };
struct QuadrantLayout {
    std::array<QuadrantRectangle,4> quadrants;
    std::array<QuadrantBorder,6> borders;
    std::vector<QuadrantCircle> points;
    std::vector<QuadrantText> axes;
    std::optional<QuadrantText> title;
};
struct QuadrantSvg { std::string svg;double width{},height{}; };
std::expected<QuadrantRenderInput,std::string> prepare_quadrant(std::u16string_view,const ConfigSession&);
QuadrantLayout layout_quadrant(const QuadrantRenderInput&);
// The original fixed-canvas layout uses font sizes, SVG text-anchor and
// dominant-baseline, not measured glyph widths. No estimated text metrics.
std::expected<QuadrantSvg,std::string> render_quadrant_svg(const QuadrantRenderInput&);
}
