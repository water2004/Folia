#include "support/folia_test.hpp"
import folia.math.output.scene.builder;
import folia.math.output.scene.html_snapshot;
import folia.math.adaptors.lite_adaptor;

namespace {
using namespace boost::ut;
using namespace folia::math::output::scene;

suite native_scene_geometry = [] {
    "enclosure geometry values freeze their ownership and preserve typed spelling"_test = [] {
        Path path;
        path.geometry.segments = {MoveTo{{1, 2}}, LineTo{{0.5, -0.25}, CoordinateMode::relative},
                                  ArcTo{0.4,
                                        0.3,
                                        15,
                                        true,
                                        false,
                                        {0.2, 0.1},
                                        CoordinateMode::relative,
                                        ArcFlagSeparator::comma}};
        path.paint.stroke_width = 0.04;
        path.paint.line_join = StrokeJoin::miter;
        path.transform = Transform{std::vector<TransformOperation>{Translation{0.1, 0.2}}};
        path.syntax = PathSyntax::spaced;
        path.paint_encoding = PathPaintEncoding::stroke_width_style;
        path.join_spelling = StrokeJoinSpelling::mitre;
        Rule rounded{{0, 0, 0.467, 0.8}, {}, {}, ElementRole::enclosure_mark, 0.267};
        Ellipse ellipse{{0.4, -0.2}, 0.15, 0.3, {}, {}, ElementRole::enclosure_mark};
        Group root;
        root.entries.emplace_back(Primitive{rounded});
        root.entries.emplace_back(Primitive{path});
        root.entries.emplace_back(Primitive{ellipse});
        Scene scene{root, {}, {}};

        auto &mutable_path = std::get<Path>(std::get<Primitive>(root.entries[1]));
        mutable_path.geometry.segments.clear();
        mutable_path.transform = {};
        auto &mutable_ellipse = std::get<Ellipse>(std::get<Primitive>(root.entries[2]));
        mutable_ellipse.center.x = 99;
        auto const &frozen_rule = std::get<Rule>(std::get<Primitive>(scene.root().entries[0]));
        auto const &frozen_path = std::get<Path>(std::get<Primitive>(scene.root().entries[1]));
        auto const &frozen_ellipse =
            std::get<Ellipse>(std::get<Primitive>(scene.root().entries[2]));
        expect(frozen_rule.corner_radius == 0.267);
        expect(frozen_path.geometry.segments.size() == 3_u);
        expect(std::get<LineTo>(frozen_path.geometry.segments[1]).mode == CoordinateMode::relative);
        auto const &arc = std::get<ArcTo>(frozen_path.geometry.segments[2]);
        expect(arc.mode == CoordinateMode::relative);
        expect(arc.flags_separator == ArcFlagSeparator::comma);
        expect(frozen_path.transform.matrix() == Affine{1, 0, 0, 1, 0.1, 0.2});
        expect(frozen_path.syntax == PathSyntax::spaced);
        expect(frozen_path.paint_encoding == PathPaintEncoding::stroke_width_style);
        expect(frozen_path.join_spelling == StrokeJoinSpelling::mitre);
        expect(frozen_ellipse.center.x == 0.4_d);

        auto rejects_path = [](Path const &candidate) {
            Group invalid_root;
            invalid_root.entries.emplace_back(Primitive{candidate});
            try {
                Scene invalid{std::move(invalid_root), {}, {}};
            } catch (std::invalid_argument const &) {
                return true;
            }
            return false;
        };
        auto invalid_mode = path;
        invalid_mode.geometry.segments = {LineTo{{1, 0}, static_cast<CoordinateMode>(99)}};
        expect(rejects_path(invalid_mode));
        auto invalid_separator = path;
        invalid_separator.geometry.segments = {ArcTo{1,
                                                     1,
                                                     0,
                                                     false,
                                                     true,
                                                     {1, 0},
                                                     CoordinateMode::relative,
                                                     static_cast<ArcFlagSeparator>(99)}};
        expect(rejects_path(invalid_separator));
        auto invalid_transform = path;
        invalid_transform.transform = Transform{
            std::vector<TransformOperation>{Translation{std::numeric_limits<Unit>::infinity(), 0}}};
        expect(rejects_path(invalid_transform));
        auto invalid_syntax = path;
        invalid_syntax.syntax = static_cast<PathSyntax>(99);
        expect(rejects_path(invalid_syntax));
        auto invalid_encoding = path;
        invalid_encoding.paint_encoding = static_cast<PathPaintEncoding>(99);
        expect(rejects_path(invalid_encoding));
        auto invalid_join = path;
        invalid_join.paint.line_join = StrokeJoin::round;
        expect(rejects_path(invalid_join));
        auto invalid_join_encoding = path;
        invalid_join_encoding.paint_encoding = PathPaintEncoding::attributes;
        expect(rejects_path(invalid_join_encoding));
        auto invalid_ellipse = ellipse;
        invalid_ellipse.radius_x = -0.1;
        Group ellipse_root;
        ellipse_root.entries.emplace_back(Primitive{invalid_ellipse});
        bool rejected_ellipse = false;
        try {
            Scene invalid{std::move(ellipse_root), {}, {}};
        } catch (std::invalid_argument const &) {
            rejected_ellipse = true;
        }
        expect(rejected_ellipse);
        invalid_ellipse.radius_x = 0.1;
        invalid_ellipse.radius_y = std::numeric_limits<Unit>::infinity();
        Group nonfinite_ellipse_root;
        nonfinite_ellipse_root.entries.emplace_back(Primitive{invalid_ellipse});
        bool rejected_nonfinite_ellipse = false;
        try {
            Scene invalid{std::move(nonfinite_ellipse_root), {}, {}};
        } catch (std::invalid_argument const &) {
            rejected_nonfinite_ellipse = true;
        }
        expect(rejected_nonfinite_ellipse);
        auto invalid_radius = rounded;
        invalid_radius.corner_radius = std::numeric_limits<Unit>::quiet_NaN();
        Group radius_root;
        radius_root.entries.emplace_back(Primitive{invalid_radius});
        bool rejected_radius = false;
        try {
            Scene invalid{std::move(radius_root), {}, {}};
        } catch (std::invalid_argument const &) {
            rejected_radius = true;
        }
        expect(rejected_radius);
        invalid_radius.corner_radius = -0.1;
        Group negative_radius_root;
        negative_radius_root.entries.emplace_back(Primitive{invalid_radius});
        bool rejected_negative_radius = false;
        try {
            Scene invalid{std::move(negative_radius_root), {}, {}};
        } catch (std::invalid_argument const &) {
            rejected_negative_radius = true;
        }
        expect(rejected_negative_radius);
    };

    "viewport values are validated and deeply frozen with groups and glyphs"_test = [] {
        Viewport viewport{{0.1, -0.2, 0.8, 0.7},
                          {-0.3, -0.4, 1.2, 1.1},
                          ViewportAspectRatio{ViewportAlignment::max, ViewportAlignment::min,
                                              ViewportFit::slice}};
        Group root;
        auto child = std::make_shared<Group>();
        child->id = 1;
        child->viewport = viewport;
        root.entries.emplace_back(std::shared_ptr<Group const>{child});
        Scene scene{root, {1, 0.5, 0.1}, {}};
        child->viewport->bounds.x = 99;
        child->viewport->aspect_ratio->fit = ViewportFit::none;
        auto const &frozen = *std::get<std::shared_ptr<Group const>>(scene.root().entries.front());
        expect(frozen.viewport->bounds.x == 0.1_d);
        expect(frozen.viewport->aspect_ratio->fit == ViewportFit::slice);

        auto rejects_viewport = [](Viewport value) {
            Group invalid_root;
            auto invalid = std::make_shared<Group>();
            invalid->id = 1;
            invalid->viewport = std::move(value);
            invalid_root.entries.emplace_back(std::shared_ptr<Group const>{invalid});
            try {
                Scene rejected{invalid_root, {}, {}};
            } catch (std::invalid_argument const &) {
                return true;
            }
            return false;
        };
        auto invalid = viewport;
        invalid.bounds.width = -1;
        auto invalid_view_box = viewport;
        invalid_view_box.view_box.height = 0;
        auto invalid_aspect = viewport;
        invalid_aspect.aspect_ratio->x = static_cast<ViewportAlignment>(99);
        expect(rejects_viewport(invalid));
        expect(rejects_viewport(invalid_view_box));
        expect(rejects_viewport(invalid_aspect));

        auto resource = GlyphResource::make(
            "font", GlyphOutline{PathGeometry{{MoveTo{{0, 0}}, LineTo{{1, 0}}, ClosePath{}}}});
        Glyph glyph{resource, {"normal", "font", 65},
                    {},       1,
                    {},       Transform{std::vector<TransformOperation>{Scaling{2, 3}}}};
        Group glyph_root;
        glyph_root.entries.emplace_back(Primitive{GlyphRun{{glyph}}});
        Scene glyph_scene{glyph_root, {}, {}};
        glyph.transform = Transform{
            std::vector<TransformOperation>{Translation{std::numeric_limits<Unit>::infinity(), 0}}};
        auto const &saved_glyph =
            std::get<GlyphRun>(std::get<Primitive>(glyph_scene.root().entries.front()))
                .glyphs.front();
        expect(saved_glyph.transform.matrix() ==
               Transform{std::vector<TransformOperation>{Scaling{2, 3}}}.matrix());
        auto invalid_glyph = glyph;
        invalid_glyph.transform = Transform{
            std::vector<TransformOperation>{Translation{std::numeric_limits<Unit>::infinity(), 0}}};
        Group invalid_glyph_root;
        invalid_glyph_root.entries.emplace_back(Primitive{GlyphRun{{invalid_glyph}}});
        bool rejected_glyph = false;
        try {
            Scene rejected{invalid_glyph_root, {}, {}};
        } catch (std::invalid_argument const &) {
            rejected_glyph = true;
        }
        expect(rejected_glyph);

        Builder builder;
        auto id = builder.add_group(0, {}, {}, {}, {}, {}, ElementRole::math_wrapper, {}, viewport);
        expect(builder.child_groups(0) == std::vector<NodeId>{id});
        auto frozen_builder = std::move(builder).freeze();
        expect(std::get<std::shared_ptr<Group const>>(frozen_builder.root().entries.front())
                   ->viewport->view_box == viewport.view_box);
    };

    "percentage frame values are owned and validated"_test = [] {
        auto rejects = [](FrameLayout layout) {
            try {
                Scene invalid{Group{}, {}, {}, {}, std::move(layout)};
            } catch (std::invalid_argument const &) {
                return true;
            }
            return false;
        };
        FrameLayout valid{0.001, {}, PercentageFrame{80, "80%", {2, 0, 4, 1}, 0.375}};
        Scene saved{Group{}, {}, {}, {}, valid};
        valid.percentage->width_text = "mutated";
        expect(saved.frame_layout().percentage->width_text == "80%");
        auto bad_percent = FrameLayout{0.001, {}, PercentageFrame{-1, "80%", {2, 0, 4, 1}, 1}};
        auto nonfinite_percent = FrameLayout{
            0.001,
            {},
            PercentageFrame{std::numeric_limits<Unit>::infinity(), "80%", {2, 0, 4, 1}, 1}};
        auto bad_scale = FrameLayout{0.001, {}, PercentageFrame{80, "80%", {2, 0, 4, 1}, 0}};
        auto bad_bounds = FrameLayout{0.001, {}, PercentageFrame{80, "80%", {2, 0, -1, 1}, 1}};
        auto mixed = FrameLayout{0.001, {{1, {}, {}}}, PercentageFrame{80, "80%", {}, 1}};
        expect(rejects(bad_percent));
        expect(rejects(nonfinite_percent));
        expect(rejects(bad_scale));
        expect(rejects(bad_bounds));
        expect(rejects(mixed));
        Group invalid_scale;
        invalid_scale.transform =
            Transform{std::vector<TransformOperation>{Scaling{1, 1, false, 13}}};
        bool rejected_precision = false;
        try {
            Scene invalid{invalid_scale, {}, {}};
        } catch (std::invalid_argument const &) {
            rejected_precision = true;
        }
        expect(rejected_precision);
    };

    "display and text font values are frozen and validated"_test = [] {
        Builder builder;
        DisplayLayout input_layout{true, "left", 0.25, 4.0, true, 1.5};
        builder.set_display_layout(input_layout);
        input_layout.font_scale = 9;
        auto copied_layout = builder.display_layout();
        expect(copied_layout.font_scale == 1.5_d);
        copied_layout.font_scale = 7;
        expect(builder.display_layout().font_scale == 1.5_d);
        TextRun run{"x", {}, {}, {}, {}};
        run.font = TextFont{"MathJax_Main", 1.25, true, false, "normal"};
        builder.add_primitive(0, run);
        auto scene = std::move(builder).freeze();
        expect(scene.display_layout().display);
        expect(scene.display_layout().alignment == "left");
        expect(scene.display_layout().font_scale == 1.5_d);
        expect(std::get<TextRun>(std::get<Primitive>(scene.root().entries.front())).font->family ==
               "MathJax_Main");
        Group root;
        TextRun invalid{};
        invalid.font = TextFont{"x", std::numeric_limits<Unit>::infinity()};
        root.entries.emplace_back(Primitive{invalid});
        bool rejected = false;
        try {
            Scene bad{root, {}, {}};
        } catch (std::invalid_argument const &) {
            rejected = true;
        }
        expect(rejected);

        auto builder_rejects_scale = [](Unit scale) {
            Builder invalid_builder;
            try {
                invalid_builder.set_display_layout({false, "center", 0, 0, false, scale});
            } catch (std::invalid_argument const &) {
                return true;
            }
            return false;
        };
        auto scene_rejects_scale = [](Unit scale) {
            try {
                Scene invalid_scene{
                    Group{}, {}, {}, DisplayLayout{false, "center", 0, 0, false, scale}};
            } catch (std::invalid_argument const &) {
                return true;
            }
            return false;
        };
        auto const nan = std::numeric_limits<Unit>::quiet_NaN();
        expect(builder_rejects_scale(-1));
        expect(builder_rejects_scale(nan));
        expect(scene_rejects_scale(-1));
        expect(scene_rejects_scale(nan));
        Builder zero_builder;
        zero_builder.set_display_layout({false, "center", 0, 0, false, 0});
        auto zero_scene = std::move(zero_builder).freeze();
        expect(zero_scene.display_layout().font_scale == 0.0_d);
        Scene direct_zero_scene{Group{}, {}, {}, DisplayLayout{false, "center", 0, 0, false, 0}};
        expect(direct_zero_scene.display_layout().font_scale == 0.0_d);
    };

    "frame layout freeze rejects dangling roots and non-finite break spacing"_test = [] {
        auto rejects = [](FrameLayout frames) {
            Group root;
            auto child = std::make_shared<Group>();
            child->id = 1;
            root.entries.emplace_back(std::shared_ptr<Group const>{child});
            try {
                Scene scene{root, {1, 0.5, 0.1}, {}, {}, std::move(frames)};
            } catch (std::invalid_argument const &) {
                return true;
            }
            return false;
        };
        expect(rejects(FrameLayout{0.1, {{2, {1, 0.5, 0.1}, {}}}}));
        expect(rejects(FrameLayout{
            0.1,
            {{1,
              {1, 0.5, 0.1},
              InlineBreak{InlineBreakKind::forced, std::numeric_limits<Unit>::infinity()}}}}));
        expect(rejects(FrameLayout{0, {}}));
    };

    "builder link and viewport getters return owned values"_test = [] {
        Builder builder;
        Viewport viewport{{0.1, -0.2, 0.8, 0.7}, {-0.3, -0.4, 2, 3}, {}};
        auto id = builder.add_group(0, {}, {}, Link{"https://example.test", "_blank", "safe"}, {},
                                    {}, ElementRole::math_wrapper, {}, viewport);
        auto link = builder.link(id);
        auto view = builder.viewport(id);
        expect(link && link->href == "https://example.test");
        expect(view && view->bounds == viewport.bounds);
        link->href = "changed";
        view->bounds.x = 42;
        expect(builder.link(id)->href == "https://example.test");
        expect(builder.viewport(id)->bounds == viewport.bounds);
        expect(!builder.link(0));
        expect(!builder.viewport(0));
        bool rejected = false;
        try {
            (void)builder.link(999);
        } catch (std::out_of_range const &) {
            rejected = true;
        }
        expect(rejected);
    };
};
} // namespace
