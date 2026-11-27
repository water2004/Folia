#include "pch.h"
#include "agent/ChatEffortScale.h"
#include "agent/ChatPalette.h"
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.ViewManagement.h>

namespace winrt::Folia::chat {
    using namespace Microsoft::UI::Composition;
    using Microsoft::UI::Xaml::Hosting::ElementCompositionPreview;

    namespace {
        constexpr float ThumbRadius = 12.f;
        constexpr float TrackRadius = 10.f;
        constexpr float CenterY = 13.f;
    }

    void ChatEffortScale::Attach(Microsoft::UI::Xaml::FrameworkElement const& host) {
        auto compositor = ElementCompositionPreview::GetElementVisual(host).Compositor();
        face_ = compositor.CreateContainerVisual();
        // Flatten the vector artwork before the XAML flyout applies its opening opacity.
        auto drawing = compositor.CreateContainerVisual();
        drawing.Children().InsertAtTop(face_);
        artwork_ = compositor.CreateVisualSurface();
        artwork_.SourceVisual(drawing);
        auto sprite = compositor.CreateSpriteVisual();
        auto brush = compositor.CreateSurfaceBrush(artwork_);
        brush.Stretch(CompositionStretch::Fill);
        sprite.Brush(brush);
        sprite.RelativeSizeAdjustment({1, 1});
        ElementCompositionPreview::SetElementChildVisual(host, sprite);
        accent_ = compositor.CreateColorBrush();
        surface_ = compositor.CreateColorBrush();
        border_ = compositor.CreateColorBrush();
        track_ = compositor.CreateColorBrush();
        muted_ = compositor.CreateColorBrush();
        onAccent_ = compositor.CreateColorBrush();
        auto addLayer = [&] {
            auto layer = compositor.CreateShapeVisual();
            layer.RelativeSizeAdjustment({1, 1});
            face_.Children().InsertAtTop(layer);
            return layer;
        };
        auto addTrack = [&](ShapeVisual const& layer, Geometry& geometry, Paint const& paint, float inset) {
            geometry = compositor.CreateRoundedRectangleGeometry();
            const auto radius = TrackRadius - inset;
            geometry.CornerRadius({radius, radius});
            geometry.Offset({inset, CenterY - radius});
            auto shape = compositor.CreateSpriteShape(geometry);
            shape.FillBrush(paint);
            layer.Shapes().Append(shape);
            return shape;
        };
        auto base = addLayer();
        auto outline = addTrack(base, rail_, track_, .5f);
        outline.StrokeBrush(border_);
        outline.StrokeThickness(1);
        ticks_ = compositor.CreateContainerShape();
        base.Shapes().Append(ticks_);

        filledLayer_ = addLayer();
        addTrack(filledLayer_, filledRail_, accent_, 0);
        filledTicks_ = compositor.CreateContainerShape();
        filledLayer_.Shapes().Append(filledTicks_);
        auto fillClip = compositor.CreateInsetClip();
        filledLayer_.Clip(fillClip);

        thumbLayer_ = addLayer();
        auto circle = compositor.CreateEllipseGeometry();
        circle.Center({0, CenterY});
        circle.Radius({ThumbRadius - .5f, ThumbRadius - .5f});
        auto thumb = compositor.CreateSpriteShape(circle);
        thumb.FillBrush(surface_);
        thumb.StrokeBrush(border_);
        thumb.StrokeThickness(1);
        thumbLayer_.Shapes().Append(thumb);

        // Cut the full rounded track at the thumb center. The moving edge stays
        // under the thumb, even at low stops where a rounded moving cap would show.
        motion_ = compositor.CreatePropertySet();
        motion_.InsertScalar(L"ThumbCenter", ThumbRadius);
        motion_.InsertScalar(L"TrackWidth", 2 * ThumbRadius);
        auto bind = [&](auto const& object, wchar_t const* property, wchar_t const* formula) {
            auto expression = compositor.CreateExpressionAnimation(formula);
            expression.SetReferenceParameter(L"scale", motion_);
            object.StartAnimation(property, expression);
        };
        bind(thumb, L"Offset", L"Vector2(scale.ThumbCenter, 0)");
        bind(fillClip, L"RightInset", L"scale.TrackWidth - scale.ThumbCenter");
    }

    void ChatEffortScale::SetPalette(ChatPalette const& palette) {
        accent_.Color(palette.accent.Color());
        auto color = palette.surface.Color();
        color.A = 255;
        surface_.Color(color);
        track_.Color(palette.track.Color());
        border_.Color(palette.border.Color());
        auto tick = [](auto brush) { auto c = brush.Color(); c.A = static_cast<std::uint8_t>(c.A * .42); return c; };
        muted_.Color(tick(palette.muted));
        onAccent_.Color(tick(palette.onAccent));
    }

    void ChatEffortScale::Draw(Microsoft::UI::Xaml::FrameworkElement const& host, std::size_t stops,
                             std::optional<std::size_t> selected, bool animate) {
        const auto width = static_cast<float>(host.ActualWidth());
        const auto height = static_cast<float>(host.ActualHeight());
        auto root = host.XamlRoot();
        if (!root || width < 2 * ThumbRadius || height <= 0 || !stops) return;
        const auto density = static_cast<float>(root.RasterizationScale());
        const Windows::Foundation::Numerics::float2 pixels{std::ceil(width * density), std::ceil(height * density)};
        face_.Size({width, height});
        face_.Scale({pixels.x / width, pixels.y / height, 1});
        artwork_.SourceVisual().Size(pixels);
        artwork_.SourceSize(pixels);
        auto compositor = face_.Compositor();
        const auto travel = width - 2 * ThumbRadius;
        const auto stopCenter = [&](std::size_t stop) {
            const auto fraction = stops == 1 ? .5f : static_cast<float>(stop) / static_cast<float>(stops - 1);
            return ThumbRadius + travel * fraction;
        };

        rail_.Size({width - 1, 2 * TrackRadius - 1});
        filledRail_.Size({width, 2 * TrackRadius});
        motion_.InsertScalar(L"TrackWidth", width);
        const auto center = stopCenter(selected.value_or(0));
        if (animate && selected && Windows::UI::ViewManagement::UISettings().AnimationsEnabled()) {
            auto motion = compositor.CreateScalarKeyFrameAnimation();
            motion.InsertExpressionKeyFrame(0, L"this.StartingValue");
            motion.InsertKeyFrame(1, center, compositor.CreateCubicBezierEasingFunction({.2f, .8f}, {.25f, 1.f}));
            motion.Duration(std::chrono::milliseconds(140));
            motion_.StartAnimation(L"ThumbCenter", motion);
        } else {
            motion_.StopAnimation(L"ThumbCenter");
            motion_.InsertScalar(L"ThumbCenter", center);
        }
        thumbLayer_.IsVisible(selected.has_value());
        filledLayer_.IsVisible(selected.has_value());

        auto marks = ticks_.Shapes();
        if (marks.Size() != stops) {
            marks.Clear();
            auto filledMarks = filledTicks_.Shapes();
            filledMarks.Clear();
            for (std::size_t i = 0; i < stops; ++i) {
                auto dot = compositor.CreateEllipseGeometry();
                dot.Radius({1.5f, 1.5f});
                auto mark = compositor.CreateSpriteShape(dot);
                mark.FillBrush(muted_);
                marks.Append(mark);
                auto filledMark = compositor.CreateSpriteShape(dot);
                filledMark.FillBrush(onAccent_);
                filledMarks.Append(filledMark);
            }
        }
        for (std::uint32_t i = 0; i < marks.Size(); ++i) {
            auto dot = marks.GetAt(i).as<CompositionSpriteShape>().Geometry().as<CompositionEllipseGeometry>();
            dot.Center({stopCenter(i), CenterY});
        }
    }
}
