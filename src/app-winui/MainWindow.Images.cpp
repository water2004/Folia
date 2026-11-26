#include "pch.h"
#include "MainWindow.xaml.h"
#include "localization/Localization.h"
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.Storage.Streams.h>

namespace winrt::Folia::implementation {
    using Microsoft::UI::Xaml::Visibility;

    void MainWindow::UpdateWorkspaceContent() {
        auto editing = HasActiveEditor();
        auto editorVisibility = editing ? Visibility::Visible : Visibility::Collapsed;
        EditorPane().Visibility(editorVisibility);
        CharacterCountText().Visibility(editorVisibility);
        SourceModeButton().Visibility(editorVisibility);
        EncodingButton().Visibility(editorVisibility);
        ImagePreviewPane().Visibility(workspaceDocuments.Active()->kind == WorkspaceTabKind::Image ? Visibility::Visible : Visibility::Collapsed);
        SetDocumentCommandsVisible(editing);
        ReloadButton().Visibility(SettingsActive() ? Visibility::Collapsed : Visibility::Visible);
        UpdateWorkspaceLayout();
    }

    void MainWindow::ClearImagePreview() {
        ++imagePreviewGeneration;
        if (imagePreviewLoad) imagePreviewLoad.Cancel();
        imagePreviewLoad = nullptr;
        ImagePreviewViewer().ChangeView(0.0, 0.0, 1.0f, true);
        WorkspaceImage().Source(nullptr);
        ImagePreviewProgress().IsActive(false);
        ImagePreviewProgress().Visibility(Visibility::Collapsed);
        ImagePreviewMessage().Text(L"");
        ImageSizeText().Visibility(Visibility::Collapsed);
    }

    void MainWindow::LoadImagePreview() {
        ClearImagePreview();
        auto document = workspaceDocuments.Active();
        using Microsoft::UI::Xaml::Automation::AutomationProperties;
        AutomationProperties::SetName(WorkspaceImage(), winrt::hstring(document->path.filename().c_str()));
        ImagePreviewProgress().Visibility(Visibility::Visible);
        ImagePreviewProgress().IsActive(true);
        ImagePreviewMessage().Text(Localize(L"ImagePreviewLoading"));
        imagePreviewLoad = DecodeImagePreview(document->path, imagePreviewGeneration);
    }

    Windows::Foundation::IAsyncAction MainWindow::DecodeImagePreview(
        std::filesystem::path path, std::uint64_t generation) {
        auto lifetime = get_strong();
        auto cancellation = co_await winrt::get_cancellation_token();
        cancellation.enable_propagation();
        auto current = [&] { return !windowClosed && imagePreviewGeneration == generation; };
        using namespace Microsoft::UI::Xaml::Media::Imaging;
        try {
            auto file = co_await Windows::Storage::StorageFile::GetFileFromPathAsync(path.native());
            auto stream = co_await file.OpenReadAsync();
            if (!current()) co_return;
            if (ClassifyWorkspaceFile(path) == WorkspaceFileKind::VectorImage) {
                SvgImageSource svg;
                WorkspaceImage().Source(svg);
                if (co_await svg.SetSourceAsync(stream) != SvgImageSourceLoadStatus::Success)
                    throw winrt::hresult_error(E_FAIL);
                if (!current()) co_return;
            } else {
                BitmapImage bitmap;
                WorkspaceImage().Source(bitmap);
                co_await bitmap.SetSourceAsync(stream);
                if (!current()) co_return;
                if (bitmap.PixelWidth() <= 0 || bitmap.PixelHeight() <= 0) throw winrt::hresult_error(E_FAIL);
                ImageSizeText().Text(LocalizeFormat(L"ImagePreviewDimensions",
                    {winrt::to_hstring(bitmap.PixelWidth()), winrt::to_hstring(bitmap.PixelHeight())}));
                ImageSizeText().Visibility(Visibility::Visible);
            }
            recentWorkspace.Remember(RecentWorkspaceKind::File, path);
            ImagePreviewMessage().Text(L"");
        } catch (winrt::hresult_canceled const&) {
            co_return;
        } catch (winrt::hresult_error const& error) {
            if (!current()) co_return;
            WorkspaceImage().Source(nullptr);
            ImagePreviewMessage().Text(WorkspaceOpenError(RecentWorkspaceKind::File, path, error, L"ImagePreviewFailed"));
        }
        ImagePreviewProgress().IsActive(false);
        ImagePreviewProgress().Visibility(Visibility::Collapsed);
        QueueOtherGroupRender();
    }

    void MainWindow::ReloadWorkspaceFile() {
        if (HasActiveEditor()) documentController().ReloadDocument();
        else if (workspaceDocuments.Active()->kind == WorkspaceTabKind::Image) LoadImagePreview();
    }
}
