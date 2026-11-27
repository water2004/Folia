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
        if (!workspaceDocuments.Find(id) || windowClosed) return;
        auto view = FindDocumentView(id);
        if (!view) view = editorGroups.Add(editorGroups.ActiveGroup(), id);
        ActivateView(view);
    }

    void MainWindow::ActivateView(std::uint64_t view) {
        auto entry = editorGroups.Find(view);
        auto document = entry ? workspaceDocuments.Find(entry->document) : nullptr;
        if (!document || windowClosed) return;
        auto previous = workspaceDocuments.Active();
        if (presentedEditorView == view) {
            editorGroups.Select(view);
            UpdateEditorGroupLayout();
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
            SaveEditorView(presentedEditorView);
            editor->controller.SuspendPresentation();
            ++editor->openSerial;
            editor->opening = false;
            editor->math = editorRenderer.TakeMathCache();
        } else if (previous->kind == WorkspaceTabKind::Image) {
            if (editorGroups.IsSplit() && WorkspaceImage().Source()) {
                otherImageDocument = previous->id;
                OtherWorkspaceImage().Source(WorkspaceImage().Source());
            }
            ClearImagePreview();
        }
        if (previous->editor) lastEditorDocument = previous->id;
        const bool enteringSettings = document->kind == WorkspaceTabKind::Settings;
        const bool leavingSettings = previous->kind == WorkspaceTabKind::Settings;
        if (enteringSettings) previousDocumentTab = previous->id;
        workspaceDocuments.Activate(document->id);
        editorGroups.Select(view);
        presentedEditorView = view;
        UpdateEditorGroupLayout();
        if (enteringSettings || leavingSettings) ShowSettingsPage(enteringSettings);
        UpdateAgentChatPages();
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
            if (document->kind == WorkspaceTabKind::Image) LoadImagePreview();
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
        if (entry->selection) editor->session.SetSelection(*entry->selection);
        editorRenderer.SetScrollOffset(0);
        InitializeTextInput();
        AttachControllers();
        UpdateSourceModeUi();
        if (editorGroups.IsSplit()) ReadingMotionFrame().UpdateLayout();
        RenderEditorSurface();
        RestoreEditorView(*entry);
        if (FindReplaceBar().Visibility() == Visibility::Visible) RefreshSearch(false);
        UpdateLoadingState();
        if (!editor->loaded) LoadWorkspaceDocument(document);
        if (leavingSettings) EditorSurface().Focus(FocusState::Programmatic);
    }

    void MainWindow::NewDocument() {
        NewDocumentInGroup(editorGroups.ActiveGroup());
    }

    void MainWindow::NewDocumentInGroup(EditorGroup group) {
        if (closePending || closingDocument) return;
        textInputController.FocusLeave();
        auto document = workspaceDocuments.New();
        ActivateView(editorGroups.Add(group, document->id));
        RefreshDocumentTabs();
        EditorSurface().Focus(FocusState::Programmatic);
    }

    winrt::hstring MainWindow::WorkspaceOpenError(RecentWorkspaceKind kind, std::filesystem::path const& path,
            winrt::hresult_error const& error, wchar_t const* fallback) {
        if (recentWorkspace.ForgetMissing(kind, path, error.code())) {
            auto key = kind == RecentWorkspaceKind::Folder ? L"WorkspaceFolderNotFound" : L"WorkspaceFileNotFound";
            return LocalizeFormat(key, {winrt::hstring(path.native())});
        }
        return fallback ? Localize(fallback) :
            LocalizeFormat(kind == RecentWorkspaceKind::Folder ? L"StatusFolderFailed" : L"StatusOpenFailed", {error.message()});
    }

    winrt::fire_and_forget MainWindow::OpenWorkspaceFile(std::filesystem::path path, winrt::Folia::WorkspaceOpenMode mode,
        std::optional<winrt::Folia::EditorSourceLocation> location) {
        try {
            if (closePending || closingDocument || windowClosed || relocatingFiles) co_return;
            if (!location && winrt::Folia::ClassifyWorkspaceFile(path) == winrt::Folia::WorkspaceFileKind::Other) {
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
            auto group = editorGroups.ActiveGroup();
            auto result = workspaceDocuments.Open(path, mode, [this, group](auto candidate) {
                return editorGroups.ViewFor(group, candidate) && editorGroups.ViewCount(candidate) == 1;
            });
            if (location && result.document->editor) result.document->editor->pendingLocation = location;
            auto id = result.document->id;
            auto view = editorGroups.Add(group, id);
            if (editorGroups.ViewCount(id) > 1) workspaceDocuments.Keep(id);
            ActivateView(view);
            RefreshDocumentTabs();
            if (location) ApplyDocumentLocation();
            if (result.document->editor && result.document->editor->loaded)
                recentWorkspace.Remember(RecentWorkspaceKind::File, result.document->path);
            if (result.replaced) {
                if (previous->id == *result.replaced && previous->editor) previous->editor->controller.Detach();
                // The old preview's last shared owner releases its session/cache.
            }
        } catch (winrt::hresult_error const& error) {
            ShowError(WorkspaceOpenError(RecentWorkspaceKind::File, path, error));
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
                ShowError(WorkspaceOpenError(RecentWorkspaceKind::File, document->path, error));
            }
        }
        catch (std::exception const& error) { if (current()) ShowError(LocalizeFormat(L"StatusOpenFailed", {winrt::to_hstring(error.what())})); }
        if (!current()) co_return;
        editor.opening = false;
        SetOperationProgress(false);
        UpdateLoadingState();
        UpdateDocumentInfo();
        RenderEditorSurface();
        if (editor.loaded) ApplyDocumentLocation();
    }

    void MainWindow::ApplyDocumentLocation() {
        auto doc = workspaceDocuments.Active();
        if (!doc->editor || !doc->editor->loaded || !doc->editor->pendingLocation) return;
        auto location = *std::exchange(doc->editor->pendingLocation, std::nullopt);
        if (location.kind == EditorSourceLocation::Kind::Document) return;
        doc->editor->session.JumpToSourceLocation(location);
        RenderEditorSurface();
        if (editorRenderer.ScrollToPosition(doc->editor->session.Selection().active, EditorScrollAlignment::Start)) RenderEditorSurface();
        EditorSurface().Focus(Microsoft::UI::Xaml::FocusState::Programmatic);
    }

    void MainWindow::UpdateLoadingState() {
        auto movable = !closePending && !closingDocument && !relocatingFiles;
        UpdateTabTransferAvailability();
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
        SplitEditorButton().IsEnabled(movable && !editorGroups.IsSplit() && !loading && !SettingsActive());
        QueueOtherGroupRender();
    }
}
