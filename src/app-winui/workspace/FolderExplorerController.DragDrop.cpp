#include "pch.h"
import std;
#include <winrt/Windows.ApplicationModel.DataTransfer.DragDrop.h>
#include "workspace/FolderExplorerState.h"
#include "workspace/FolderExplorerView.h"

namespace winrt::Folia {
    namespace {
        using namespace Microsoft::UI::Xaml;
        using namespace Microsoft::UI::Xaml::Controls;
        using namespace Windows::ApplicationModel::DataTransfer;
        using explorer_detail::Entry;
        constexpr wchar_t pathFormat[] = L"Folia.Workspace.Path";
        constexpr wchar_t directoryFormat[] = L"Folia.Workspace.Directory";
        TreeViewNode TargetFolder(TreeView const& tree, DependencyObject const& element) {
            auto node = ExplorerNodeFromElement(tree, element);
            if (auto entry = Entry(node); entry && !entry->directory) node = node.Parent();
            if (!node && tree.RootNodes().Size()) node = tree.RootNodes().GetAt(0);
            return node;
        }
    }

    Windows::Foundation::IAsyncOperation<winrt::hstring> FolderExplorerController::TransferAsync(
        std::filesystem::path source, std::filesystem::path targetFolder, bool directory, bool copy) {
        return TransferAsync(state_, std::move(source), std::move(targetFolder), directory, copy);
    }
    Windows::Foundation::IAsyncOperation<winrt::hstring> FolderExplorerController::TransferAsync(std::shared_ptr<State> state,
        std::filesystem::path source, std::filesystem::path targetFolder, bool directory, bool copy) {
        if (!state->tree || state->operationBusy || state->preparingEdit || state->editingNode)
            throw winrt::hresult_illegal_method_call();
        WorkspaceFileAction action{copy ? WorkspaceFileActionKind::Copy : WorkspaceFileActionKind::Move,
            source, targetFolder / source.filename(), directory};
        auto generation = state->generation;
        state->operationBusy = true;
        state->NotifyCommands();
        struct Pending { std::shared_ptr<State> state; ~Pending() { state->operationBusy = false; state->NotifyCommands(); } } pending{state};
        auto result = co_await (state->operate ? state->operate(action) : ApplyWorkspaceFileActionAsync(action));
        if (state->tree && state->generation == generation) PublishAction(state, action, std::filesystem::path(result.c_str()));
        co_return result;
    }

    void FolderExplorerController::RegisterDragDropHandlers(std::shared_ptr<State> const& state) {
        auto weak = std::weak_ptr<State>(state);
        state->hoverTimer = state->tree.DispatcherQueue().CreateTimer();
        state->hoverTimer.IsRepeating(false);
        state->hoverTimer.Interval(std::chrono::milliseconds(650));
        state->hoverTimer.Tick([weak](auto const&, auto const&) {
            if (auto state = weak.lock(); state && state->tree && !state->operationBusy && !state->editingNode)
                if (auto node = state->hoverFolder) { node.IsExpanded(true); Populate(state, node); }
        });
        state->dragLeave = state->tree.DragLeave([weak](auto const&, auto const&) {
            if (auto state = weak.lock()) { state->hoverTimer.Stop(); state->hoverFolder = nullptr; }
        });
        state->tree.CanDragItems(true);
        state->tree.CanReorderItems(false);
        state->tree.AllowDrop(true);
        state->dragStarting = state->tree.DragItemsStarting([weak](auto const&, TreeViewDragItemsStartingEventArgs const& args) {
            auto state = weak.lock();
            if (!state || state->operationBusy || state->editingNode || args.Items().Size() != 1) { args.Cancel(true); return; }
            auto node = args.Items().GetAt(0).try_as<TreeViewNode>();
            auto entry = Entry(node);
            if (!entry || entry->edit || entry->root) { args.Cancel(true); return; }
            args.Data().SetData(pathFormat, winrt::box_value(winrt::hstring(entry->path.c_str())));
            args.Data().SetData(directoryFormat, winrt::box_value(entry->directory));
            args.Data().RequestedOperation(DataPackageOperation::Move);
        });
        state->dragOver = state->tree.DragOver([weak](auto const&, DragEventArgs const& args) {
            auto state = weak.lock();
            if (!state) return;
            auto target = TargetFolder(state->tree, args.OriginalSource().try_as<DependencyObject>());
            const bool internal = args.DataView().Contains(pathFormat);
            const bool available = !state->operationBusy && !state->editingNode && !state->preparingEdit && target && Entry(target) && !Entry(target)->edit;
            args.AcceptedOperation(available && (internal || args.DataView().Contains(StandardDataFormats::StorageItems()))
                ? (!internal || (args.Modifiers() & Windows::ApplicationModel::DataTransfer::DragDrop::DragDropModifiers::Control) != Windows::ApplicationModel::DataTransfer::DragDrop::DragDropModifiers::None
                    ? DataPackageOperation::Copy : DataPackageOperation::Move) : DataPackageOperation::None);
            if (target != state->hoverFolder) {
                state->hoverTimer.Stop(); state->hoverFolder = target;
                if (available && target && !target.IsExpanded()) state->hoverTimer.Start();
            }
            args.Handled(true);
        });
        state->drop = state->tree.Drop([weak](auto const&, DragEventArgs args) -> winrt::fire_and_forget {
            auto state = weak.lock();
            if (!state || state->operationBusy || state->editingNode || state->preparingEdit) co_return;
            auto node = TargetFolder(state->tree, args.OriginalSource().try_as<DependencyObject>());
            auto target = Entry(node);
            if (!target || target->edit) co_return;
            state->hoverTimer.Stop(); state->hoverFolder = nullptr;
            auto folder = target->path;
            auto generation = state->generation;
            const auto root = state->root;
            const bool copy = !args.DataView().Contains(pathFormat)
                || (args.Modifiers() & Windows::ApplicationModel::DataTransfer::DragDrop::DragDropModifiers::Control) != Windows::ApplicationModel::DataTransfer::DragDrop::DragDropModifiers::None;
            args.AcceptedOperation(copy ? DataPackageOperation::Copy : DataPackageOperation::Move);
            args.Handled(true);
            auto deferral = args.GetDeferral();
            std::vector<std::pair<std::filesystem::path, bool>> sources;
            winrt::hstring failure;
            try {
                if (args.DataView().Contains(pathFormat)) {
                    auto path = winrt::unbox_value<winrt::hstring>(co_await args.DataView().GetDataAsync(pathFormat));
                    auto directory = winrt::unbox_value<bool>(co_await args.DataView().GetDataAsync(directoryFormat));
                    sources.emplace_back(std::filesystem::path(path.c_str()), directory);
                } else if (args.DataView().Contains(StandardDataFormats::StorageItems())) {
                    for (auto item : co_await args.DataView().GetStorageItemsAsync())
                        sources.emplace_back(std::filesystem::path(item.Path().c_str()), item.IsOfType(Windows::Storage::StorageItemTypes::Folder));
                }
            } catch (winrt::hresult_error const& error) { failure = error.message(); }
            // Complete the drag session before copying; no input is held while
            // filesystem work runs on a worker thread.
            deferral.Complete();
            if (!failure.empty()) { if (state->tree && state->report) state->report(failure); co_return; }
            try {
                for (auto const& [source, directory] : sources) {
                    if (!state->tree || state->generation != generation || state->root != root) co_return;
                    co_await TransferAsync(state, source, folder, directory, copy);
                    if (!state->tree || state->root != root) co_return;
                    generation = state->generation;
                }
            } catch (winrt::hresult_error const& error) { if (state->tree && state->report) state->report(error.message()); }
            catch (std::exception const& error) { if (state->tree && state->report) state->report(winrt::to_hstring(error.what())); }
        });
    }
}
