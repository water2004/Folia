#include "pch.h"
import std;
#include "MainWindow.xaml.h"
#include "localization/Localization.h"

namespace winrt::Folia::implementation {
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    winrt::Folia::EditorSession& MainWindow::editorSession() noexcept {
        return workspaceDocuments.Active()->session;
    }
    winrt::Folia::EditorDocumentController& MainWindow::documentController() noexcept {
        return workspaceDocuments.Active()->controller;
    }
    bool MainWindow::IsActiveDocument(std::uint64_t id) const noexcept {
        return !windowClosed && workspaceDocuments.Active()->id == id;
    }

    void MainWindow::AttachDocumentController(std::shared_ptr<winrt::Folia::WorkspaceDocument> const& document) {
        auto weak = get_weak();
        auto id = document->id;
        auto active = [weak, id]() -> winrt::com_ptr<MainWindow> {
            auto window = weak.get();
            return window && window->IsActiveDocument(id) ? window : nullptr;
        };
        winrt::Folia::EditorDocumentController::Presentation presentation{editorRenderer, textInputController,
            [active](auto const& command) { if (auto window = active()) return window->ExecuteEditorCommand(command); return false; },
            [active](auto const& status) { if (auto window = active()) window->SetStatus(status); },
            [active](bool busy, std::optional<double> value, bool cancellable) {
                if (auto window = active()) { window->SetOperationProgress(busy, value, cancellable); window->UpdateLoadingState(); }
            },
            [weak, id] {
                auto window = weak.get();
                if (!window || window->windowClosed) return;
                auto document = window->workspaceDocuments.Find(id);
                if (!document) return;
                auto previousPath = document->path;
                window->workspaceDocuments.RefreshPath(id);
                if (document->session.HasFile()) document->loaded = true;
                if (window->IsActiveDocument(id)) document->themeRevision = window->workspaceThemeRevision;
                window->workspaceDocuments.ObserveEdit(id);
                if (previousPath != document->path) window->RefreshDocumentTabs();
                else window->UpdateDocumentTab(id);
                if (!window->IsActiveDocument(id)) return;
                window->latexCompletionController.Cancel();
                window->UpdateSourceModeUi();
                window->UpdateDocumentInfo();
                window->sidebarController.Refresh();
                if (window->FindReplaceBar().Visibility() == Visibility::Visible) window->RefreshSearch(false);
                else window->RenderEditorSurface();
            },
            [active] { if (auto window = active()) window->RenderEditorSurface(); },
            [weak] { if (auto window = weak.get()) return window->WindowHandle(); return HWND{}; },
            [active](bool visible, bool deleted) {
                if (auto window = active()) {
                    window->ExternalChangeInfoBar().Title(Localize(deleted ? L"ExternalFileDeletedTitle" : L"ExternalFileChangedTitle"));
                    window->ExternalChangeInfoBar().Message(Localize(deleted ? L"ExternalFileDeletedMessage" : L"ExternalFileChangedMessage"));
                    window->ReloadExternalButton().Visibility(deleted ? Visibility::Collapsed : Visibility::Visible);
                    window->ExternalChangeInfoBar().IsOpen(visible);
                }
            }};
        if (document->controllerAttached) document->controller.RebindPresentation(std::move(presentation));
        else document->controller.Attach(document->session, std::move(presentation));
        document->controller.SetSaveDialogs([weak, id]() -> Windows::Foundation::IAsyncOperation<ContentDialogResult> {
            if (auto window = weak.get()) co_return co_await window->ConfirmUnsavedChangesAsync(id);
            co_return ContentDialogResult::None;
        });
        document->controllerAttached = true;
    }

    void MainWindow::ActivateDocument(std::uint64_t id) {
        auto document = workspaceDocuments.Find(id);
        if (!document || windowClosed) return;
        if (settingsMode) SetSettingsMode(false);
        auto previous = workspaceDocuments.Active();
        if (previous == document) {
            SelectDocumentTab();
            if (!document->loaded) LoadWorkspaceDocument(document);
            return;
        }
        // CoreText leaves the old session before the active owner changes.
        // A final IME commit can therefore still pin the old preview.
        textInputController.Detach();
        pointerController.Detach();
        latexCompletionController.Detach();
        codeLanguageController.Detach();
        keyboardController.Detach();
        sidebarController.Detach();
        scrollController.Stop();
        if (footnoteFlyout) footnoteFlyout.Hide();
        previous->viewportPosition = editorRenderer.ViewportPosition();
        previous->scrollOffset = editorRenderer.ScrollOffset();
        if (previous->viewportPosition)
            if (auto bounds = editorRenderer.CaretBounds(*previous->viewportPosition)) previous->viewportInset = bounds->top;
        previous->controller.SuspendPresentation();
        ++previous->openSerial;
        previous->opening = false;
        previous->math = editorRenderer.TakeMathCache();
        workspaceDocuments.Activate(id);
        if (document->themeRevision != workspaceThemeRevision) {
            document->session.SetTheme(themeProfile);
            document->themeRevision = workspaceThemeRevision;
        }
        if (document->math) {
            editorRenderer.RestoreMathCache(std::move(*document->math));
            document->math.reset();
        }
        editorRenderer.SetScrollOffset(0);
        ExternalChangeInfoBar().IsOpen(false);
        InitializeTextInput();
        AttachControllers();
        SetOperationProgress(false);
        activeSearchMatch.reset();
        UpdateSourceModeUi();
        UpdateDocumentInfo();
        SelectDocumentTab();
        RenderEditorSurface();
        if (document->viewportPosition) {
            editorRenderer.ScrollToPosition(*document->viewportPosition, winrt::Folia::EditorScrollAlignment::Start);
            RenderEditorSurface();
            if (auto bounds = editorRenderer.CaretBounds(*document->viewportPosition))
                editorRenderer.ScrollBy(bounds->top - document->viewportInset);
        } else editorRenderer.SetScrollOffset(document->scrollOffset);
        if (FindReplaceBar().Visibility() == Visibility::Visible) RefreshSearch(false);
        else RenderEditorSurface();
        UpdateLoadingState();
        if (!document->loaded) LoadWorkspaceDocument(document);
    }

    void MainWindow::NewDocument() {
        if (closePending || closingDocument) return;
        textInputController.FocusLeave();
        auto document = workspaceDocuments.New();
        ActivateDocument(document->id);
        RefreshDocumentTabs();
        EditorSurface().Focus(FocusState::Programmatic);
    }

    winrt::fire_and_forget MainWindow::OpenWorkspaceFile(std::filesystem::path path, winrt::Folia::WorkspaceOpenMode mode) {
        try {
            if (closePending || closingDocument || windowClosed || relocatingFiles) co_return;
            auto extension = path.extension().native();
            const bool editable = _wcsicmp(extension.c_str(), L".md") == 0 || _wcsicmp(extension.c_str(), L".markdown") == 0 || _wcsicmp(extension.c_str(), L".txt") == 0;
            if (!editable) {
                if (mode == winrt::Folia::WorkspaceOpenMode::Keep) {
                    auto lifetime = get_strong();
                    auto file = co_await Windows::Storage::StorageFile::GetFileFromPathAsync(path.native());
                    co_await Windows::System::Launcher::LaunchFileAsync(file);
                }
                co_return;
            }
            textInputController.FocusLeave();
            auto previous = workspaceDocuments.Active();
            auto result = workspaceDocuments.Open(std::move(path), mode);
            ActivateDocument(result.document->id);
            RefreshDocumentTabs();
            if (result.replaced) {
                if (previous->id == *result.replaced) previous->controller.Detach();
                // The old preview's last shared owner releases its session/cache.
            }
            if (!result.document->loaded) LoadWorkspaceDocument(result.document);
        } catch (winrt::hresult_error const& error) { SetStatus(LocalizeFormat(L"StatusOpenFailed", {error.message()})); }
        catch (std::exception const& error) { SetStatus(LocalizeFormat(L"StatusOpenFailed", {winrt::to_hstring(error.what())})); }
    }

    winrt::fire_and_forget MainWindow::LoadWorkspaceDocument(std::shared_ptr<winrt::Folia::WorkspaceDocument> document) {
        if (document->loaded || document->opening || !IsActiveDocument(document->id) || document->path.empty()) co_return;
        auto lifetime = get_strong();
        const auto ticket = ++document->openSerial;
        document->opening = true;
        UpdateLoadingState();
        auto current = [&] { return IsActiveDocument(document->id) && workspaceDocuments.Find(document->id) == document && ticket == document->openSerial; };
        try {
            auto file = co_await Windows::Storage::StorageFile::GetFileFromPathAsync(document->path.wstring());
            if (!current()) co_return;
            auto loaded = co_await document->controller.OpenFileAsync(file);
            if (current()) document->loaded = loaded;
        } catch (winrt::hresult_error const& error) { if (current()) SetStatus(LocalizeFormat(L"StatusOpenFailed", {error.message()})); }
        catch (std::exception const& error) { if (current()) SetStatus(LocalizeFormat(L"StatusOpenFailed", {winrt::to_hstring(error.what())})); }
        if (!current()) co_return;
        document->opening = false;
        SetOperationProgress(false);
        UpdateLoadingState();
        UpdateDocumentInfo();
        RenderEditorSurface();
    }

    void MainWindow::UpdateLoadingState() {
        auto movable = !closePending && !closingDocument && !relocatingFiles;
        DocumentTabs().CanTearOutTabs(movable);
        DocumentTabs().CanDragTabs(movable);
        bool loading = workspaceDocuments.Active()->opening || documentController().IsLoading();
        DocumentLoadingOverlay().Visibility(loading ? Visibility::Visible : Visibility::Collapsed);
        SaveButton().IsEnabled(!loading);
        SaveAsButton().IsEnabled(!loading);
        SourceModeButton().IsEnabled(!loading);
        EncodingButton().IsEnabled(!loading && editorSession().HasFile());
    }
}
