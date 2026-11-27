#include "pch.h"
#include "agent/ChatReplySurface.h"
#include "agent/ChatTextLayout.h"
#include <wil/resource.h>
#include "localization/Localization.h"
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.System.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>

namespace winrt::Folia::chat {
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    std::shared_ptr<ChatReplySurface> ChatReplySurface::Create(std::string const& markdown, std::shared_ptr<ChatMarkdownAppearance const> appearance) {
        auto self = std::make_shared<ChatReplySurface>();
        self->markdown_ = markdown;
        auto text = to_hstring(markdown);
        self->session_.SetText(text);
        self->appearance_ = std::move(appearance);
        Automation::AutomationProperties::SetName(self->paper_, text);
        self->paper_.Name(L"ChatMarkdownReply");
        self->paper_.Height(32);
        self->paper_.Background(Media::SolidColorBrush(Microsoft::UI::Colors::Transparent()));
        self->slice_.IsHitTestVisible(false);
        self->slice_.Height(1);
        self->slice_.Width(1);
        self->paper_.Children().Append(self->slice_);
        auto weak = self->weak_from_this();
        self->paper_.EffectiveViewportChanged([weak](auto const&, EffectiveViewportChangedEventArgs const& args) {
            if (auto item = weak.lock()) {
                auto rect = args.EffectiveViewport();
                item->top_ = rect.Y;
                item->bottom_ = rect.Y + rect.Height;
                item->Draw(false);
                item->UpdateCursor();
            }
        });
        self->paper_.SizeChanged([weak](auto const&, SizeChangedEventArgs const& args) {
            if (args.PreviousSize() != args.NewSize())
                if (auto item = weak.lock()) item->Draw(true);
        });
        self->paper_.Loaded([weak](auto const&, auto const&) { if (auto item = weak.lock()) item->Draw(true); });
        self->BindInput();
        return self;
    }

    void ChatReplySurface::AppendMarkdown(std::string_view addition) {
        if (addition.empty()) return;
        if (!stream_) stream_ = session_.BeginMarkdownStream();
        markdown_.append(addition);
        session_.AppendMarkdown(*stream_, addition);
        Draw(true);
    }

    void ChatReplySurface::Seal() {
        if (stream_ && stream_->accepted != stream_->source.size()) session_.SetText(to_hstring(markdown_));
        stream_.reset();
        Automation::AutomationProperties::SetName(paper_, to_hstring(markdown_));
        Draw(true);
    }

    bool ChatReplySurface::CopySelection() {
        if (!session_.HasSelection()) return false;
        using namespace Windows::ApplicationModel::DataTransfer;
        DataPackage data;
        data.SetText(to_hstring(session_.SelectedDisplayText()));
        Clipboard::SetContent(data);
        return true;
    }

    ChatReplySurface::HoverHit ChatReplySurface::ContentAt(Windows::Foundation::Point point) const {
        if (!renderer_) return {};
        auto pos = renderer_->HitTestTextUnit(point.X, point.Y, folia::platform::editor::TextHitMode::VisibleCharacter);
        if (!pos) return {};
        auto action = session_.InteractionAt(*pos);
        if (action && action->link) {
            auto const& link = *action->link;
            if (link.starts_with("http://") || link.starts_with("https://") || (openDocument && link.starts_with("folia-doc:")))
                return {true, link};
        }
        return {true, {}};
    }

    void ChatReplySurface::UpdateCursor() {
        using Shape = Microsoft::UI::Input::InputSystemCursorShape;
        std::optional<Shape> shape;
        if (pointer_ && session_.HasSelection()) shape = Shape::IBeam;
        else if (pointerInRoot_ && paper_.IsLoaded()) {
            auto local = paper_.XamlRoot().Content().TransformToVisual(slice_).TransformPoint(*pointerInRoot_);
            auto hit = ContentAt(local);
            if (!hit.link.empty()) shape = Shape::Hand;
            else shape = hit.text ? Shape::IBeam : Shape::Arrow;
        }
        if (shape == cursorShape_) return;
        cursorShape_ = shape;
        paper_.as<IUIElementProtected>().ProtectedCursor(shape ? Microsoft::UI::Input::InputSystemCursor::Create(*shape) : nullptr);
    }

    void ChatReplySurface::BindInput() {
        auto weak = weak_from_this();
        paper_.IsTabStop(true);
        auto hover = [weak](auto const&, Input::PointerRoutedEventArgs const& e) {
            if (auto self = weak.lock(); self && e.Pointer().PointerDeviceType() == Microsoft::UI::Input::PointerDeviceType::Mouse) {
                self->pointerInRoot_ = e.GetCurrentPoint(self->paper_.XamlRoot().Content()).Position();
                self->UpdateCursor();
            }
        };
        paper_.PointerEntered(hover);
        paper_.PointerExited([weak](auto const&, Input::PointerRoutedEventArgs const& e) {
            if (e.Pointer().PointerDeviceType() != Microsoft::UI::Input::PointerDeviceType::Mouse) return;
            if (auto self = weak.lock()) { self->pointerInRoot_.reset(); self->UpdateCursor(); }
        });
        paper_.PointerPressed([weak](auto const&, Input::PointerRoutedEventArgs const& e) {
            auto self = weak.lock();
            if (!self || !self->renderer_ || e.Pointer().PointerDeviceType() == Microsoft::UI::Input::PointerDeviceType::Touch) return;
            auto point = e.GetCurrentPoint(self->slice_);
            if (!point.Properties().IsLeftButtonPressed()) return;
            auto p = point.Position();
            auto hit = self->renderer_->HitTest(p.X, p.Y);
            if (!hit) return;
            self->paper_.Focus(FocusState::Pointer);
            if (!self->paper_.CapturePointer(e.Pointer())) return;
            self->pointer_ = e.Pointer().PointerId();
            self->anchor_ = *hit;
            self->pressedLink_ = self->ContentAt(p).link;
            self->session_.SetSelection(*hit, *hit);
            self->Draw(true);
            e.Handled(true);
        });
        paper_.PointerMoved([weak, hover](auto const& sender, Input::PointerRoutedEventArgs const& e) {
            hover(sender, e);
            auto self = weak.lock();
            if (!self || self->pointer_ != e.Pointer().PointerId() || !self->anchor_) return;
            auto p = e.GetCurrentPoint(self->slice_).Position();
            if (auto hit = self->renderer_->HitTest(p.X, p.Y)) {
                self->session_.SetSelection(*self->anchor_, *hit);
                if (self->session_.HasSelection()) self->pressedLink_.clear();
                self->Draw(true);
            }
            e.Handled(true);
        });
        paper_.PointerReleased([weak](auto const&, Input::PointerRoutedEventArgs const& e) -> fire_and_forget {
            auto self = weak.lock();
            if (!self || self->pointer_ != e.Pointer().PointerId() || !self->anchor_) co_return;
            auto p = e.GetCurrentPoint(self->slice_).Position();
            auto link = self->ContentAt(p).link;
            const bool open = !link.empty() && link == self->pressedLink_ && !self->session_.HasSelection();
            self->paper_.ReleasePointerCapture(e.Pointer());
            self->pointer_.reset();
            self->UpdateCursor();
            e.Handled(true);
            if (!open) co_return;
            if (link.starts_with("folia-doc:")) self->openDocument(link);
            else {
                try { co_await Windows::System::Launcher::LaunchUriAsync(Windows::Foundation::Uri(to_hstring(link))); }
                catch (hresult_error const&) {}
            }
        });
        paper_.PointerCaptureLost([weak](auto const&, auto const&) {
            if (auto self = weak.lock()) { self->pointer_.reset(); self->pressedLink_.clear(); self->UpdateCursor(); }
        });
        Input::KeyboardAccelerator copyKey;
        copyKey.Modifiers(Windows::System::VirtualKeyModifiers::Control);
        copyKey.Key(Windows::System::VirtualKey::C);
        copyKey.ScopeOwner(paper_);
        copyKey.Invoked([weak](auto const&, auto const& args) {
            if (auto self = weak.lock()) args.Handled(self->CopySelection());
        });
        paper_.KeyboardAccelerators().Append(copyKey);
        MenuFlyout menu;
        MenuFlyoutItem copy;
        copy.Text(Localize(L"Copy"));
        copy.Click([weak](auto const&, auto const&) {
            if (auto self = weak.lock(); self && !self->CopySelection()) {
                using namespace Windows::ApplicationModel::DataTransfer;
                DataPackage data;
                data.SetText(to_hstring(self->markdown_));
                Clipboard::SetContent(data);
            }
        });
        menu.Items().Append(copy);
        paper_.ContextFlyout(menu);
    }

    void ChatMarkdownAppearance::SetTheme(folia::ThemeProfile const& profile) {
        theme = profile;
        theme.layout.document_horizontal_padding = 1;
        theme.layout.document_vertical_padding = 2;
        theme.layout.block_gap = 6;
        auto scale = static_cast<float>(chatBodyFontSize) / (std::max)(1.f, theme.typography.body.size);
        for (auto* font : {&theme.typography.body, &theme.typography.code, &theme.typography.heading1,
                           &theme.typography.heading2, &theme.typography.heading3}) {
            font->size *= scale;
            font->line_height *= scale;
        }
        ++themeRevision;
    }

    void ChatReplySurface::Draw(bool dirty) {
        needsRedraw_ |= dirty;
        if (drawing_ || !paper_.IsLoaded() || paper_.ActualWidth() < 2) return;
        auto height = paper_.Height();
        if (bottom_ <= 0 || top_ >= height || !std::isfinite(bottom_) || bottom_ <= top_) return;
        drawing_ = true;
        auto reset = wil::scope_exit([&] { drawing_ = false; });
        constexpr double overscan = 160;
        auto top = (std::clamp)(double(top_), 0., height);
        auto bottom = (std::clamp)(double(bottom_), top, height);
        auto start = Canvas::GetTop(slice_);
        if (!needsRedraw_ && renderer_ && top >= start && bottom <= start + slice_.Height()) return;
        needsRedraw_ = false;
        const bool firstFrame = !renderer_;
        if (firstFrame) renderer_ = std::make_unique<EditorSurfaceRenderer>();
        if (appliedTheme_ != appearance_->themeRevision) {
            session_.SetTheme(appearance_->theme);
            renderer_->SetTheme(appearance_->theme);
            appliedTheme_ = appearance_->themeRevision;
        }
        renderer_->SetMediaDisplaySettings(appearance_->media);
        renderer_->SetTextLayout(appearance_->textLayout);
        if (firstFrame) {
            renderer_->Initialize(slice_);
            renderer_->SetInvalidateCallback([weak = weak_from_this()] { if (auto item = weak.lock()) item->Draw(true); });
        }
        auto y = (std::max)(0., std::floor(top - overscan));
        auto extent = (std::max)(1., std::ceil((std::min)(height, bottom + overscan) - y));
        slice_.Width(paper_.ActualWidth());
        slice_.Height(extent);
        Canvas::SetTop(slice_, y);
        renderer_->Resize(slice_, paper_.ActualWidth(), extent);
        renderer_->SetScrollOffset(static_cast<float>(y));
        auto frame = session_.RenderFrame();
        if (!session_.HasSelection()) frame.selection = {};
        frame.allowImageResources = false;
        frame.readOnlyPresentation = true;
        renderer_->Render(frame);
        auto measured = (std::max)(1., std::ceil(static_cast<double>(renderer_->DocumentHeight())));
        if (height != measured) paper_.Height(measured);
        if (dirty) UpdateCursor();
    }
}
