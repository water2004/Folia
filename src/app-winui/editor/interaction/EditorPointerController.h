#pragma once

#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/interaction/EditorScrollController.h"
#include "editor/interaction/EditorSelectionDrag.h"
#include "editor/interaction/EditorTextInputController.h"
#include "editor/interaction/EditorTablePointerController.h"

namespace winrt::Folia
{
    using folia::platform::editor::EditorVisualFootnoteHit;

    struct EditorPointerController
    {
        using ExecuteCommand = std::function<bool(folia::Command const&)>;
        using Render = std::function<void()>;
        using OpenLink = std::function<void(std::string)>;
        using OpenFootnote = std::function<void(EditorVisualFootnoteHit, winrt::Windows::Foundation::Point)>;
        using ResetCaretGoal = std::function<void()>;

        void Attach(
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
            ResetCaretGoal resetCaretGoal);
        void Detach();
        Microsoft::UI::Xaml::Controls::MenuFlyout ContextMenuAt(std::optional<Windows::Foundation::Point> point);
        void PointerPressed(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void PointerMoved(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void PointerReleased(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void PointerExited();
        void CancelPointerInteraction();
        bool CancelSelectionMove();
        void DoubleTapped(winrt::Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const& args);

    private:
        enum class Cursor { Text, Link, Move, Blocked, ResizeColumn };
        bool SelectWordAt(folia::TextPosition position);
        void SetCursor(Cursor cursor);
        std::optional<EditorDocumentInteraction> InteractionAtPoint(winrt::Windows::Foundation::Point point) const;

        EditorSession* session_ = nullptr;
        EditorSurfaceRenderer* renderer_ = nullptr;
        EditorScrollController* scroll_ = nullptr;
        EditorTextInputController* textInput_ = nullptr;
        EditorSelectionDrag selectionDrag_;
        EditorTablePointerController tablePointer_;
        winrt::Microsoft::UI::Xaml::FrameworkElement surface_{ nullptr };
        std::array<winrt::Microsoft::UI::Input::InputCursor, 5> cursors_{nullptr, nullptr, nullptr, nullptr, nullptr};
        std::optional<Cursor> currentCursor_;
        ExecuteCommand executeCommand_;
        Render render_;
        Render requestFrame_;
        OpenLink openLink_;
        OpenFootnote openFootnote_;
        ResetCaretGoal resetCaretGoal_;
        std::optional<std::string> hoverTooltip_;
        std::optional<folia::TextPosition> hoverTaskCheckbox_;
        folia::platform::editor::EditorTableControls hoverTableControls_;
    };
}
