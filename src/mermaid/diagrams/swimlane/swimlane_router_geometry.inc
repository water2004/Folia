// Private geometry shared by implementation units of folia.mermaid.swimlane_router.
#pragma once

namespace folia::mermaid::swimlane_routing_detail {

using Point = FlowchartPoint;
using Index = std::size_t;
inline constexpr double eps = 1e-6;
inline constexpr double geometry_eps = 1e-3;
inline constexpr double anchor_offset = 20;
inline constexpr double inf = std::numeric_limits<double>::infinity();

enum class Side { top, bottom, left, right };
inline bool vertical(Side side) { return side == Side::top || side == Side::bottom; }
inline Index side_index(Side side) { return static_cast<Index>(side); }
inline bool same(double a, double b, double tolerance = eps) { return std::abs(a - b) < tolerance; }
inline bool same(Point a, Point b, double tolerance = eps) {
    return same(a.x, b.x, tolerance) && same(a.y, b.y, tolerance);
}
inline int sign(double value) { return (value > 0) - (value < 0); }
inline double distance(Point a, Point b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); }
inline Point center(const FlowchartPlacedNode& box) { return {box.x, box.y}; }

struct Rect { double left, right, top, bottom; };
inline Rect rect(const FlowchartPlacedNode& b, double margin = 0) {
    return {b.x - b.width / 2 - margin, b.x + b.width / 2 + margin,
            b.y - b.height / 2 - margin, b.y + b.height / 2 + margin};
}
inline bool inside(Point p, Rect r) {
    return p.x > r.left && p.x < r.right && p.y > r.top && p.y < r.bottom;
}

struct TrackSegment {
    Index edge, index;
    Point a, b;
    bool horizontal;
};

Point port(const FlowchartPlacedNode& box, Side side, double offset);
std::vector<Point> simplify(std::vector<Point> points);
Rect inflate(Rect r, double amount);
bool within(double v, double lo, double hi);
bool overlaps(Rect a, Rect b);
bool contains(Rect a, Rect b);
Rect segment_bounds(Point a, Point b);
double overlap_length(double a, double b, double c, double d);
std::vector<Point> distinct_points(std::span<const Point> points);
std::vector<Point> simplify_candidate(std::span<const Point> points);
std::vector<TrackSegment> tracks_for(Index edge, std::span<const Point> points,
                                     double tolerance = geometry_eps);
bool strictly_crosses(const TrackSegment& a, const TrackSegment& b);
bool crosses_at_segment(const TrackSegment& a, const TrackSegment& b, double tolerance);
double shared_axis_length(const TrackSegment& a, const TrackSegment& b, double tolerance);
bool real_node(const SwimlaneRoutingNode& node);
bool line_hits_real_node(Point a, Point b, std::span<const SwimlaneRoutingNode> nodes,
                         Index exclude_start, Index exclude_end, double margin);
std::vector<Point> same_side_path(Point a, Side side, Point b, double track);
std::vector<Point> port_path(Point a, Side start, Point b, Side end, double anchor);
inline constexpr std::array cardinal_sides{Side::top, Side::bottom, Side::left, Side::right};
bool incident_edges(const SwimlaneRoutingEdge& a, const SwimlaneRoutingEdge& b);
Index bends(std::span<const TrackSegment> segments);

} // namespace folia::mermaid::swimlane_routing_detail
