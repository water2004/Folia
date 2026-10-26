#include "pch.h"
import std;
#include "MainWindow.xaml.h"
#include "localization/Localization.h"

namespace winrt::Folia::implementation {
    winrt::fire_and_forget MainWindow::PickWorkspaceFile() {
        if (closePending || closingDocument || windowClosed) co_return;
        auto lifetime = get_strong();
        try {
            Windows::Storage::Pickers::FileOpenPicker picker;
            for (auto extension : {L".md", L".markdown", L".txt"}) picker.FileTypeFilter().Append(extension);
            winrt::check_hresult(picker.as<IInitializeWithWindow>()->Initialize(WindowHandle()));
            if (auto file = co_await picker.PickSingleFileAsync())
                OpenWorkspaceFile(std::filesystem::path(file.Path().c_str()), winrt::Folia::WorkspaceOpenMode::Keep);
        } catch (winrt::hresult_error const& error) { SetStatus(LocalizeFormat(L"StatusOpenFailed", {error.message()})); }
    }

    winrt::fire_and_forget MainWindow::PickWorkspaceFolder() {
        if (closePending || closingDocument || windowClosed) co_return;
        auto lifetime = get_strong();
        try {
            Windows::Storage::Pickers::FolderPicker picker;
            picker.FileTypeFilter().Append(L"*");
            winrt::check_hresult(picker.as<IInitializeWithWindow>()->Initialize(WindowHandle()));
            if (auto folder = co_await picker.PickSingleFolderAsync()) {
                if (windowClosed) co_return;
                auto path = std::filesystem::path(folder.Path().c_str());
                folderExplorer.Open(path);
                Microsoft::UI::Xaml::Controls::ToolTipService::SetToolTip(ExplorerHeadingText(), winrt::box_value(folder.Path()));
                FileTree().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
                ExplorerEmptyState().Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
                FolderActionsButton().Visibility(Microsoft::UI::Xaml::Visibility::Visible);
                explorerOpen = true;
                ExplorerToggleButton().IsChecked(true);
                UpdateWorkspaceLayout();
            }
        } catch (winrt::hresult_error const& error) { SetStatus(LocalizeFormat(L"StatusFolderFailed", {error.message()})); }
        catch (std::exception const& error) { SetStatus(LocalizeFormat(L"StatusFolderFailed", {winrt::to_hstring(error.what())})); }
    }
}
