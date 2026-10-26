#pragma once
#include "workspace/FolderExplorerController.h"
#include "workspace/FolderEntry.h"

namespace winrt::Folia {
    struct FolderExplorerController::State {
        Microsoft::UI::Xaml::Controls::TreeView tree{nullptr};
        OpenFile open;
        ReportError report;
        OperationHandler operate;
        std::filesystem::path root;
        std::uint64_t generation = 0;
        std::stop_source stop;
        Microsoft::UI::Xaml::Input::ICommand refresh{nullptr}, collapse{nullptr}, newFile{nullptr}, newFolder{nullptr};
        winrt::event_token expanding{}, collapsed{}, invoked{}, doubleTapped{}, keyDown{};
        winrt::event_token lostFocus{}, layoutUpdated{}, contextRequested{}, pointerEntered{}, pointerExited{}, selectionChanged{}, dragStarting{}, dragOver{}, drop{}, dragLeave{};
        Microsoft::UI::Xaml::Controls::TreeViewNode editingNode{nullptr};
        bool preparingEdit = false, committingEdit = false, pendingEditFocus = false, selectEditName = false, operationBusy = false;
        bool guidesVisible = false;
        Microsoft::UI::Dispatching::DispatcherQueueTimer hoverTimer{nullptr};
        std::uint64_t editSerial = 0;
        Microsoft::UI::Xaml::Controls::TreeViewNode hoverFolder{nullptr};
        std::filesystem::path pendingSelection;
        winrt::weak_ref<Microsoft::UI::Xaml::Controls::TreeViewNode> revealNode;
        bool revealParentRequested = false;
        void NotifyCommands() {
            if (!tree) return;
            for (auto const& command : {refresh, collapse, newFile, newFolder})
                if (command) winrt::get_self<explorer_detail::RootCommand>(command)->Notify();
        }
    };
}
