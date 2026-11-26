#pragma once

import folia.platform.editor_interaction;
import folia.core.command;
#include "editor/interaction/EditorTableInteraction.h"

namespace winrt::Folia
{
    struct EditorSession;
    struct EditorSurfaceRenderer;
    struct EditorScrollController;
    struct EditorTextInputController;

    class EditorTablePointerController
    {
    public:
        using Execute = std::function<bool(folia::Command const&)>;
        using OpenLink = std::function<void(std::string)>;
        using PointerArgs = Microsoft::UI::Xaml::Input::PointerRoutedEventArgs;
        using Point = Windows::Foundation::Point;
        using Action = folia::platform::editor::EditorTableAction;
        using ControlSet = folia::platform::editor::EditorTableControls;

        void Attach(EditorSession&, EditorSurfaceRenderer&, EditorScrollController&, EditorTextInputController&,
            Microsoft::UI::Xaml::FrameworkElement const&, Microsoft::UI::Xaml::Style const&,
            EditorTableInteraction::ClipboardActions, Execute, OpenLink,
            std::function<void()> render, std::function<void()> requestFrame);
        void Detach();
        Microsoft::UI::Xaml::Controls::MenuFlyout ContextMenuAt(std::optional<Point> point);
        bool Press(PointerArgs const&, Point);
        bool Move(PointerArgs const&, Point);
        bool Release(PointerArgs const&, Point);
        bool DoubleTap(Point);
        bool Cancel();
        bool Active() const { return press.has_value(); }
        ControlSet Controls(Point) const;

    private:
        struct Range { folia::TextPosition anchor; folia::NodeId table; };
        struct Pressed
        {
            folia::platform::editor::EditorTableGesture gesture;
            std::uint64_t revision;
            std::variant<Range, Action> target;
        };
        using Menu = EditorTableInteraction::MenuScope;
        enum class SelectionUpdate { Frame, Immediate, Drag };
        void ShowMenu(Point, Menu, folia::TextPosition, bool preserveSelection);
        std::function<bool()> SelectionGuard() const;
        Execute CurrentCommand() const;
        bool ApplyControl(Action const&);
        bool ChangeSelection(folia::TextSelection, SelectionUpdate = SelectionUpdate::Frame);
        void ExtendRange(Range const&, Point, bool autoScroll);

        EditorSession* session = nullptr;
        EditorSurfaceRenderer* renderer = nullptr;
        EditorScrollController* scroll = nullptr;
        EditorTextInputController* textInput = nullptr;
        Microsoft::UI::Xaml::FrameworkElement surface{nullptr};
        Microsoft::UI::Xaml::Style menuStyle{nullptr};
        EditorTableInteraction::ClipboardActions clipboard;
        Execute execute;
        OpenLink openLink;
        std::function<void()> render;
        std::function<void()> requestFrame;
        std::shared_ptr<int> lifetime;
        std::optional<Pressed> press;
        std::optional<std::size_t> dropIndex;
        Point lastPoint{};
        bool autoScrolling = false;
        bool pendingSelectionNotification = false;
    };
}
