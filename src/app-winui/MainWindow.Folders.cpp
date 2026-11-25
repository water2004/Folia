#include "pch.h"
import std;
#include "MainWindow.xaml.h"
#include "localization/Localization.h"
#include "workspace/WorkspaceFileCoordinator.h"
#include <winrt/Microsoft.Windows.Storage.Pickers.h>

namespace winrt::Folia::implementation {
    winrt::fire_and_forget MainWindow::PickWorkspaceFile() {
        if (closePending || closingDocument || windowClosed || relocatingFiles || folderExplorer.IsBusy()) co_return;
        auto lifetime = get_strong();
        try {
            Windows::Storage::Pickers::FileOpenPicker picker;
            for (auto extension : WorkspaceTextExtensions) picker.FileTypeFilter().Append(extension);
            for (auto extension : WorkspaceRasterExtensions) picker.FileTypeFilter().Append(extension);
            picker.FileTypeFilter().Append(WorkspaceVectorExtension);
            winrt::check_hresult(picker.as<IInitializeWithWindow>()->Initialize(WindowHandle()));
            if (auto file = co_await picker.PickSingleFileAsync())
                OpenWorkspaceFile(std::filesystem::path(file.Path().c_str()), winrt::Folia::WorkspaceOpenMode::Keep);
        } catch (winrt::hresult_error const& error) { SetStatus(LocalizeFormat(L"StatusOpenFailed", {error.message()})); }
    }

    winrt::fire_and_forget MainWindow::PickWorkspaceFolder() {
        if (closePending || closingDocument || windowClosed || relocatingFiles || folderExplorer.IsBusy()) co_return;
        auto lifetime = get_strong();
        try {
            Microsoft::Windows::Storage::Pickers::FolderPicker picker{AppWindow().Id()};
            if (auto folder = folderExplorer.RootPath(); !folder.empty()) picker.SuggestedFolder(folder.native());
            if (auto folder = co_await picker.PickSingleFolderAsync()) {
                if (windowClosed) co_return;
                auto path = std::filesystem::path(folder.Path().c_str());
                folderExplorer.Open(path);

                FileTree().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
                ExplorerEmptyState().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
                explorerOpen = true;
                UpdateWorkspaceLayout();
            }
        } catch (winrt::hresult_error const& error) { SetStatus(LocalizeFormat(L"StatusFolderFailed", {error.message()})); }
        catch (std::exception const& error) { SetStatus(LocalizeFormat(L"StatusFolderFailed", {winrt::to_hstring(error.what())})); }
    }
    Windows::Foundation::IAsyncOperation<winrt::hstring> MainWindow::ApplyExplorerActionAsync(folia::platform::WorkspaceFileAction action) {
        if (windowClosed || closePending || closingDocument || relocatingFiles) throw winrt::hresult_illegal_method_call();
        auto lifetime = get_strong();
        const bool relocate = action.kind == folia::platform::WorkspaceFileActionKind::Move || action.kind == folia::platform::WorkspaceFileActionKind::Rename;
        relocatingFiles = relocate;
        UpdateLoadingState();
        struct ProgressState {
            std::mutex mutex;
            folia::platform::WorkspaceFileProgress latest;
            bool queued = false, finished = false;
        };
        auto progress = std::make_shared<ProgressState>();
        auto dispatcher = DispatcherQueue();
        const auto label = std::wstring(action.kind == folia::platform::WorkspaceFileActionKind::Copy ? L"CopyingFile"
            : relocate ? L"MovingFile" : L"CreatingFile");
        FileOperationText().Text(LocalizeFormat(label.c_str(), {winrt::hstring(action.target.filename().c_str())}));
        FileOperationProgress().IsIndeterminate(true);
        auto timer = dispatcher.CreateTimer();
        timer.IsRepeating(false); timer.Interval(std::chrono::milliseconds(200));
        timer.Tick([weak = get_weak()](auto const&, auto const&) {
            if (auto window = weak.get(); window && !window->windowClosed)
                window->FileOperationPanel().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
        });
        timer.Start();
        struct Pending {
            MainWindow& window;
            std::shared_ptr<ProgressState> progress;
            Microsoft::UI::Dispatching::DispatcherQueueTimer timer;
            ~Pending() {
                { std::scoped_lock lock(progress->mutex); progress->finished = true; }
                timer.Stop(); window.relocatingFiles = false;
                if (!window.windowClosed) window.UpdateLoadingState();
                if (!window.windowClosed) window.FileOperationPanel().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
            }
        } pending{*this, progress, timer};
        auto report = [weak = get_weak(), dispatcher, progress, label](folia::platform::WorkspaceFileProgress const& value) {
            {
                std::scoped_lock lock(progress->mutex);
                if (progress->finished) return;
                progress->latest = value;
                if (progress->queued) return;
                progress->queued = true;
            }
            dispatcher.TryEnqueue(Microsoft::UI::Dispatching::DispatcherQueuePriority::Low, [weak, progress, label] {
                folia::platform::WorkspaceFileProgress value;
                {
                    std::scoped_lock lock(progress->mutex);
                    progress->queued = false;
                    if (progress->finished) return;
                    value = progress->latest;
                }
                if (auto window = weak.get(); window && !window->windowClosed) {
                    window->FileOperationText().Text(LocalizeFormat(label.c_str(), {winrt::hstring(value.path.filename().c_str())}));
                    window->FileOperationProgress().IsIndeterminate(value.total == 0);
                    if (value.total) window->FileOperationProgress().Value(100. * value.completed / value.total);
                }
            });
        };
        auto active = workspaceDocuments.Active();
        auto reloadImage = !active->editor && RelocatedWorkspacePath(action, active->path).has_value();
        if (reloadImage) ClearImagePreview();
        auto restoreImage = [&] {
            if (reloadImage && !windowClosed && active == workspaceDocuments.Active()) {
                UpdateDocumentInfo();
                LoadImagePreview();
            }
        };
        winrt::hstring result;
        try {
            result = co_await winrt::Folia::ApplyWorkspaceDocumentFileActionAsync(workspaceDocuments, action, report);
        } catch (...) {
            restoreImage();
            throw;
        }
        if (!windowClosed) {
            RefreshDocumentTabs();
            restoreImage();
        }
        co_return result;
    }

}
