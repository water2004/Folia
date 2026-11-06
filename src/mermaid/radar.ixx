// Mermaid 12.2.0 Radar Langium grammar / db.ts. MIT; see ../LICENSE.
export module folia.mermaid.radar;
export import folia.mermaid.frontend;
import std;
export namespace folia::mermaid {
struct RadarAxis { std::u16string name,label; };
struct RadarCurve { std::u16string name,label;std::vector<double> entries; };
struct RadarOptions {
    bool show_legend=true,polygon=false;
    double ticks=5,min=0;
    std::optional<double> max;
};
struct RadarDocument {
    std::u16string title,acc_title,acc_description;
    std::vector<RadarAxis> axes;
    std::vector<RadarCurve> curves;
    RadarOptions options;
};
std::expected<RadarDocument,std::string> parse_radar(std::u16string_view);
}
