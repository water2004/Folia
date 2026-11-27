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
#include "workspace/WorkspaceEditorGroups.h"
#include "workspace/FolderExplorerController.h"
#include "workspace/RecentWorkspace.h"
#include "workspace/DocumentTools.h"
#include "workspace/TabDragPresentation.h"
#include "workspace/WorkspaceSidebarMotion.h"

namespace winrt::Folia::chat { class ChatView; struct ViewHost; }

namespace winrt::Folia::implementation
{
    struct MainWindow : MainWindowT<MainWindow>
    {
        MainWindow();
        explicit MainWindow(winrt::hstring const& initialDocumentPath);
        void DocumentTabLoaded(Windows::Foundation::IInspectable const& sender,
            Microsoft::UI::Xaml::RoutedEventArgs const& args);
        void DocumentTabSizeChanged(Windows::Foundation::IInspectable const&, Microsoft::UI::Xaml::SizeChangedEventArgs const&);
        void ChatTitleTrimmingChanged(Microsoft::UI::Xaml::Controls::TextBlock const& label,
            Microsoft::UI::Xaml::Controls::IsTextTrimmedChangedEventArgs const&);
        void ChatRowPrepared(Microsoft::UI::Xaml::FrameworkElement const& sender,
            Microsoft::UI::Xaml::DataContextChangedEventArgs const& args);
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
        friend struct AgentChatWindowTestAccess;
        friend struct CompletionWindowTestAccess;
        friend struct TextInputWindowTestAccess;
        friend struct App;
        struct DocumentTab {
            Windows::Foundation::IInspectable item{nullptr};
            std::uint64_t document = 0;
            Microsoft::UI::Xaml::FrameworkElement::SizeChanged_revoker closeButtonSizeChanged;
            winrt::weak_ref<MainWindow> owner;
            winrt::weak_ref<MainWindow> tearOutWindow;
        };
        void RegisterTabTransferHandlers();
        struct GroupDropTarget { EditorGroup group; std::size_t index; float indicatorX; bool onStrip; };
        EditorGroup GroupAtWorkspacePoint(Windows::Foundation::Point point);
        std::optional<EditorGroup> TabStripAt(Windows::Foundation::Point point);
        std::optional<GroupDropTarget> GroupDropAt(Windows::Foundation::Point point, std::uint64_t view);
        void SetTabDropIndicator(std::optional<GroupDropTarget> const& drop);
        void StartNativeTabDrag(Microsoft::UI::Input::EnteringMoveSizeEventArgs const&);
        void TrackNativeTabDrag(Microsoft::UI::Input::WindowRectChangingEventArgs const&);
        void FinishNativeTabDrag();
        void RefreshTabCaptionRegion();
        MainWindow* CreateTabWindow();
        void SynchronizeTabOrder();
        void QueueTabOrderSync();
        bool CanAcceptTab(std::shared_ptr<DocumentTab> const& tab) const;
        bool TransferTab(std::shared_ptr<DocumentTab> tab, std::size_t index, EditorGroup group = EditorGroup::Left);
        MainWindow* BeginTabTearOut(std::shared_ptr<DocumentTab> const& tab);
        bool CompleteTabTearOut(std::shared_ptr<DocumentTab> const& tab);
        bool CanDockTab(std::shared_ptr<DocumentTab> const& tab) const;
        bool ContainsTabDragPoint(Windows::Graphics::PointInt32 point, EditorGroup group = EditorGroup::Left);
        struct TabHit { std::uint64_t id; float grabOffset; bool closable; };
        std::optional<TabHit> HitTestVisibleTab(Windows::Graphics::PointInt32 point);
        void BeginTabPointerGesture(Windows::Graphics::PointInt32 point);
        static LRESULT CALLBACK TabPressSubclass(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
        void EndTabPointerGesture();
        void EnterTabMove(Microsoft::UI::Input::InputNonClientPointerSource const& pointer);
        void ExitTabMove();
        void EndTabDrag();
        void RevealTabWindow();
        void AttachControllers();
        void ClaimDocumentPresentation();
        void AttachDocumentController(std::shared_ptr<winrt::Folia::WorkspaceDocument> const& document);
        winrt::Folia::EditorSession& editorSession() noexcept;
        winrt::Folia::EditorDocumentController& documentController() noexcept;
        bool HasActiveEditor() const noexcept { return bool(workspaceDocuments.Active()->editor); }
        bool IsActiveDocument(std::uint64_t id) const noexcept;
        void ActivateDocument(std::uint64_t id);
        void ActivateView(std::uint64_t view);
        void NewDocumentInGroup(EditorGroup group);
        void NewDocument();
        winrt::fire_and_forget OpenWorkspaceFile(std::filesystem::path path, winrt::Folia::WorkspaceOpenMode mode,
            std::optional<winrt::Folia::EditorSourceLocation> location = {});
        bool CanReplaceWorkspacePreview(std::uint64_t document) const;
        void PresentWorkspaceTab(WorkspaceOpenResult const& result);
        void ApplyDocumentLocation();
        void OpenChatDocument(std::uint64_t chatDocument, std::string_view tabKey, std::filesystem::path const&, EditorSourceLocation);
        std::uint64_t lastEditorDocument = 0;
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
        bool BeginTabMiddlePress(Windows::Graphics::PointInt32 point);
        bool ReleaseTabMiddlePress(Windows::Graphics::PointInt32 point);
        void CancelTabMiddlePress();
        Microsoft::UI::Xaml::Controls::TabView GroupTabs(EditorGroup group);
        Microsoft::UI::Xaml::Controls::ScrollViewer TabStripScroller(EditorGroup group = EditorGroup::Left);
        bool ScrollTabStrip(double steps, EditorGroup group);
        bool ScrollTabsWithWheel(int delta, EditorGroup group = EditorGroup::Left);
        void RegisterGroupTabs(EditorGroup group);
        void PrepareGroupTabStrip(EditorGroup group);
        void RefreshDocumentTabs();
        void SelectDocumentTab();
        void UpdateDocumentTab(std::uint64_t id);
        void UpdateTabHeader(Microsoft::UI::Xaml::Controls::TabViewItem const&, WorkspaceDocument const&);
        std::uint64_t SelectedEditorView() const;
        std::uint64_t FindDocumentView(std::uint64_t document) const;
        void RegisterEditorGroups();
        void SplitEditor();
        bool MoveEditorView(std::uint64_t view, EditorGroup group, std::size_t index);
        void UpdateEditorGroupLayout();
        void UpdateReadingGroupCorners();
        void ResizeEditorGroups(double delta);
        void SaveEditorView(std::uint64_t view);
        void RestoreEditorView(WorkspaceEditorView const& view);
        void QueueOtherGroupRender();
        void RenderOtherGroup();
        void ReleaseOtherGroupRenderer();
        bool FocusOtherGroup();
        void UpdateWorkspaceLayout();
        void UpdateChromeMetrics();
        Microsoft::UI::Xaml::Controls::Button NewTabButton(EditorGroup group = EditorGroup::Left);
        void ApplyBarLayout();
        void RegisterWorkspaceLayout();
        enum class WorkspacePane { Files, Outline, Chats };
        void SelectWorkspacePane(WorkspacePane);
        void RegisterAgentChats();
        void RefreshChatList();
        void SelectChatListItem(std::string const& id);
        void OpenAgentChat(std::string id = {}, WorkspaceOpenMode mode = WorkspaceOpenMode::Preview);
        chat::ViewHost ChatHostFor(std::shared_ptr<WorkspaceDocument> const& document);
        void TakeChatPage(MainWindow& source, std::shared_ptr<WorkspaceDocument> const& document);
        void UpdateAgentChatPages();
        void UpdateAgentChatTheme();
        void UpdateAgentChatMedia();
        void ReleaseClosedAgentChats();
        winrt::fire_and_forget ManageAgentChat(std::string id, bool remove);
        void SetSidebarOpen(bool open);
        void UpdateWorkspaceAppearance(WorkspaceAppearance const& settings);
        void EnsureSelectedTabVisible();
        void UpdateLoadingState();
        bool DispatchApplicationAction(std::string_view action);
        void RegisterApplicationShortcuts();
        winrt::fire_and_forget CloseDocumentAsync(std::uint64_t id);
        winrt::fire_and_forget CloseEditorViewAsync(std::uint64_t view);
        void RegisterWindowHandlers();
        void RegisterFileDrops();
        winrt::fire_and_forget DropWorkspaceFiles(Microsoft::UI::Xaml::DragEventArgs args, EditorGroup group);
        Windows::Foundation::IAsyncAction OpenDroppedItemsAsync(Windows::ApplicationModel::DataTransfer::DataPackageView data, EditorGroup group);
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
        void UpdateSelectedTabBackdrop();
        folia::Theme CurrentThemeVariant();
        bool SettingsActive() const noexcept { return workspaceDocuments.Active()->kind == WorkspaceTabKind::Settings; }
        void OpenSettingsTab();
        void ReturnToDocumentTab();
        void ShowSettingsPage(bool enabled);
        void PlaceSettingsView(bool active);
        void SetDocumentCommandsVisible(bool visible);
        std::optional<winrt::hstring> ApplySettings(winrt::Folia::AppSettings const& settings);
        void UpdateMediaSettings(winrt::Folia::MediaDisplaySettings const& settings);
        void ApplyMediaToViews();
        void RegisterCommandHandlers();
        void ToggleSourceMode();
        void UpdateSourceModeUi();
        void UpdateDocumentInfo(bool notifyPeers = true);
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
        void HandleEditorPointerPressed(Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
        void ShowEditorContextMenu(Microsoft::UI::Xaml::Input::ContextRequestedEventArgs const& args);
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

        std::chrono::steady_clock::time_point lastActivated{};
        winrt::Folia::WorkspaceDocuments workspaceDocuments;
        WorkspaceEditorGroups editorGroups;
        std::uint64_t presentedEditorView = 0;
        double splitFraction = .5;
        std::unique_ptr<EditorSurfaceRenderer> otherEditorRenderer;
        EditorScrollController otherScrollController;
        EditorAnimationController otherAnimationController;
        std::uint64_t otherRenderedView = 0;
        std::uint64_t otherImageDocument = 0;
        bool otherGroupRenderPending = false;
        weak_ref<MainWindow> dropWindow;
        std::optional<GroupDropTarget> pendingDrop;
        weak_ref<MainWindow> detachedFrom;
        EditorGroup detachedGroup = EditorGroup::Left;
        std::size_t detachedIndex = 0;
        Windows::Graphics::PointInt32 windowGrab{};
        struct TabStripParts {
            winrt::weak_ref<Microsoft::UI::Xaml::FrameworkElement> list;
            winrt::weak_ref<Microsoft::UI::Xaml::Controls::ScrollViewer> scroller;
            std::array<winrt::weak_ref<Microsoft::UI::Xaml::Controls::Primitives::RepeatButton>, 2> arrows;
            std::optional<double> requestedOffset;
        };
        std::array<TabStripParts, 2> tabStripParts;
        winrt::Folia::FolderExplorerController folderExplorer;
        winrt::Folia::RecentWorkspace recentWorkspace;
        winrt::Folia::EditorSurfaceRenderer editorRenderer;
        Microsoft::UI::Xaml::XamlRoot::Changed_revoker editorRootChanged;
        double editorRasterizationScale = 0.;
        winrt::Folia::EditorScrollController scrollController;
        winrt::Folia::EditorAnimationController animationController;
        winrt::Folia::EditorTextInputController textInputController;
        winrt::Folia::EditorPointerController pointerController;
        Microsoft::UI::Xaml::Controls::MenuFlyout documentContextMenu{nullptr};
        Microsoft::UI::Xaml::Controls::MenuFlyout activeEditorContextMenu{nullptr};
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
        std::optional<std::uint64_t> middlePressedTab;
        bool tabRegionsPending = false;
        bool updatingTabs = false;
        DocumentTools documentTools;
        DocumentTools otherDocumentTools;
        WorkspaceSidebarMotion sidebarMotion;
        std::optional<WindowMaterial> appliedMaterial;
        std::optional<WindowMaterial> appliedReadingMaterial;
        Microsoft::UI::Xaml::Media::CompositionTarget::Rendering_revoker tabBackdropFrame;
        WorkspacePane workspacePane = WorkspacePane::Files;
        std::unordered_map<std::string, int> chatListIndex;
        std::unordered_map<std::uint64_t, std::shared_ptr<chat::ChatView>> agentChatViews;
        bool updatingRailSelection = false;
        TabDragPresentation dragPresentation;
        std::optional<float> tabGrabOffset;
        std::optional<Windows::Graphics::PointInt32> nativeTabPress;
        bool operationActive = false;
        bool windowClosed = false;
        Windows::Foundation::IAsyncAction imagePreviewLoad{nullptr};
        std::uint64_t imagePreviewGeneration = 0;
        std::uint64_t folderOpenGeneration = 0;
        std::uint64_t workspaceThemeRevision = 1;
        std::unordered_map<std::uint64_t, std::shared_ptr<DocumentTab>> tabItems;
        Windows::Foundation::Collections::IObservableVector<Windows::Foundation::IInspectable> tabSource =
            winrt::single_threaded_observable_vector<Windows::Foundation::IInspectable>();
        Windows::Foundation::Collections::IObservableVector<Windows::Foundation::IInspectable> rightTabSource =
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
