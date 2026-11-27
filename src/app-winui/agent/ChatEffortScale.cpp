#include "pch.h"
#include "agent/ChatEffortScale.h"
#include "agent/ChatPalette.h"
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.ViewManagement.h>

namespace winrt::Folia::chat {
    using namespace Microsoft::UI::Composition;
    using Microsoft::UI::Xaml::Hosting::ElementCompositionPreview;

    void ChatEffortScale::Attach(Microsoft::UI::Xaml::FrameworkElement const& host) {
        auto compositor = ElementCompositionPreview::GetElementVisual(host).Compositor();
        face_ = compositor.CreateShapeVisual();
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
        auto bar = [&](Geometry& geometry, Paint const& paint) {
            geometry = compositor.CreateRoundedRectangleGeometry();
            geometry.CornerRadius({10, 10});
            auto shape = compositor.CreateSpriteShape(geometry);
            shape.FillBrush(paint);
            face_.Shapes().Append(shape);
            return shape;
        };
        auto base = bar(rail_, track_);
        rail_.Offset({.5f, 3.5f});
        base.StrokeBrush(border_);
        base.StrokeThickness(1);
        active_ = bar(fill_, accent_);
        fill_.Offset({0, 3});
        ticks_ = compositor.CreateContainerShape();
        face_.Shapes().Append(ticks_);
        auto circle = compositor.CreateEllipseGeometry();
        circle.Center({12, 13});
        circle.Radius({11.5f, 11.5f});
        thumb_ = compositor.CreateSpriteShape(circle);
        thumb_.FillBrush(surface_);
        thumb_.StrokeBrush(border_);
        thumb_.StrokeThickness(1);
        face_.Shapes().Append(thumb_);

        motion_ = compositor.CreatePropertySet();
        motion_.InsertScalar(L"Position", 0);
        motion_.InsertScalar(L"Travel", 0);
        auto bind = [&](auto const& object, wchar_t const* property, wchar_t const* formula) {
            auto expression = compositor.CreateExpressionAnimation(formula);
            expression.SetReferenceParameter(L"scale", motion_);
            object.StartAnimation(property, expression);
        };
        bind(thumb_, L"Offset", L"Vector2(scale.Position * scale.Travel, 0)");
        bind(fill_, L"Size", L"Vector2(scale.Position * (scale.Travel + 24), 20)");
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
        if (!root || width < 24 || height <= 0 || !stops) return;
        const auto density = static_cast<float>(root.RasterizationScale());
        const Windows::Foundation::Numerics::float2 pixels{std::ceil(width * density), std::ceil(height * density)};
        face_.Size({width, height});
        face_.Scale({pixels.x / width, pixels.y / height, 1});
        artwork_.SourceVisual().Size(pixels);
        artwork_.SourceSize(pixels);
        auto compositor = face_.Compositor();
        const auto travel = static_cast<float>(width - 24);
        rail_.Size({static_cast<float>(width) - 1, 19});
        const float position = stops == 1 ? .5f : static_cast<float>(selected.value_or(0)) / static_cast<float>(stops - 1);
        motion_.InsertScalar(L"Travel", travel);
        if (animate && selected && Windows::UI::ViewManagement::UISettings().AnimationsEnabled()) {
            auto motion = compositor.CreateScalarKeyFrameAnimation();
            motion.InsertExpressionKeyFrame(0, L"this.StartingValue");
            motion.InsertKeyFrame(1, position, compositor.CreateCubicBezierEasingFunction({.2f,.8f}, {.25f,1.f}));
            motion.Duration(std::chrono::milliseconds(140));
            motion_.StartAnimation(L"Position", motion);
        } else {
            motion_.StopAnimation(L"Position");
            motion_.InsertScalar(L"Position", position);
        }
        thumb_.Scale(selected ? Windows::Foundation::Numerics::float2{1,1} : Windows::Foundation::Numerics::float2{0,0});
        active_.FillBrush(selected ? accent_ : nullptr);
        auto marks = ticks_.Shapes();
        if (marks.Size() != stops) {
            marks.Clear();
            for (std::size_t i = 0; i < stops; ++i) {
                auto dot = compositor.CreateEllipseGeometry();
                dot.Radius({1.5f, 1.5f});
                marks.Append(compositor.CreateSpriteShape(dot));
            }
        }
        for (unsigned i = 0; i < stops; ++i) {
            auto mark = marks.GetAt(i).as<CompositionSpriteShape>();
            auto fraction = stops == 1 ? .5f : static_cast<float>(i) / static_cast<float>(stops - 1);
            mark.Offset({12 + travel * fraction, 13});
            mark.FillBrush(selected && i < *selected ? onAccent_ : muted_);
        }
    }
}
