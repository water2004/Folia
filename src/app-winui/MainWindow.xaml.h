#pragma once

#include "editor/session/EditorSession.h"
#include "editor/rendering/EditorSurfaceRenderer.h"
#include "editor/interaction/EditorScrollController.h"
#include "editor/interaction/EditorAnimationController.h"
#include "editor/interaction/EditorTextInputController.h"
#include "editor/interaction/EditorPointerController.h"
#include "editor/interaction/EditorKeyboardController.h"
#include "editor/interaction/EditorLatexCompletionController.h"
#include "editor/interaction/EditorCodeLanguageController.h"
#include "editor/interaction/EditorDocumentController.h"
#include "editor/interaction/EditorSidebarController.h"
#include "settings/AppSettings.h"
#include "settings/SettingsView.h"
#include "settings/LatexCommandCatalog.h"
#include "theme/ThemeCatalog.h"
#include "theme/ThemeConfig.h"
#include "MainWindow.g.h"
#include "MainWindow.Lifecycle.h"
#include "workspace/WorkspaceDocuments.h"
#include "workspace/FolderExplorerController.h"

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

    private:
        // Child XAML controls may deliver final notifications after Closed.
        // Lock the outer window for delivery and discard notifications after it closes.
        template<class Handler> auto WindowEvent(Handler handler) {
            return [weak = get_weak(), handler = std::move(handler)](auto const& sender, auto const& args) {
                if (auto owner = weak.get(); owner && !owner->windowClosed) handler(sender, args);
            };
        }
        friend struct WorkspaceWindowTestAccess;
        friend struct App;
        struct DocumentTab {
            Windows::Foundation::IInspectable item{nullptr};
            winrt::weak_ref<MainWindow> owner;
            winrt::weak_ref<MainWindow> tearOutWindow;
        };
        void RegisterTabTransferHandlers();
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
        void EndTabDrag();
        void RevealTabWindow();
        void AttachControllers();
        void AttachDocumentController(std::shared_ptr<winrt::Folia::WorkspaceDocument> const& document);
        winrt::Folia::EditorSession& editorSession() noexcept;
        winrt::Folia::EditorDocumentController& documentController() noexcept;
        bool IsActiveDocument(std::uint64_t id) const noexcept;
        void ActivateDocument(std::uint64_t id);
        void NewDocument();
        winrt::fire_and_forget OpenWorkspaceFile(std::filesystem::path path, winrt::Folia::WorkspaceOpenMode mode);
        winrt::fire_and_forget LoadWorkspaceDocument(std::shared_ptr<winrt::Folia::WorkspaceDocument> document);
        winrt::fire_and_forget PickWorkspaceFile();
        winrt::fire_and_forget PickWorkspaceFolder();
        Windows::Foundation::IAsyncOperation<winrt::hstring> ApplyExplorerActionAsync(winrt::Folia::WorkspaceFileAction action);
        void RegisterWorkspaceHandlers();
        void RefreshDocumentTabs();
        void SelectDocumentTab();
        void UpdateDocumentTab(std::uint64_t id);
        void UpdateWorkspaceLayout();
        void UpdateChromeMetrics();
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
        folia::Theme CurrentThemeVariant();
        void ToggleSettingsMode();
        void SetSettingsMode(bool enabled);
        void SetDocumentCommandsVisible(bool visible);
        std::optional<winrt::hstring> ApplySettings(winrt::Folia::AppSettings const& settings);
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
        void ShowFootnoteFlyout(
            folia::platform::editor::EditorVisualFootnoteHit const& hit,
            winrt::Windows::Foundation::Point position);
        void NavigateToFootnote(folia::TextPosition position);
        void HandlePointerWheel(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void SetStatus(winrt::hstring const& text);
        void SetOperationProgress(
            bool active,
            std::optional<double> value = std::nullopt,
            bool cancellable = false);
        HWND WindowHandle();

        winrt::hstring lastCommand;
        winrt::Folia::WorkspaceDocuments workspaceDocuments;
        winrt::Folia::FolderExplorerController folderExplorer;
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
        std::shared_ptr<winrt::Folia::ThemeCatalog> themeCatalog = std::make_shared<winrt::Folia::ThemeCatalog>();
        std::shared_ptr<winrt::Folia::LatexCommandCatalog> latexCommandCatalog =
            std::make_shared<winrt::Folia::LatexCommandCatalog>();
        std::shared_ptr<winrt::Folia::SettingsView> settingsView;
        folia::ThemeProfile themeProfile = folia::default_theme_profile();
        bool updatingTheme = false;
        bool settingsMode = false;
        bool closePending = false;
        bool closeApproved = false;
        bool closingDocument = false;
        bool relocatingFiles = false;
        bool selectedTabVisibilityPending = false;
        bool updatingTabs = false;
        bool explorerOpen = false;
        bool outlineOpen = false;
        bool windowClosed = false;
        std::uint64_t workspaceThemeRevision = 1;
        std::unordered_map<std::uint64_t, std::shared_ptr<DocumentTab>> tabItems;
        Windows::Foundation::Collections::IObservableVector<Windows::Foundation::IInspectable> tabSource =
            winrt::single_threaded_observable_vector<Windows::Foundation::IInspectable>();
        std::shared_ptr<DocumentTab> draggingTab;
        winrt::Folia::detail::TabDragCompletionSubscription tabDragEnded;
        bool preparedTabWindow = false;
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
