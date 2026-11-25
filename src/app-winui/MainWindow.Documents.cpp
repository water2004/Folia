#include "pch.h"
import std;
#include "MainWindow.xaml.h"
#include "localization/Localization.h"

namespace winrt::Folia::implementation {
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    winrt::Folia::EditorSession& MainWindow::editorSession() noexcept {
        return workspaceDocuments.Active()->editor->session;
    }
    winrt::Folia::EditorDocumentController& MainWindow::documentController() noexcept {
        return workspaceDocuments.Active()->editor->controller;
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
                if (document->editor->session.HasFile()) document->editor->loaded = true;
                if (window->IsActiveDocument(id)) document->editor->themeRevision = window->workspaceThemeRevision;
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
            },
            [active](auto const& message) { if (auto window = active()) window->ShowError(message); }};
        auto& editor = *document->editor;
        if (editor.controllerAttached) editor.controller.RebindPresentation(std::move(presentation));
        else editor.controller.Attach(editor.session, std::move(presentation));
        editor.controller.SetSaveDialogs([weak, id]() -> Windows::Foundation::IAsyncOperation<ContentDialogResult> {
            if (auto window = weak.get()) co_return co_await window->ConfirmUnsavedChangesAsync(id);
            co_return ContentDialogResult::None;
        });
        editor.controllerAttached = true;
    }

    void MainWindow::ActivateDocument(std::uint64_t id) {
        auto document = workspaceDocuments.Find(id);
        if (!document || windowClosed) return;
        if (settingsMode) SetSettingsMode(false);
        auto previous = workspaceDocuments.Active();
        if (previous == document) {
            SelectDocumentTab();
            if (document->editor && !document->editor->loaded) LoadWorkspaceDocument(document);
            return;
        }
        auto frameUpdate = editorRenderer.UpdateWithXaml();
        // CoreText leaves the old session before the active owner changes.
        // A final IME commit can therefore still pin the old preview.
        if (auto const& editor = previous->editor) {
            if (!document->editor) HideFindBar();
            textInputController.Detach();
            inlineCompletion.reset();
            pointerController.Detach();
            latexCompletionController.Detach();
            codeLanguageController.Detach();
            keyboardController.Detach();
            sidebarController.Detach();
            scrollController.Stop();
            if (footnoteFlyout) footnoteFlyout.Hide();
            editor->viewportPosition = editorRenderer.ViewportPosition();
            editor->scrollOffset = editorRenderer.ScrollOffset();
            if (editor->viewportPosition)
                if (auto bounds = editorRenderer.CaretBounds(*editor->viewportPosition)) editor->viewportInset = bounds->top;
            editor->controller.SuspendPresentation();
            ++editor->openSerial;
            editor->opening = false;
            editor->math = editorRenderer.TakeMathCache();
        } else ClearImagePreview();
        workspaceDocuments.Activate(id);
        UpdateWorkspaceContent();
        ExternalChangeInfoBar().IsOpen(false);
        SetOperationProgress(false);
        activeSearchMatch.reset();
        SelectDocumentTab();
        UpdateDocumentInfo();
        UpdateLoadingState();
        auto const& editor = document->editor;
        if (!editor) {
            UpdateCompletionStatus(winrt::Folia::CompletionState::Idle);
            LoadImagePreview();
            return;
        }
        if (editor->themeRevision != workspaceThemeRevision) {
            editor->session.SetTheme(themeProfile);
            editor->themeRevision = workspaceThemeRevision;
        }
        if (editor->math) {
            editorRenderer.RestoreMathCache(std::move(*editor->math));
            editor->math.reset();
        }
        editorRenderer.SetScrollOffset(0);
        InitializeTextInput();
        AttachControllers();
        UpdateSourceModeUi();
        RenderEditorSurface();
        if (editor->viewportPosition) {
            editorRenderer.ScrollToPosition(*editor->viewportPosition, winrt::Folia::EditorScrollAlignment::Start);
            RenderEditorSurface();
            if (auto bounds = editorRenderer.CaretBounds(*editor->viewportPosition))
                editorRenderer.ScrollBy(bounds->top - editor->viewportInset);
        } else editorRenderer.SetScrollOffset(editor->scrollOffset);
        if (FindReplaceBar().Visibility() == Visibility::Visible) RefreshSearch(false);
        else RenderEditorSurface();
        UpdateLoadingState();
        if (!editor->loaded) LoadWorkspaceDocument(document);
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
            if (winrt::Folia::ClassifyWorkspaceFile(path) == winrt::Folia::WorkspaceFileKind::Other) {
                if (mode == winrt::Folia::WorkspaceOpenMode::Keep) {
                    auto lifetime = get_strong();
                    auto file = co_await Windows::Storage::StorageFile::GetFileFromPathAsync(path.native());
                    if (co_await Windows::System::Launcher::LaunchFileAsync(file))
                        recentWorkspace.Remember(RecentWorkspaceKind::File, path);
                }
                co_return;
            }
            textInputController.FocusLeave();
            auto previous = workspaceDocuments.Active();
            auto result = workspaceDocuments.Open(std::move(path), mode);
            ActivateDocument(result.document->id);
            RefreshDocumentTabs();
            if (result.document->editor && result.document->editor->loaded)
                recentWorkspace.Remember(RecentWorkspaceKind::File, result.document->path);
            if (result.replaced) {
                if (previous->id == *result.replaced && previous->editor) previous->editor->controller.Detach();
                // The old preview's last shared owner releases its session/cache.
            }
        } catch (winrt::hresult_error const& error) {
            recentWorkspace.ForgetMissing(RecentWorkspaceKind::File, path, error.code());
            ShowError(LocalizeFormat(L"StatusOpenFailed", {error.message()}));
        }
        catch (std::exception const& error) { ShowError(LocalizeFormat(L"StatusOpenFailed", {winrt::to_hstring(error.what())})); }
    }

    winrt::fire_and_forget MainWindow::LoadWorkspaceDocument(std::shared_ptr<winrt::Folia::WorkspaceDocument> document) {
        auto& editor = *document->editor;
        if (editor.loaded || editor.opening || !IsActiveDocument(document->id) || document->path.empty()) co_return;
        auto lifetime = get_strong();
        const auto ticket = ++editor.openSerial;
        editor.opening = true;
        DocumentLoadingText().Text(Localize(L"PreparingDocument"));
        UpdateLoadingState();
        auto current = [&] { return IsActiveDocument(document->id) && workspaceDocuments.Find(document->id) == document && ticket == editor.openSerial; };
        try {
            auto file = co_await Windows::Storage::StorageFile::GetFileFromPathAsync(document->path.wstring());
            if (!current()) co_return;
            auto loaded = co_await editor.controller.OpenFileAsync(file);
            if (current()) {
                editor.loaded = loaded;
                if (loaded) recentWorkspace.Remember(RecentWorkspaceKind::File, document->path);
            }
        } catch (winrt::hresult_error const& error) {
            if (current()) {
                recentWorkspace.ForgetMissing(RecentWorkspaceKind::File, document->path, error.code());
                ShowError(LocalizeFormat(L"StatusOpenFailed", {error.message()}));
            }
        }
        catch (std::exception const& error) { if (current()) ShowError(LocalizeFormat(L"StatusOpenFailed", {winrt::to_hstring(error.what())})); }
        if (!current()) co_return;
        editor.opening = false;
        SetOperationProgress(false);
        UpdateLoadingState();
        UpdateDocumentInfo();
        RenderEditorSurface();
    }

    void MainWindow::UpdateLoadingState() {
        auto movable = !closePending && !closingDocument && !relocatingFiles;
        DocumentTabs().CanTearOutTabs(movable);
        DocumentTabs().CanDragTabs(movable);
        auto const& editor = workspaceDocuments.Active()->editor;
        bool loading = editor && (editor->opening || editor->controller.IsLoading());
        auto showProgress = loading || operationActive;
        OperationPanel().Visibility(showProgress ? Visibility::Visible : Visibility::Collapsed);
        OperationRing().IsActive(showProgress);
        OperationPanel().VerticalAlignment(loading ? VerticalAlignment::Center : VerticalAlignment::Bottom);
        DocumentLoadingOverlay().Visibility(loading ? Visibility::Visible : Visibility::Collapsed);
        SaveButton().IsEnabled(editor && !loading);
        SaveAsButton().IsEnabled(editor && !loading);
        SourceModeButton().IsEnabled(editor && !loading);
        EncodingButton().IsEnabled(editor && !loading && editor->session.HasFile());
    }
}
