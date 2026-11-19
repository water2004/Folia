export module folia.math.output.scene.types;
export import folia.math.output.scene.html_types;
import std;

export namespace folia::math::output::scene {
// All geometry is in ems. The common layout pass supplies these values;
// consumers serialize/draw them without recomputing mathematical placement.
using Unit = double;
using NodeId = std::uint64_t;

// Describes why a completed element exists so the SVG serializer can retain
// Source transform spelling without inspecting node names or recomputing layout.
enum class ElementRole {
    math_wrapper,
    id_alignment_box,
    operator_adjustment,
    inferred_row_adjustment,
    enclosure_content,
    enclosure_mark,
    table_content_viewport,
    table_label_frame,
    image_glyph,
    unknown_text,
    foreign_content_viewport,
    anonymous_group,
    glyph_composition,
    background_box,
    link_hitbox,
    id_alignment_marker,
    fraction_rule,
    radical_rule,
    vertical_extender_viewport,
    horizontal_extender_viewport,
    table_frame,
    table_vertical_line,
    table_horizontal_line,
    error_background,
};

struct Point {
    Unit x{}, y{};
    friend bool operator==(Point const &, Point const &) = default;
};
struct Size {
    Unit width{}, height{};
    friend bool operator==(Size const &, Size const &) = default;
};
struct Rect {
    Unit x{}, y{}, width{}, height{};
    friend bool operator==(Rect const &, Rect const &) = default;
};
// Column-vector affine transform: (x',y')=(a*x+c*y+e,b*x+d*y+f).
// Local child transform is applied first, then its parent's: world=P*L.
struct Affine {
    Unit a{1}, b{}, c{}, d{1}, e{}, f{};
    friend bool operator==(Affine const &, Affine const &) = default;
};
[[nodiscard]] constexpr Affine compose(Affine const &parent, Affine const &local) noexcept {
    return {parent.a * local.a + parent.c * local.b,
            parent.b * local.a + parent.d * local.b,
            parent.a * local.c + parent.c * local.d,
            parent.b * local.c + parent.d * local.d,
            parent.a * local.e + parent.c * local.f + parent.e,
            parent.b * local.e + parent.d * local.f + parent.f};
}
enum class TranslationSeparator { role_default, comma, space, comma_space };
struct Translation {
    Unit x{}, y{};
    TranslationSeparator separator{TranslationSeparator::role_default};
    friend bool operator==(Translation const &, Translation const &) = default;
};
struct Scaling {
    Unit x{1}, y{1};
    bool explicit_axes{};
    unsigned precision{3};
    friend bool operator==(Scaling const &, Scaling const &) = default;
};
// Degrees are positive counterclockwise in Scene's y-up coordinate system;
// center is interpreted in the operation's current local coordinates.
struct Rotation {
    Unit degrees{};
    Point center{};
    friend bool operator==(Rotation const &, Rotation const &) = default;
};
using TransformOperation = std::variant<Translation, Scaling, Rotation, Affine>;

// The ordered operations are authoritative. matrix() derives the composed
// result when needed; it is never stored as a parallel transform state.
class Transform {
    std::vector<TransformOperation> operations_;

  public:
    Transform() = default;
    Transform(Affine affine) : operations_{std::move(affine)} {}
    explicit Transform(std::vector<TransformOperation> operations)
        : operations_(std::move(operations)) {}
    [[nodiscard]] std::span<TransformOperation const> operations() const noexcept {
        return operations_;
    }
    [[nodiscard]] Affine matrix() const noexcept {
        auto as_affine = [](TransformOperation const &operation) {
            return std::visit(
                [](auto const &value) -> Affine {
                    using T = std::remove_cvref_t<decltype(value)>;
                    if constexpr (std::same_as<T, Translation>)
                        return {1, 0, 0, 1, value.x, value.y};
                    else if constexpr (std::same_as<T, Scaling>)
                        return {value.x, 0, 0, value.y, 0, 0};
                    else if constexpr (std::same_as<T, Rotation>) {
                        auto const radians = value.degrees * std::numbers::pi_v<Unit> / 180.0;
                        auto const cosine = std::cos(radians);
                        auto const sine = std::sin(radians);
                        auto const cx = value.center.x;
                        auto const cy = value.center.y;
                        return {cosine,
                                sine,
                                -sine,
                                cosine,
                                cx - cosine * cx + sine * cy,
                                cy - sine * cx - cosine * cy};
                    } else
                        return value;
                },
                operation);
        };
        auto result = Affine{};
        for (auto const &operation : operations_)
            result = compose(result, as_affine(operation));
        return result;
    }
    friend bool operator==(Transform const &, Transform const &) = default;
};

// Baseline is y=0, x increases right, y increases up. Signed values preserve
// completed common-layout BBox metrics, including negative extents.
struct Metrics {
    Unit width{}, height{}, depth{};
    friend bool operator==(Metrics const &, Metrics const &) = default;
};
struct FontMetrics {
    // em/ex are actual CSS-pixel metrics; x_height/cap_height are em ratios.
    Unit em{1}, ex{}, x_height{}, cap_height{};
    friend bool operator==(FontMetrics const &, FontMetrics const &) = default;
};
struct Color {
    std::uint8_t red{}, green{}, blue{}, alpha{255};
    friend bool operator==(Color const &, Color const &) = default;
};
enum class FillRule { nonzero, evenodd };
struct InheritedPaint {
    friend bool operator==(InheritedPaint, InheritedPaint) = default;
};
struct NoPaint {
    friend bool operator==(NoPaint, NoPaint) = default;
};
struct CurrentColorPaint {
    friend bool operator==(CurrentColorPaint, CurrentColorPaint) = default;
};
struct CssColor {
    std::string value;
    friend bool operator==(CssColor const &, CssColor const &) = default;
};
using PaintColor = std::variant<InheritedPaint, NoPaint, CurrentColorPaint, Color, CssColor>;
struct PaintColors {
    PaintColor fill{InheritedPaint{}};
    PaintColor stroke{InheritedPaint{}};
    friend bool operator==(PaintColors const &, PaintColors const &) = default;
};
enum class StrokeCap { butt, round, square };
enum class StrokeJoin { miter, round, bevel };
struct Paint {
    // Closed CSS/SVG paint alternatives preserve inheritance, explicit none,
    // currentColor, and literal color as distinct semantics.
    PaintColor fill{InheritedPaint{}};
    PaintColor stroke{InheritedPaint{}};
    Unit stroke_width{};
    Unit opacity{1};
    FillRule fill_rule{FillRule::nonzero};
    // Absence inherits the surrounding SVG stroke setting; each explicit
    // value remains distinguishable, including the SVG initial defaults.
    std::optional<StrokeCap> line_cap;
    std::optional<StrokeJoin> line_join;
    // Absence inherits; an empty list explicitly selects a solid stroke.
    // Values are completed em lengths and are serialized without layout math.
    std::optional<std::vector<Unit>> dash_array;
    // Stylesheet declarations are retained separately from authored paint.
    std::optional<PaintColors> stylesheet;
    friend bool operator==(Paint const &, Paint const &) = default;
};

struct MoveTo {
    Point to;
    friend bool operator==(MoveTo const &, MoveTo const &) = default;
};
enum class CoordinateMode { absolute, relative };
struct LineTo {
    Point to;
    // Relative coordinates are completed local deltas, not cached endpoints.
    CoordinateMode mode{CoordinateMode::absolute};
    friend bool operator==(LineTo const &, LineTo const &) = default;
};
struct QuadTo {
    Point control, to;
    friend bool operator==(QuadTo const &, QuadTo const &) = default;
};
struct CubicTo {
    Point control1, control2, to;
    friend bool operator==(CubicTo const &, CubicTo const &) = default;
};
enum class ArcFlagSeparator { space, comma };
struct ArcTo {
    Unit radius_x{}, radius_y{}, rotation{};
    bool large_arc{}, sweep{};
    Point to;
    // Relative coordinates are completed local deltas, not cached endpoints.
    CoordinateMode mode{CoordinateMode::absolute};
    // Preserves the source separator between the two arc flags.
    ArcFlagSeparator flags_separator{ArcFlagSeparator::space};
    friend bool operator==(ArcTo const &, ArcTo const &) = default;
};
struct ClosePath {
    friend bool operator==(ClosePath const &, ClosePath const &) = default;
};
using PathSegment = std::variant<MoveTo, LineTo, QuadTo, CubicTo, ArcTo, ClosePath>;
struct PathGeometry {
    std::vector<PathSegment> segments;
};

struct SemanticAttribute {
    std::string name, value;
};
struct SemanticMetadata {
    std::string source_tag;
    std::string math_role;
    std::string annotation_encoding;
    std::string annotation;
    std::vector<SemanticAttribute> ordered_attributes;
    // Source handleStyles/handleScale has already installed the group's first
    // transform before placing metadata; later placement transforms follow it.
    bool initial_transform{};
};

struct GlyphOutline {
    PathGeometry outline;
};
struct GlyphKey {
    // Semantic selection is distinct from the Source SVG cache namespace:
    // extensions can share cache IDs while supplying different outlines.
    std::string variant;
    std::string variant_cache_id;
    std::uint32_t codepoint{};
    friend bool operator==(GlyphKey const &, GlyphKey const &) = default;
};
class GlyphResource final {
    std::string font_id_;
    std::shared_ptr<GlyphOutline const> outline_;
    GlyphResource(std::string font_id, GlyphOutline outline)
        : font_id_(std::move(font_id)),
          outline_(std::make_shared<GlyphOutline const>(std::move(outline))) {}

  public:
    GlyphResource(GlyphResource const &) = delete;
    GlyphResource &operator=(GlyphResource const &) = delete;
    [[nodiscard]] static std::shared_ptr<GlyphResource const> make(std::string font_id,
                                                                   GlyphOutline outline) {
        return std::shared_ptr<GlyphResource const>(
            new GlyphResource(std::move(font_id), std::move(outline)));
    }
    [[nodiscard]] std::string_view font_id() const noexcept {
        return font_id_;
    }
    [[nodiscard]] GlyphOutline const &outline() const noexcept {
        return *outline_;
    }
};
struct Glyph {
    std::shared_ptr<GlyphResource const> resource;
    GlyphKey key;
    Point origin{};
    Unit advance{};
    Paint paint{};
    Transform transform{};
};
struct GlyphRun {
    std::vector<Glyph> glyphs;
    std::uint64_t resource_order{};
};
enum class PathElement { path, polygon, line };
struct Rule {
    Rect bounds;
    Paint paint{};
    SemanticMetadata metadata{};
    ElementRole role{ElementRole::math_wrapper};
    // Keep the requested radius; shape consumers clamp it to the rectangle.
    std::optional<Unit> corner_radius;
};
enum class PathSyntax { compact, spaced };
enum class PathPaintEncoding { attributes, stroke_width_style };
enum class StrokeJoinSpelling { standard, mitre };
struct Path {
    PathGeometry geometry;
    Paint paint{};
    SemanticMetadata metadata{};
    ElementRole role{ElementRole::math_wrapper};
    PathElement element{PathElement::path};
    // Source table background sizing may target a first-child border path.
    // This completed width does not change the path geometry or stroke width.
    std::optional<Unit> layout_width;
    // Transform is semantic; the following spelling fields affect projection only.
    Transform transform{};
    PathSyntax syntax{PathSyntax::compact};
    PathPaintEncoding paint_encoding{PathPaintEncoding::attributes};
    StrokeJoinSpelling join_spelling{StrokeJoinSpelling::standard};
};
struct Ellipse {
    Point center;
    Unit radius_x{}, radius_y{};
    Paint paint{};
    SemanticMetadata metadata{};
    ElementRole role{ElementRole::math_wrapper};
};
struct TextFont {
    std::string family;
    Unit size{1};
    bool italic{}, bold{};
    std::string variant;
};
struct TextRun {
    std::string utf8;
    Point origin{};
    Metrics metrics{};
    GlyphRun measured_glyphs;
    Paint paint{};
    std::optional<TextFont> font;
    SemanticMetadata metadata{};
    ElementRole role{ElementRole::math_wrapper};
};
enum class ViewportAlignment { min, mid, max };
enum class ViewportFit { meet, slice, none };
struct ViewportAspectRatio {
    ViewportAlignment x{ViewportAlignment::mid}, y{ViewportAlignment::mid};
    ViewportFit fit{ViewportFit::meet};
};
struct Viewport {
    Rect bounds;
    Rect view_box;
    std::optional<ViewportAspectRatio> aspect_ratio;
    bool clip_content{true};
};
struct Clip {
    PathGeometry geometry;
    FillRule fill_rule{FillRule::nonzero};
};
struct Link {
    std::string href;
    std::string target;
    std::string rel;
    // Paint belongs to the link shell; its child groups can override inheritance.
    PaintColors paint{};
};
struct Tooltip {
    std::string text;
};
struct ForeignContent {
    HtmlNodePrimitive content;
    ForeignObjectGeometry geometry;
    SemanticMetadata metadata{};
};
struct Title {
    std::string text;
};
struct DisplayLayout {
    bool display{};
    std::string alignment{"center"};
    Unit shift{}, minimum_width{};
    bool full_width{};
    Unit font_scale{1};
};
enum class InlineBreakKind { opportunity, forced };
struct InlineBreak {
    InlineBreakKind kind{InlineBreakKind::opportunity};
    Unit spacing{};
};
struct InlineFragment {
    NodeId root{};
    Metrics metrics;
    std::optional<InlineBreak> before;
};
struct PercentageFrame {
    Unit percent{100};
    std::string width_text;
    Rect bounds;
    Unit scale{1};
};
struct FrameLayout {
    Unit minimum_extent{0.001};
    std::vector<InlineFragment> inline_fragments;
    std::optional<PercentageFrame> percentage;
};
struct Image {
    Size size;
    std::string href;
    std::string alt;
    Transform transform;
};
using Primitive = std::variant<GlyphRun, Rule, Path, TextRun, Ellipse, Title, Image>;
} // namespace folia::math::output::scene
