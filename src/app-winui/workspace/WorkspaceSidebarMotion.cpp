#include "pch.h"
import std;
#include "workspace/WorkspaceSidebarMotion.h"
#include <winrt/Microsoft.UI.Xaml.Hosting.h>

namespace winrt::Folia {
    using namespace Microsoft::UI::Composition;
    using namespace Microsoft::UI::Xaml;
    using Hosting::ElementCompositionPreview;
    using Media::CompositionTarget;
    using namespace std::chrono_literals;

    void WorkspaceSidebarMotion::Attach(UIElement const& workspace, Controls::ColumnDefinition const& column,
        FrameworkElement const& pane, UIElement const& layout, UIElement const& document,
        std::function<void()> settled) {
        column_ = column;
        pane_ = pane;
        settled_ = std::move(settled);
        width_ = static_cast<float>(pane.Width());
        compositor_ = ElementCompositionPreview::GetElementVisual(workspace).Compositor();
        auto slot = ElementCompositionPreview::GetElementVisual(layout);
        reveal_ = compositor_.CreatePropertySet();
        reveal_.InsertScalar(L"Edge", 0.f);
        reveal_.InsertScalar(L"PaneWidth", width_);
        layout_ = compositor_.CreatePropertySet();
        layout_.InsertScalar(L"Shift", 0.f);

        // The column is the settled layout. The compositor places the artwork
        // at Edge even when XAML commits a different column offset.
        auto shift = compositor_.CreateExpressionAnimation(L"reveal.Edge - slot.Offset.X");
        shift.SetReferenceParameter(L"reveal", reveal_);
        shift.SetReferenceParameter(L"slot", slot);
        layout_.StartAnimation(L"Shift", shift);
        Translate(document, L"Vector3(layout.Shift, 0, 0)");
        auto paneClip = compositor_.CreateInsetClip();
        paneClip.StartAnimation(L"RightInset", Expression(L"reveal.PaneWidth - reveal.Edge"));
        ElementCompositionPreview::GetElementVisual(pane_).Clip(paneClip);
    }

    ExpressionAnimation WorkspaceSidebarMotion::Expression(winrt::hstring const& code) const {
        auto expression = compositor_.CreateExpressionAnimation(code);
        expression.SetReferenceParameter(L"reveal", reveal_);
        expression.SetReferenceParameter(L"layout", layout_);
        return expression;
    }

    void WorkspaceSidebarMotion::Translate(UIElement const& element, winrt::hstring const& code) const {
        ElementCompositionPreview::SetIsTranslationEnabled(element, true);
        auto visual = ElementCompositionPreview::GetElementVisual(element);
        visual.Properties().StartAnimation(L"Translation", Expression(code));
    }

    void WorkspaceSidebarMotion::ClipTitle(UIElement const& frame, UIElement const& caption) {
        auto clip = compositor_.CreateInsetClip();
        auto expression = Expression(L"caption.Size.X + Max(0, layout.Shift)");
        expression.SetReferenceParameter(L"caption", ElementCompositionPreview::GetElementVisual(caption));
        clip.StartAnimation(L"RightInset", expression);
        ElementCompositionPreview::GetElementVisual(frame).Clip(clip);
    }

    void WorkspaceSidebarMotion::ClipReading(UIElement const& frame) {
        auto clip = compositor_.CreateInsetClip();
        clip.StartAnimation(L"RightInset", Expression(L"Max(0, layout.Shift)"));
        ElementCompositionPreview::GetElementVisual(frame).Clip(clip);
    }

    void WorkspaceSidebarMotion::PinTrailing(std::initializer_list<UIElement> elements, UIElement const& leadingLabel) {
        for (auto const& element : elements) Translate(element, L"Vector3(-layout.Shift, 0, 0)");
        auto clip = compositor_.CreateInsetClip();
        clip.StartAnimation(L"RightInset", Expression(L"Max(0, layout.Shift)"));
        ElementCompositionPreview::GetElementVisual(leadingLabel).Clip(clip);
    }

    void WorkspaceSidebarMotion::SetOpen(bool open, bool animate) {
        if (open_ == open) return;
        open_ = open;
        CancelCallbacks();
        if (!animate) {
            Settle();
            return;
        }
        pane_.Visibility(Visibility::Visible);
        pane_.IsHitTestVisible(false);
        if (!open && column_.Width().Value != 0) {
            // On close, give the document its wider layout before starting the
            // compositor transition. The clip keeps its right edge in place.
            ReserveWidth(0);
            frame_ = CompositionTarget::Rendered([this](auto const&, auto const&) {
                CompositionTarget::Rendered(std::exchange(frame_, {}));
                Animate();
            });
        } else {
            Animate();
        }
    }

    void WorkspaceSidebarMotion::Animate() {
        auto animation = compositor_.CreateScalarKeyFrameAnimation();
        animation.InsertExpressionKeyFrame(0.f, L"this.StartingValue");
        animation.InsertKeyFrame(1.f, open_ ? width_ : 0.f,
            compositor_.CreateCubicBezierEasingFunction({.22f, .86f}, {.25f, 1.f}));
        animation.Duration(open_ ? 220ms : 190ms);
        batch_ = compositor_.CreateScopedBatch(CompositionBatchTypes::Animation);
        reveal_.StartAnimation(L"Edge", animation);
        completed_ = batch_.Completed([this](auto const& sender, auto const&) {
            if (sender == batch_) Settle();
        });
        batch_.End();
    }

    void WorkspaceSidebarMotion::ReserveWidth(double width) {
        column_.Width(GridLength{width, GridUnitType::Pixel});
    }

    void WorkspaceSidebarMotion::Settle() {
        CancelCallbacks();
        reveal_.StopAnimation(L"Edge");
        const float width = open_ ? width_ : 0.f;
        reveal_.InsertScalar(L"Edge", width);
        ReserveWidth(width);
        pane_.Visibility(open_ ? Visibility::Visible : Visibility::Collapsed);
        pane_.IsHitTestVisible(open_);
        settled_();
    }

    void WorkspaceSidebarMotion::CancelCallbacks() {
        if (batch_) {
            batch_.Completed(std::exchange(completed_, {}));
            batch_ = nullptr;
        }
        if (frame_.value) CompositionTarget::Rendered(std::exchange(frame_, {}));
    }

    void WorkspaceSidebarMotion::Stop() {
        CancelCallbacks();
        if (!reveal_) return;
        reveal_.StopAnimation(L"Edge");
        layout_.StopAnimation(L"Shift");
        reveal_ = nullptr;
        layout_ = nullptr;
        compositor_ = nullptr;
        pane_ = nullptr;
        column_ = nullptr;
        settled_ = {};
    }
}
