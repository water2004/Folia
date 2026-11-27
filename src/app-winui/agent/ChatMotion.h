#pragma once

#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.ViewManagement.h>

namespace winrt::Folia::chat {
    // Animate the newly revealed element after layout. Stream updates retain the same visual.
    inline void Reveal(Microsoft::UI::Xaml::UIElement const& element, float x, float y) {
        if (!Windows::UI::ViewManagement::UISettings().AnimationsEnabled()) return;
        using Microsoft::UI::Xaml::Hosting::ElementCompositionPreview;
        ElementCompositionPreview::SetIsTranslationEnabled(element, true);
        auto visual = ElementCompositionPreview::GetElementVisual(element);
        auto compositor = visual.Compositor();
        auto ease = compositor.CreateCubicBezierEasingFunction({.18f, .75f}, {.25f, 1.f});
        auto slide = compositor.CreateVector3KeyFrameAnimation();
        slide.InsertKeyFrame(0, {x, y, 0});
        slide.InsertKeyFrame(1, {0, 0, 0}, ease);
        slide.Duration(std::chrono::milliseconds(150));
        visual.StartAnimation(L"Translation", slide);
        auto fade = compositor.CreateScalarKeyFrameAnimation();
        fade.InsertKeyFrame(0, .5f);
        fade.InsertKeyFrame(1, 1.f, ease);
        fade.Duration(std::chrono::milliseconds(120));
        visual.StartAnimation(L"Opacity", fade);
    }
}
