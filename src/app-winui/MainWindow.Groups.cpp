#include "pch.h"
import std;
#include "MainWindow.xaml.h"
#include "localization/Localization.h"
#include "workspace/WorkspaceIcons.h"
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>

namespace winrt::Folia::implementation {
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    namespace {
        void SetColumns(Grid const& grid, double left, double gap, bool split) {
            auto columns = grid.ColumnDefinitions();
            auto set = [&](std::uint32_t index, GridLength value) {
                auto column = columns.GetAt(index);
                auto previous = column.Width();
                if (previous.Value != value.Value || previous.GridUnitType != value.GridUnitType) column.Width(value);
            };
            set(0, split ? GridLengthHelper::FromPixels(left) : GridLength{1, GridUnitType::Star});
            set(1, GridLengthHelper::FromPixels(gap));
            set(2, split ? GridLength{1, GridUnitType::Star} : GridLengthHelper::FromPixels(0));
        }

        double GroupWidth(double available, double fraction) {
            const auto minimum = (std::min)(160., available * .5);
            return std::clamp(available * fraction, minimum, available - minimum);
        }

        WorkspaceViewport CaptureViewport(EditorSurfaceRenderer& renderer) {
            WorkspaceViewport result{renderer.ViewportPosition(), renderer.ScrollOffset()};
            if (result.position)
                if (auto bounds = renderer.CaretBounds(*result.position)) result.inset = bounds->top;
            return result;
        }

        void RestoreViewport(EditorSurfaceRenderer& renderer, WorkspaceViewport const& viewport, auto const& draw) {
            if (!viewport.position || !renderer.ScrollToPosition(*viewport.position, EditorScrollAlignment::Start)) {
                if (!viewport.position || !renderer.CaretBounds(*viewport.position)) renderer.SetScrollOffset(viewport.offset);
            }
            if (viewport.position) {
                draw();
                if (auto bounds = renderer.CaretBounds(*viewport.position)) renderer.ScrollBy(bounds->top - viewport.inset);
            }
            draw();
        }

        void IconButton(ContentControl const& button, WorkspaceIcon icon, wchar_t const* resource) {
            button.Content(MakeWorkspaceIcon(icon));
            auto label = Localize(resource);
            Automation::AutomationProperties::SetName(button, label);
            ToolTipService::SetToolTip(button, box_value(label));
        }
    }

    std::uint64_t MainWindow::SelectedEditorView() const {
        return editorGroups.Selected(editorGroups.ActiveGroup());
    }

    std::uint64_t MainWindow::FindDocumentView(std::uint64_t document) const {
        auto group = editorGroups.ActiveGroup();
        if (auto view = editorGroups.ViewFor(group, document)) return view;
        return editorGroups.ViewFor(OtherEditorGroup(group), document);
    }

    void MainWindow::RegisterEditorGroups() {
        IconButton(SplitEditorButton(), WorkspaceIcon::SplitEditor, L"SplitEditor");
        IconButton(OtherDocumentMenu(), WorkspaceIcon::More, L"FileMenu");
        IconButton(OtherSourceMode(), WorkspaceIcon::Source, L"SourceMode");
        Automation::AutomationProperties::SetName(OtherEncoding(), Localize(L"SelectFileEncoding"));
        Automation::AutomationProperties::SetName(EditorGroupSplitter(), Localize(L"ResizeEditorGroups"));
        if (auto splitter = EditorGroupSplitter().try_as<IUIElementProtected>())
            splitter.ProtectedCursor(Microsoft::UI::Input::InputSystemCursor::Create(Microsoft::UI::Input::InputSystemCursorShape::SizeWestEast));
        SplitEditorButton().Click(WindowEvent([this](auto const&, auto const&) { SplitEditor(); }));
        EditorGroupSplitter().DragDelta(WindowEvent([this](auto const&, Primitives::DragDeltaEventArgs const& args) {
            ResizeEditorGroups(args.HorizontalChange());
        }));
        EditorGroupSplitter().DoubleTapped(WindowEvent([this](auto const&, auto const&) {
            splitFraction = .5;
            UpdateEditorGroupLayout();
        }));
        EditorGroupSplitter().KeyDown(WindowEvent([this](auto const&, Input::KeyRoutedEventArgs const& args) {
            using Windows::System::VirtualKey;
            if (args.Key() == VirtualKey::Left || args.Key() == VirtualKey::Right) {
                ResizeEditorGroups(args.Key() == VirtualKey::Left ? -12. : 12.);
                args.Handled(true);
            }
        }));
        DocumentColumn().SizeChanged(WindowEvent([this](auto const&, auto const&) { UpdateEditorGroupLayout(); }));
        RegisterGroupDragSurface();
        OtherEditorSurface().Loaded(WindowEvent([this](auto const&, auto const&) { QueueOtherGroupRender(); }));
        OtherEditorSurface().SizeChanged(WindowEvent([this](auto const&, auto const&) { QueueOtherGroupRender(); }));
        otherDocumentTools.Attach(OtherReadingCard(), OtherDocumentToolbar(), OtherDocumentViewport());
        OtherDocumentMenu().Click(WindowEvent([this](auto const&, auto const&) {
            if (FocusOtherGroup()) DocumentActionsButton().Flyout().ShowAt(DocumentActionsButton());
        }));
        OtherEncoding().Click(WindowEvent([this](auto const&, auto const&) {
            if (FocusOtherGroup() && EncodingButton().IsEnabled()) EncodingButton().Flyout().ShowAt(EncodingButton());
        }));
        OtherSourceMode().Click(WindowEvent([this](auto const&, auto const&) {
            if (FocusOtherGroup() && HasActiveEditor()) ToggleSourceMode();
        }));
        OtherDocumentViewport().PointerPressed(WindowEvent([this](auto const&, Input::PointerRoutedEventArgs const& args) {
            auto properties = args.GetCurrentPoint(OtherDocumentViewport()).Properties();
            for (auto node = args.OriginalSource().try_as<DependencyObject>(); node; node = Media::VisualTreeHelper::GetParent(node))
                if (node == OtherEditorScrollBar()) return;
            if (!properties.IsLeftButtonPressed() || !FocusOtherGroup()) return;
            HandleEditorPointerPressed(args);
        }));
        OtherDocumentViewport().ContextRequested(WindowEvent([this](auto const&, Input::ContextRequestedEventArgs const& args) {
            if (FocusOtherGroup()) ShowEditorContextMenu(args);
        }));
        OtherDocumentViewport().GotFocus(WindowEvent([this](auto const& sender, auto const&) {
            if (sender.template as<UIElement>().FocusState() == FocusState::Keyboard) FocusOtherGroup();
        }));
        OtherDocumentViewport().PointerWheelChanged(WindowEvent([this](auto const&, Input::PointerRoutedEventArgs const& args) {
            auto group = OtherEditorGroup(editorGroups.ActiveGroup());
            auto view = editorGroups.Find(editorGroups.Selected(group));
            if (!view || !otherEditorRenderer || otherRenderedView != view->id) return;
            auto properties = args.GetCurrentPoint(OtherDocumentViewport()).Properties();
            if (properties.IsHorizontalMouseWheel()) return;
            auto delta = -static_cast<float>(properties.MouseWheelDelta());
            if (args.Pointer().PointerDeviceType() == Microsoft::UI::Input::PointerDeviceType::Touchpad)
                otherScrollController.ScrollPreciselyBy(delta);
            else otherScrollController.QueueScrollBy(delta);
            args.Handled(true);
        }));
    }

    void MainWindow::SplitEditor() {
        if (closePending || closingDocument || relocatingFiles || SettingsActive() || editorGroups.IsSplit()) return;
        if (workspaceDocuments.Active()->kind == WorkspaceTabKind::Chat) return;
        SaveEditorView(presentedEditorView);
        auto view = editorGroups.Split();
        if (!view) return;
        workspaceDocuments.Keep(workspaceDocuments.Active()->id);
        splitFraction = .5;
        ActivateView(view);
        RefreshDocumentTabs();
        if (HasActiveEditor()) EditorSurface().Focus(FocusState::Programmatic);
    }

    bool MainWindow::MoveEditorView(std::uint64_t view, EditorGroup group, std::size_t index) {
        if (windowClosed || closePending || closingDocument || relocatingFiles) return false;
        SaveEditorView(presentedEditorView);
        auto moved = editorGroups.Move(view, group, index);
        if (!moved) return false;
        ActivateView(moved);
        RefreshDocumentTabs();
        if (HasActiveEditor()) EditorSurface().Focus(FocusState::Programmatic);
        return true;
    }

    void MainWindow::SaveEditorView(std::uint64_t view) {
        if (auto entry = editorGroups.Find(view); entry && HasActiveEditor()) {
            entry->viewport = CaptureViewport(editorRenderer);
            entry->selection = editorSession().Selection();
        }
    }

    void MainWindow::RestoreEditorView(WorkspaceEditorView const& view) {
        RestoreViewport(editorRenderer, view.viewport, [this] { RenderEditorSurface(); });
    }

    bool MainWindow::FocusOtherGroup() {
        auto view = editorGroups.Selected(OtherEditorGroup(editorGroups.ActiveGroup()));
        if (!view || closePending || closingDocument) return false;
        ActivateView(view);
        if (HasActiveEditor()) EditorSurface().Focus(FocusState::Programmatic);
        return true;
    }

    void MainWindow::ResizeEditorGroups(double delta) {
        auto gap = unbox_value<double>(Root().Resources().Lookup(box_value(L"WorkspaceSplitHandleWidth")));
        const auto available = ReadingMotionFrame().ActualWidth() - gap;
        if (!editorGroups.IsSplit() || available <= 0.) return;
        splitFraction = GroupWidth(available, (GroupWidth(available, splitFraction) + delta) / available) / available;
        UpdateEditorGroupLayout();
    }

    void MainWindow::UpdateEditorGroupLayout() {
        if (windowClosed) return;
        const bool split = editorGroups.IsSplit();
        const auto visible = split ? Visibility::Visible : Visibility::Collapsed;
        const bool activeRight = split && editorGroups.ActiveGroup() == EditorGroup::Right;
        auto gap = split ? unbox_value<double>(Root().Resources().Lookup(box_value(L"WorkspaceSplitHandleWidth"))) : 0.;
        const auto margin = ReadingMotionFrame().Margin();
        const auto width = (std::max)(0., DocumentColumn().ActualWidth() - margin.Left - margin.Right - gap);
        const auto left = GroupWidth(width, splitFraction);
        // The title and reading grids share their origin and divider tracks.
        SetColumns(ReadingMotionFrame(), left, gap, split);
        SetColumns(TabGroups(), left, gap, split);
        Grid::SetColumn(ReadingCard(), activeRight ? 2 : 0);
        Grid::SetColumn(OtherReadingCard(), activeRight ? 0 : 2);
        RightTabViewport().Visibility(visible);
        OtherReadingCard().Visibility(visible);
        EditorGroupSplitter().Visibility(visible);
        SplitEditorButton().Visibility(split ? Visibility::Collapsed : Visibility::Visible);
        Automation::AutomationProperties::SetName(OtherDocumentViewport(), Localize(activeRight ? L"LeftEditorGroup" : L"RightEditorGroup"));
        UpdateReadingGroupCorners();
        UpdateTabTransferAvailability();
        if (!split) {
            CancelGroupDrag();
            ReleaseOtherGroupRenderer();
        }
        ApplyBarLayout();
        QueueOtherGroupRender();
    }

    void MainWindow::UpdateReadingGroupCorners() {
        auto right = TabStripScroller(EditorGroup::Right);
        const bool joinsRightTab = editorGroups.IsSplit() && RightDocumentTabs().SelectedIndex() == 0
            && (!right || right.HorizontalOffset() < .5);
        const bool activeRight = editorGroups.ActiveGroup() == EditorGroup::Right;
        auto set = [](auto const& card, auto const& backdrop, auto const& tint, bool join) {
            auto corners = card.CornerRadius();
            const auto left = join ? 0. : corners.TopRight;
            if (corners.TopLeft == left) return;
            corners.TopLeft = left;
            card.CornerRadius(corners);
            backdrop.CornerRadius(corners);
            tint.CornerRadius(corners);
        };
        set(ReadingCard(), ReadingBackdrop(), ReadingTint(), joinsRightTab && activeRight);
        set(OtherReadingCard(), OtherReadingBackdrop(), OtherReadingTint(), joinsRightTab && !activeRight);
    }

    void MainWindow::ReleaseOtherGroupRenderer() {
        if (otherEditorRenderer) {
            otherAnimationController.Detach();
            otherScrollController.Detach();
            otherEditorRenderer->SetInvalidateCallback({});
            if (!windowClosed) Hosting::ElementCompositionPreview::SetElementChildVisual(OtherEditorSurface(), nullptr);
            otherEditorRenderer.reset();
            otherRenderedView = 0;
        }
        if (otherImageDocument) {
            OtherWorkspaceImage().Source(nullptr);
            otherImageDocument = 0;
        }
    }

    void MainWindow::QueueOtherGroupRender() {
        if (windowClosed || !editorGroups.IsSplit() || otherGroupRenderPending) return;
        otherGroupRenderPending = DispatcherQueue().TryEnqueue([weak = get_weak()] {
            if (auto window = weak.get(); window && !window->windowClosed) {
                window->otherGroupRenderPending = false;
                window->RenderOtherGroup();
            }
        });
    }

    void MainWindow::RenderOtherGroup() {
        auto view = editorGroups.Find(editorGroups.Selected(OtherEditorGroup(editorGroups.ActiveGroup())));
        auto document = view ? workspaceDocuments.Find(view->document) : nullptr;
        if (!document || !OtherEditorSurface().IsLoaded()) return;
        const bool settings = document->kind == WorkspaceTabKind::Settings;
        if (settings || !SettingsActive()) {
            PlaceSettingsView(!settings);
            SettingsViewHost().Visibility(settings ? Visibility::Visible : Visibility::Collapsed);
        }
        const bool image = document->kind == WorkspaceTabKind::Image;
        auto name = document->kind == WorkspaceTabKind::Chat ? to_hstring(document->chatTitle)
            : settings ? Localize(L"Settings") : document->path.empty()
            ? Localize(L"UntitledMarkdown") : winrt::hstring(document->path.filename().c_str());
        OtherToolbarFilename().Text(name);
        auto editor = document->editor.get();
        const auto editorVisibility = editor ? Visibility::Visible : Visibility::Collapsed;
        OtherEditorPane().Visibility(editorVisibility);
        OtherCharacterCount().Visibility(editorVisibility);
        OtherEncoding().Visibility(editorVisibility);
        OtherSourceMode().Visibility(editorVisibility);
        OtherWorkspaceImage().Visibility(image ? Visibility::Visible : Visibility::Collapsed);
        if (image && document == workspaceDocuments.Active() && WorkspaceImage().Source()) {
            otherImageDocument = document->id;
            OtherWorkspaceImage().Source(WorkspaceImage().Source());
        }
        if (otherImageDocument && otherImageDocument != document->id) {
            OtherWorkspaceImage().Source(nullptr);
            otherImageDocument = 0;
        }
        const bool label = (image && !OtherWorkspaceImage().Source()) || (editor && !editor->loaded);
        OtherDocumentLabel().Visibility(label ? Visibility::Visible : Visibility::Collapsed);
        if (label) OtherDocumentLabel().Text(image ? name : Localize(L"PreparingDocument"));
        if (!editor) { otherRenderedView = 0; return; }
        OtherCharacterCount().Text(LocalizeFormat(L"CharacterCount", {to_hstring(editor->session.CharacterCount())}));
        auto const& encoding = editor->session.Encoding();
        auto encodingText = to_hstring(encoding.name);
        if (encoding.byteOrderMark == folia::encoding::DocumentByteOrderMark::Utf8) encodingText = encodingText + L" BOM";
        OtherEncoding().Content(box_value(encodingText));
        OtherEncoding().IsEnabled(editor->session.HasFile() && editor->loaded);
        OtherSourceMode().IsChecked(editor->session.IsSourceMode());
        OtherSourceMode().IsEnabled(editor->loaded);
        auto surface = OtherEditorSurface();
        if (!editor->loaded || surface.ActualWidth() <= 0 || surface.ActualHeight() <= 0) return;

        try {
            if (!otherEditorRenderer) {
                otherEditorRenderer = std::make_unique<EditorSurfaceRenderer>();
                otherEditorRenderer->SetTheme(themeProfile);
                otherEditorRenderer->SetMediaDisplaySettings(appSettings.media);
                otherEditorRenderer->Initialize(surface);
                auto weak = get_weak();
                otherScrollController.Attach(*otherEditorRenderer, OtherEditorScrollBar(), OtherEditorScrollBarColumn(), [weak] {
                    if (auto window = weak.get(); window && !window->windowClosed) window->otherAnimationController.RequestFrame();
                });
                otherScrollController.SetWidth(themeProfile.layout.scrollbar_width);
                otherAnimationController.Attach(*otherEditorRenderer, otherScrollController, DispatcherQueue(), [weak] {
                    if (auto window = weak.get(); window && !window->windowClosed) window->RenderOtherGroup();
                });
                otherEditorRenderer->SetInvalidateCallback([weak] { if (auto window = weak.get()) window->QueueOtherGroupRender(); });
            } else otherEditorRenderer->Resize(surface, surface.ActualWidth(), surface.ActualHeight());
            if (editor->themeRevision != workspaceThemeRevision) {
                editor->session.SetTheme(themeProfile);
                editor->themeRevision = workspaceThemeRevision;
            }
            auto& renderer = *otherEditorRenderer;
            otherAnimationController.PrepareRender();
            auto draw = [&] {
                auto frame = editor->session.RenderFrame();
                frame.selection = folia::TextSelection::caret(frame.selection.active);
                frame.tableSelection.reset();
                renderer.Render(frame);
            };
            if (std::exchange(otherRenderedView, view->id) != view->id) {
                otherScrollController.Stop();
                auto update = renderer.UpdateWithXaml();
                renderer.SetScrollOffset(0);
                draw();
                RestoreViewport(renderer, view->viewport, draw);
            } else draw();
            otherScrollController.Sync();
            view->viewport = CaptureViewport(renderer);
            otherAnimationController.Rendered();
        } catch (winrt::hresult_error const& error) {
            ShowError(LocalizeFormat(L"RenderFailed", {error.message()}));
        } catch (std::exception const& error) {
            ShowError(LocalizeFormat(L"RenderFailed", {to_hstring(error.what())}));
        }
    }
}
