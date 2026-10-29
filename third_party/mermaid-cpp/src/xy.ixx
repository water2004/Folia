// Mermaid 12.2.0 xychart parser and DB. MIT; see ../LICENSE.
export module folia.mermaid.xy;
export import folia.mermaid.frontend;
import std;
export namespace folia::mermaid {
struct XYAxis { bool band=false;std::u16string title;double min=0,max=0;std::vector<std::u16string> categories; };
struct XYPoint { std::u16string category,label;double x=0,y=0; };
struct XYPlot { bool bar=false;std::u16string title;std::vector<XYPoint> points; };
struct XYDocument {
    std::u16string title,acc_title,acc_description;
    std::optional<bool> horizontal;
    XYAxis x{true},y;
    std::vector<XYPlot> plots;
};
std::expected<XYDocument,std::string> parse_xy(std::u16string_view);
}
