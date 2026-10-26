#pragma once
#include "workspace/WorkspaceFileOperations.h"

namespace winrt::Folia {
    // One directory per request. The tree owns only the entries the user has
    // expanded; enumeration and sorting never run on the XAML thread.
    class FolderExplorerController {
    public:
        using OpenFile = std::function<void(std::filesystem::path, bool keep)>;
        using ReportError = std::function<void(winrt::hstring)>;
        using OperationHandler = std::function<Windows::Foundation::IAsyncOperation<winrt::hstring>(WorkspaceFileAction)>;
        FolderExplorerController();
        ~FolderExplorerController();
        void Attach(winrt::Microsoft::UI::Xaml::Controls::TreeView const& tree,
            OpenFile open, ReportError report);
        void Detach();
        void Open(std::filesystem::path path);
        void Expand(winrt::Microsoft::UI::Xaml::Controls::TreeViewNode const& node);
        void Refresh();
        void Collapse();
        void SetOperationHandler(OperationHandler handler);
        void NewFile();
        void NewFolder();
        void RenameSelected();
        void CancelNameEdit();
        bool IsEditing() const noexcept;
        bool IsBusy() const noexcept;
        Windows::Foundation::IAsyncOperation<bool> CommitNameAsync(winrt::hstring name);
        Windows::Foundation::IAsyncOperation<winrt::hstring> TransferAsync(
            std::filesystem::path source, std::filesystem::path targetFolder, bool directory, bool copy);
        std::filesystem::path RootPath() const;

    private:
        struct State;
        static void OpenRoot(std::shared_ptr<State> const& state, std::filesystem::path path);
        static void RefreshLoadedNodes(std::shared_ptr<State> const& state);
        static void CollapseNodes(std::shared_ptr<State> const& state);
        static Windows::Foundation::IAsyncAction Populate(std::shared_ptr<State> state,
            Microsoft::UI::Xaml::Controls::TreeViewNode node);
        static Windows::Foundation::IAsyncAction PopulateAsync(std::shared_ptr<State> state,
            winrt::Microsoft::UI::Xaml::Controls::TreeViewNode node);
        static void RegisterEditingHandlers(std::shared_ptr<State> const& state);
        static void RegisterDragDropHandlers(std::shared_ptr<State> const& state);
        static winrt::fire_and_forget BeginCreateAsync(std::shared_ptr<State> state, bool directory,
            Microsoft::UI::Xaml::Controls::TreeViewNode parent = nullptr);
        static void BeginRename(std::shared_ptr<State> const& state, Microsoft::UI::Xaml::Controls::TreeViewNode node);
        static void CancelEdit(std::shared_ptr<State> const& state, bool force = false);
        static Windows::Foundation::IAsyncOperation<bool> CommitEditAsync(std::shared_ptr<State> state, winrt::hstring name);
        static Windows::Foundation::IAsyncOperation<winrt::hstring> TransferAsync(std::shared_ptr<State> state,
            std::filesystem::path source, std::filesystem::path targetFolder, bool directory, bool copy);
        static void PublishAction(std::shared_ptr<State> const& state, WorkspaceFileAction const& action, std::filesystem::path const& result);
        static void RevealPendingSelection(std::shared_ptr<State> const& state);
        std::shared_ptr<State> state_;
    };
}
