// Seeded RoughJS 4.6.6 ellipse and hatch paths. See THIRD_PARTY_NOTICES.md.
export module folia.mermaid.rough_hachure;
import std;
export import folia.mermaid.rough_rectangle;

export namespace folia::mermaid {
struct RoughArc {
    RoughPoint start, end;
    double radius;
    bool large, sweep;
};

struct RoughEllipsePaths {
    std::string fill_path, stroke_path;
};

RoughEllipsePaths rough_ellipse_hachure(RoughPoint center, double width, double height,
    double angle, std::uint32_t seed);
std::string rough_arc_cross_hatch(std::span<const RoughArc> arcs, std::uint32_t seed);
}
