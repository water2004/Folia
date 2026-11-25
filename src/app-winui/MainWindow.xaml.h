#pragma once

#include <winrt/Microsoft.UI.Composition.h>

#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/interaction/EditorScrollController.h"
#include "editor/interaction/EditorAnimationController.h"
#include "editor/interaction/EditorTextInputController.h"
#include "editor/interaction/EditorPointerController.h"
#include "editor/interaction/EditorKeyboardController.h"
#include "editor/interaction/EditorLatexCompletionController.h"
#include "editor/interaction/EditorCodeLanguageController.h"
#include "editor/document/EditorDocumentController.h"
#include "workspace/EditorSidebarController.h"
#include "settings/storage/AppSettings.h"
#include "completion/InlineCompletionController.h"
#include "settings/views/SettingsView.h"
#include "settings/latex/LatexCommandCatalog.h"
#include "theme/ThemeCatalog.h"
#include "theme/ThemeConfig.h"
#include "MainWindow.g.h"
#include "MainWindow.Lifecycle.h"
#include "workspace/WorkspaceDocuments.h"
#include "workspace/FolderExplorerController.h"
#include "workspace/RecentWorkspace.h"
#include "workspace/DocumentTools.h"
#include "workspace/TabDragPresentation.h"

namespace winrt::Folia::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
        explicit MainWindow(winrt::hstring const& initialDocumentPath);
        void DocumentTabLoaded(Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void DocumentTabPrepared(Microsoft::UI::Xaml::FrameworkElement const& sender,
            Microsoft::UI::Xaml::DataContextChangedEventArgs const& args);
        void DocumentTabDoubleTapped(Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::Input::DoubleTappedRoutedEventArgs const& args);

    private:
        // Child XAML controls may deliver final notifications after Closed.
        // Lock the outer window for delivery and discard notifications after it closes.
        template<class Handler> auto WindowEvent(Handler handler) {
            return [weak = get_weak(), handler = std::move(handler)](auto const& sender, auto const& args) {
                if (auto owner = weak.get(); owner && !owner->windowClosed) handler(sender, args);
            };
        }
        friend struct WorkspaceWindowTestAccess;
        friend struct CompletionWindowTestAccess;
        friend struct TextInputWindowTestAccess;
        friend struct App;
        struct DocumentTab {
            Windows::Foundation::IInspectable item{nullptr};
            winrt::weak_ref<MainWindow> owner;
            winrt::weak_ref<MainWindow> tearOutWindow;
        };
        void RegisterTabTransferHandlers();
        void UpdateTabTransferAvailability();
        void RefreshTabCaptionRegion();
        MainWindow* CreateTabWindow();
        void SynchronizeTabOrder();
        void QueueTabOrderSync();
        bool CanAcceptTab(std::shared_ptr<DocumentTab> const& tab) const;
        bool TransferTab(std::shared_ptr<DocumentTab> tab, std::size_t index);
        std::shared_ptr<DocumentTab> FindTransferTab(Windows::Foundation::IInspectable const& item);
        MainWindow* BeginTabTearOut(std::shared_ptr<DocumentTab> const& tab);
        bool CompleteTabTearOut(std::shared_ptr<DocumentTab> const& tab);
        bool PrepareExternalTab(Windows::Foundation::IInspectable const& item);
        bool ContainsTabDragPoint(Windows::Graphics::PointInt32 point);
        void BeginTabPointerGesture(Windows::Graphics::PointInt32 point);
        void EndTabPointerGesture();
        void EnterTabMove(Microsoft::UI::Input::InputNonClientPointerSource const& pointer);
        void ExitTabMove();
        void RestoreIdleTabTransfer();
        void EndTabDrag();
        void RevealTabWindow();
        void AttachControllers();
        void AttachDocumentController(std::shared_ptr<winrt::Folia::WorkspaceDocument> const& document);
        winrt::Folia::EditorSession& editorSession() noexcept;
        winrt::Folia::EditorDocumentController& documentController() noexcept;
        bool HasActiveEditor() const noexcept { return bool(workspaceDocuments.Active()->editor); }
        bool IsActiveDocument(std::uint64_t id) const noexcept;
        void ActivateDocument(std::uint64_t id);
        void NewDocument();
        winrt::fire_and_forget OpenWorkspaceFile(std::filesystem::path path, winrt::Folia::WorkspaceOpenMode mode);
        winrt::fire_and_forget LoadWorkspaceDocument(std::shared_ptr<winrt::Folia::WorkspaceDocument> document);
        void UpdateWorkspaceContent();
        void ClearImagePreview();
        void LoadImagePreview();
        Windows::Foundation::IAsyncAction DecodeImagePreview(std::filesystem::path path, std::uint64_t generation);
        void ReloadWorkspaceFile();
        winrt::fire_and_forget PickWorkspaceFile();
        winrt::fire_and_forget PickWorkspaceFolder();
        winrt::fire_and_forget OpenWorkspaceFolder(std::filesystem::path path);
        void RefreshRecentMenu();
        Windows::Foundation::IAsyncOperation<winrt::hstring> ApplyExplorerActionAsync(folia::platform::WorkspaceFileAction action);
        void RegisterWorkspaceHandlers();
        Microsoft::UI::Xaml::Controls::ScrollViewer TabStripScroller();
        void RefreshDocumentTabs();
        void SelectDocumentTab();
        void UpdateDocumentTab(std::uint64_t id);
        void UpdateWorkspaceLayout();
        void UpdateChromeMetrics();
        void ApplyBarLayout();
        void RegisterWorkspaceLayout();
        void SelectWorkspacePane(bool outline);
        void SetSidebarOpen(bool open);
        void ReserveSidebarSpace();
        void AnimateSidebarOverlay();
        void FinishSidebarMotion();
        void RevokeSidebarMotion();
        void LayoutSidebar();
        void ApplySidebarSurface();
        void UpdateWorkspaceAppearance(WorkspaceAppearance const& settings);
        void EnsureSelectedTabVisible();
        void UpdateLoadingState();
        bool DispatchApplicationAction(std::string_view action);
        void RegisterApplicationShortcuts();
        winrt::fire_and_forget CloseDocumentAsync(std::uint64_t id);
        void RegisterWindowHandlers();
        winrt::fire_and_forget ConfirmWindowCloseAsync();
        winrt::Windows::Foundation::IAsyncOperation<winrt::Microsoft::UI::Xaml::Controls::ContentDialogResult> ConfirmUnsavedChangesAsync(std::uint64_t id);
        void InitializeEditorSurface();
        void InitializeTextInput();
        void LocalizeShell();
        winrt::hstring LocalizedDocumentName();
        void ResizeEditorSurface(double width, double height);
        void RenderEditorSurface();
        void UpdateTheme();
        void ApplyShellTheme();
        void ApplyWorkspaceMaterial();
        void UpdateSelectedTabMaterial();
        folia::Theme CurrentThemeVariant();
        bool SettingsActive() const noexcept { return workspaceDocuments.Active()->kind == WorkspaceTabKind::Settings; }
        void OpenSettingsTab();
        void ReturnToDocumentTab();
        void ShowSettingsPage(bool enabled);
        void SetDocumentCommandsVisible(bool visible);
        std::optional<winrt::hstring> ApplySettings(winrt::Folia::AppSettings const& settings);
        void UpdateMediaSettings(winrt::Folia::MediaDisplaySettings const& settings);
        void RegisterCommandHandlers();
        void ToggleSourceMode();
        void UpdateSourceModeUi();
        void UpdateDocumentInfo();
        winrt::fire_and_forget ShowEncodingPicker(bool saveWithEncoding);
        void ShowFindBar(bool replace);
        void HideFindBar();
        void RefreshSearch(bool activateMatch = false);
        void NavigateSearch(int direction);
        void ReplaceCurrentSearchMatch();
        void ReplaceAllSearchMatches();
        folia::SearchOptions CurrentSearchOptions();
        bool ExecuteEditorCommand(folia::Command const& command);
        void ConfigureInlineCompletion(folia::platform::completion::CompletionService::Transport transport = {});
        void UpdateCompletionStatus(winrt::Folia::CompletionState, std::string const& error = {});
        bool HandleCompletionKey(folia::platform::editor::EditorKeyGesture);
        void ShowFootnoteFlyout(
            folia::platform::editor::EditorVisualFootnoteHit const& hit,
            winrt::Windows::Foundation::Point position);
        void NavigateToFootnote(folia::TextPosition position);
        void HandlePointerWheel(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void ShowError(winrt::hstring const& text);
        winrt::hstring WorkspaceOpenError(RecentWorkspaceKind kind, std::filesystem::path const& path,
            winrt::hresult_error const& error, wchar_t const* fallback = nullptr);
        void SetStatus(winrt::hstring const& text);
        void SetOperationProgress(
            bool active,
            std::optional<double> value = std::nullopt,
            bool cancellable = false);
        HWND WindowHandle();

        winrt::Folia::WorkspaceDocuments workspaceDocuments;
        winrt::Folia::FolderExplorerController folderExplorer;
        winrt::Folia::RecentWorkspace recentWorkspace;
        winrt::Folia::EditorSurfaceRenderer editorRenderer;
        winrt::Folia::EditorScrollController scrollController;
        winrt::Folia::EditorAnimationController animationController;
        winrt::Folia::EditorTextInputController textInputController;
        winrt::Folia::EditorPointerController pointerController;
        winrt::Folia::EditorKeyboardController keyboardController;
        winrt::Folia::EditorLatexCompletionController latexCompletionController;
        winrt::Folia::EditorCodeLanguageController codeLanguageController;
        winrt::Folia::EditorSidebarController sidebarController;
        winrt::Microsoft::UI::Xaml::Controls::Flyout footnoteFlyout{ nullptr };
        winrt::Folia::AppSettings appSettings;
        std::shared_ptr<winrt::Folia::InlineCompletionController> inlineCompletion;
        std::shared_ptr<winrt::Folia::ThemeCatalog> themeCatalog = std::make_shared<winrt::Folia::ThemeCatalog>();
        std::shared_ptr<winrt::Folia::LatexCommandCatalog> latexCommandCatalog =
            std::make_shared<winrt::Folia::LatexCommandCatalog>();
        std::shared_ptr<winrt::Folia::SettingsView> settingsView;
        folia::ThemeProfile themeProfile = folia::default_theme_profile();
        bool updatingTheme = false;
        std::optional<std::uint64_t> previousDocumentTab;
        bool closePending = false;
        bool closeApproved = false;
        bool closingDocument = false;
        bool relocatingFiles = false;
        bool selectedTabVisibilityPending = false;
        bool tabPointerDown = false, tabMoveInProgress = false;
        bool tabRegionsPending = false;
        bool updatingTabs = false;
        DocumentTools documentTools;
        std::optional<WindowMaterial> appliedMaterial;
        std::optional<WindowMaterial> appliedReadingMaterial;
        Microsoft::UI::Xaml::Controls::SystemBackdropElement selectedTabBackdrop{nullptr};
        bool sidebarOpen = false, sidebarOverlay = false, outlineSelected = false;
        bool updatingRailSelection = false;
        bool sidebarPresented = false;
        int sidebarWindowExtension = 0;
        Microsoft::UI::Composition::InsetClip sidebarClip{nullptr};
        Microsoft::UI::Composition::CompositionScopedBatch sidebarBatch{nullptr};
        winrt::event_token sidebarMotionCompleted{};
        TabDragPresentation dragPresentation;
        std::optional<float> tabGrabOffset;
        bool operationActive = false;
        bool windowClosed = false;
        Windows::Foundation::IAsyncAction imagePreviewLoad{nullptr};
        std::uint64_t imagePreviewGeneration = 0;
        std::uint64_t folderOpenGeneration = 0;
        std::uint64_t workspaceThemeRevision = 1;
        std::unordered_map<std::uint64_t, std::shared_ptr<DocumentTab>> tabItems;
        Windows::Foundation::Collections::IObservableVector<Windows::Foundation::IInspectable> tabSource =
            winrt::single_threaded_observable_vector<Windows::Foundation::IInspectable>();
        std::shared_ptr<DocumentTab> draggingTab;
        winrt::Folia::detail::TabDragCompletionSubscription tabDragEnded;
        bool preparedTabWindow = false;
        winrt::weak_ref<MainWindow> preparedTabOwner;
        winrt::Folia::detail::PreparedTabActivation preparedTabActivation;
        LONG_PTR tabWindowStyle = 0;
        std::optional<std::uint64_t> transferPlaceholder;
        bool tabOrderSyncPending = false;
        std::vector<winrt::Microsoft::UI::Xaml::Input::KeyboardAccelerator> applicationShortcuts;
        winrt::Folia::detail::WindowClosingSubscription closingHandler;
        std::optional<std::size_t> activeSearchMatch;
        std::size_t searchMatchCount = 0;
        bool updatingSearch = false;
    };
}

namespace winrt::Folia::factory_implementation
{
    struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
    {
    };
}
