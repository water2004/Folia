#include "pch.h"
#include "editor/interaction/EditorPointerController.h"

import folia.core.input_command;
import folia.core.command;

namespace winrt::Folia
{
    namespace
    {
        using namespace winrt::Microsoft::UI::Input;

        InputCursor SystemSelectionMoveCursor()
        {
            static const std::unique_ptr<std::remove_pointer_t<HMODULE>, decltype(&::FreeLibrary)> ole32{
                ::LoadLibraryExW(L"ole32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32), ::FreeLibrary};
            // OLE's move-drop cursor, with the dashed box next to the arrow.
            constexpr std::uint32_t oleMoveCursor = 2;
            if (ole32 && ::FindResourceW(ole32.get(), MAKEINTRESOURCEW(oleMoveCursor), RT_GROUP_CURSOR))
                return InputDesktopResourceCursor::CreateFromModule(L"ole32.dll", oleMoveCursor);
            return InputSystemCursor::Create(InputSystemCursorShape::Arrow);
        }
    }

    void EditorPointerController::Attach(
        EditorSession& session,
        EditorSurfaceRenderer& renderer,
        EditorScrollController& scroll,
        EditorTextInputController& textInput,
        winrt::Microsoft::UI::Xaml::FrameworkElement const& surface,
        winrt::Microsoft::UI::Xaml::Style const& menuStyle,
        EditorTableInteraction::ClipboardActions clipboard,
        ExecuteCommand executeCommand,
        Render render,
        Render requestFrame,
        OpenLink openLink,
        OpenFootnote openFootnote,
        ResetCaretGoal resetCaretGoal)
    {
        Detach();
        session_ = &session;
        renderer_ = &renderer;
        scroll_ = &scroll;
        textInput_ = &textInput;
        surface_ = surface;
        selectionDrag_.Attach(session, renderer, scroll, textInput);
        using winrt::Microsoft::UI::Input::InputSystemCursor;
        using Shape = winrt::Microsoft::UI::Input::InputSystemCursorShape;
        cursors_ = {InputSystemCursor::Create(Shape::IBeam), InputSystemCursor::Create(Shape::Hand),
            SystemSelectionMoveCursor(), InputSystemCursor::Create(Shape::UniversalNo),
            InputSystemCursor::Create(Shape::SizeWestEast)};
        SetCursor(Cursor::Text);
        executeCommand_ = std::move(executeCommand);
        render_ = std::move(render);
        requestFrame_ = requestFrame;
        openLink_ = std::move(openLink);
        openFootnote_ = std::move(openFootnote);
        resetCaretGoal_ = std::move(resetCaretGoal);
        tablePointer_.Attach(session, renderer, scroll, textInput, surface,
            menuStyle, std::move(clipboard), executeCommand_, openLink_, render_, std::move(requestFrame));
    }

    Microsoft::UI::Xaml::Controls::MenuFlyout EditorPointerController::ContextMenuAt(
        std::optional<Windows::Foundation::Point> point)
    {
        return tablePointer_.ContextMenuAt(point);
    }

    void EditorPointerController::Detach()
    {
        tablePointer_.Detach();
        selectionDrag_.Detach();
        if (surface_)
            if (auto protectedElement = surface_.try_as<winrt::Microsoft::UI::Xaml::IUIElementProtected>())
                protectedElement.ProtectedCursor(nullptr);
        if (renderer_) renderer_->ClearPointer();
        hoverTooltip_.reset();
        hoverTaskCheckbox_.reset();
        hoverTableControls_ = {};
        executeCommand_ = {};
        render_ = {};
        requestFrame_ = {};
        openLink_ = {};
        openFootnote_ = {};
        resetCaretGoal_ = {};
        currentCursor_.reset();
        cursors_.fill(nullptr);
        surface_ = nullptr;
        textInput_ = nullptr;
        scroll_ = nullptr;
        renderer_ = nullptr;
        session_ = nullptr;
    }

    void EditorPointerController::PointerPressed(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
    {
        if (!session_ || !renderer_ || !surface_) return;
        // Primary contact covers mouse-left, touch and the pen tip. Secondary
        // input belongs to the context menu and must not start selection,
        // capture the pointer, activate links or execute table/task actions.
        auto pointerPoint = args.GetCurrentPoint(surface_);
        if (!pointerPoint.Properties().IsLeftButtonPressed()) return;
        surface_.Focus(winrt::Microsoft::UI::Xaml::FocusState::Pointer);
        if (resetCaretGoal_) resetCaretGoal_();
        auto point = pointerPoint.Position();
        if (scroll_) scroll_->Stop();
        renderer_->UpdatePointer(static_cast<float>(point.X), static_cast<float>(point.Y));
        if (tablePointer_.Press(args, point))
        {
            args.Handled(true);
            return;
        }
        using Modifiers = winrt::Windows::System::VirtualKeyModifiers;
        if (args.Pointer().PointerDeviceType() == winrt::Microsoft::UI::Input::PointerDeviceType::Mouse
            && args.KeyModifiers() == Modifiers::None
            && session_->HasSelection() && !session_->Selection().selects_cells()
            && renderer_->SelectionBackgroundAt(static_cast<float>(point.X), static_cast<float>(point.Y)))
        {
            if (auto hit = renderer_->HitTest(static_cast<float>(point.X), static_cast<float>(point.Y));
                hit && surface_.CapturePointer(args.Pointer()))
            {
                selectionDrag_.BeginMove(*hit, static_cast<float>(point.X), static_cast<float>(point.Y));
                args.Handled(true);
                return;
            }
        }
        if (auto checkbox = renderer_->TaskCheckboxAt(static_cast<float>(point.X), static_cast<float>(point.Y)))
        {
            session_->SetSelection(*checkbox, *checkbox);
            if (textInput_) textInput_->NotifySelectionChanged();
            folia::Command command;
            command.kind = folia::CommandKind::ToggleTaskCheckbox;
            if (executeCommand_) executeCommand_(command);
            args.Handled(true);
            return;
        }
        if (auto footnote = renderer_->FootnoteAt(static_cast<float>(point.X), static_cast<float>(point.Y)))
        {
            if (openFootnote_) openFootnote_(std::move(*footnote), point);
            args.Handled(true);
            return;
        }
        auto hit = renderer_->HitTest(static_cast<float>(point.X), static_cast<float>(point.Y));
        if (!hit) return;
        if ((args.KeyModifiers() & winrt::Windows::System::VirtualKeyModifiers::Control) == winrt::Windows::System::VirtualKeyModifiers::Control)
        {
            if (auto link = LinkAtPosition(*hit))
            {
                if (openLink_) openLink_(std::move(*link));
                args.Handled(true);
                return;
            }
        }
        auto const extend = (args.KeyModifiers() & winrt::Windows::System::VirtualKeyModifiers::Shift)
            == winrt::Windows::System::VirtualKeyModifiers::Shift;
        selectionDrag_.Begin(folia::editor_pointer_selection(session_->Selection(), *hit, extend),
            static_cast<float>(point.X), static_cast<float>(point.Y));
        surface_.CapturePointer(args.Pointer());
        if (render_) render_();
        args.Handled(true);
    }

    void EditorPointerController::PointerMoved(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
    {
        if (!session_ || !renderer_ || !surface_) return;
        auto point = args.GetCurrentPoint(surface_).Position();
        renderer_->UpdatePointer(point.X, point.Y);
        if (tablePointer_.Move(args, point))
        {
            args.Handled(true);
            return;
        }
        if (selectionDrag_.Active())
        {
            const bool changed = selectionDrag_.Update(point.X, point.Y);
            using Feedback = EditorSelectionDrag::MoveFeedback;
            switch (selectionDrag_.Feedback())
            {
                case Feedback::Allowed: SetCursor(Cursor::Move); break;
                case Feedback::Blocked: SetCursor(Cursor::Blocked); break;
                case Feedback::None: SetCursor(Cursor::Text); break;
            }
            if (changed && requestFrame_) requestFrame_();
            args.Handled(true);
            return;
        }
        auto checkbox = renderer_->TaskCheckboxAt(point.X, point.Y);
        const auto controls = tablePointer_.Controls(point);
        const auto* control = controls.Hit();
        if (checkbox != hoverTaskCheckbox_ || !std::ranges::equal(controls.Visible(), hoverTableControls_.Visible()))
        {
            hoverTaskCheckbox_ = checkbox;
            hoverTableControls_ = controls;
            if (requestFrame_) requestFrame_();
        }
        auto hit = control ? std::nullopt : renderer_->HitTest(point.X, point.Y);
        auto interaction = hit ? session_->InteractionAt(*hit) : std::nullopt;
        auto tooltip = interaction ? std::move(interaction->tooltip) : std::nullopt;
        const auto interactive = checkbox || control || (interaction && interaction->link)
            || renderer_->CodeLanguageAt(point.X, point.Y).has_value();
        SetCursor(control && control->action.kind == folia::platform::editor::EditorTableActionKind::ResizeColumn
            ? Cursor::ResizeColumn : interactive ? Cursor::Link : Cursor::Text);
        if (tooltip != hoverTooltip_)
        {
            hoverTooltip_ = tooltip;
            Microsoft::UI::Xaml::Controls::ToolTipService::SetToolTip(
                surface_, tooltip ? box_value(to_hstring(*tooltip)) : nullptr);
        }
    }

    void EditorPointerController::PointerReleased(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args)
    {
        if (!session_ || !renderer_ || !surface_) return;
        if (tablePointer_.Release(args, args.GetCurrentPoint(surface_).Position()))
        {
            SetCursor(Cursor::Text);
            args.Handled(true);
            return;
        }
        if (selectionDrag_.Active())
        {
            auto point = args.GetCurrentPoint(surface_).Position();
            auto changed = selectionDrag_.Finish(static_cast<float>(point.X), static_cast<float>(point.Y), executeCommand_);
            SetCursor(Cursor::Text);
            surface_.ReleasePointerCapture(args.Pointer());
            if (changed && render_) render_();
            args.Handled(true);
        }
    }

    void EditorPointerController::PointerExited()
    {
        SetCursor(selectionDrag_.Feedback() == EditorSelectionDrag::MoveFeedback::None ? Cursor::Text : Cursor::Blocked);
        hoverTooltip_.reset();
        hoverTaskCheckbox_.reset();
        hoverTableControls_ = {};
        if (surface_) winrt::Microsoft::UI::Xaml::Controls::ToolTipService::SetToolTip(surface_, nullptr);
        if (!tablePointer_.Active() && renderer_)
        {
            renderer_->ClearPointer();
            if (render_) render_();
        }
    }

    void EditorPointerController::CancelPointerInteraction()
    {
        auto repaint = selectionDrag_.Moving();
        selectionDrag_.Cancel();
        SetCursor(Cursor::Text);
        tablePointer_.Cancel();
        if (repaint && render_) render_();
    }

    bool EditorPointerController::CancelSelectionMove()
    {
        if (!selectionDrag_.Moving() && !tablePointer_.Active()) return false;
        CancelPointerInteraction();
        if (surface_) surface_.ReleasePointerCaptures();
        return true;
    }

    void EditorPointerController::DoubleTapped(winrt::Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const& args)
    {
        if (!renderer_ || !surface_) return;
        surface_.Focus(winrt::Microsoft::UI::Xaml::FocusState::Pointer);
        // A double tap replaces the in-flight drag selection.  Touchpads and
        // some mouse stacks can deliver the gesture before the final release;
        // if that release completes the old drag, it collapses the
        // authoritative source selection after the word was already painted.
        CancelPointerInteraction();
        surface_.ReleasePointerCaptures();
        auto point = args.GetPosition(surface_);
        if (tablePointer_.DoubleTap(point))
        {
            args.Handled(true);
            return;
        }
        auto hit = renderer_->HitTestTextUnit(static_cast<float>(point.X), static_cast<float>(point.Y));
        if (hit && SelectWordAt(*hit))
        {
            if (render_) render_();
            args.Handled(true);
        }
    }

    bool EditorPointerController::SelectWordAt(folia::TextPosition position)
    {
        if (!session_) return false;
        const auto selection = session_->WordSelectionAt(position);
        if (!selection) return false;
        session_->SetSelection(*selection);
        if (textInput_) textInput_->NotifySelectionChanged();
        return true;
    }

    void EditorPointerController::SetCursor(Cursor cursor)
    {
        if (!surface_ || cursor == currentCursor_) return;
        auto protectedElement = surface_.try_as<winrt::Microsoft::UI::Xaml::IUIElementProtected>();
        if (!protectedElement) return;
        protectedElement.ProtectedCursor(cursors_[static_cast<std::size_t>(cursor)]);
        currentCursor_ = cursor;
    }

    std::optional<std::string> EditorPointerController::LinkAtPosition(folia::TextPosition position) const
    {
        if (!session_) return std::nullopt;
        auto interaction = session_->InteractionAt(position);
        return interaction ? std::move(interaction->link) : std::nullopt;
    }

}
