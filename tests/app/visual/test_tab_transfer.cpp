#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "App.xaml.h"
#include "../support/TestPaths.h"
#include <wil/resource.h>
#include "visual_window.h"
#include "EditorPixelCapture.h"
#include "MouseInput.h"
#include "MainWindow.xaml.h"
#include "windows/media/images/ImageSource.h"
#include "workspace/WorkspaceIcons.h"
#include <dwmapi.h>
#include <winrt/Microsoft.UI.Composition.SystemBackdrops.h>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Peers.h>
#include <winrt/Microsoft.UI.Xaml.Automation.Provider.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>

#pragma comment(lib, "gdi32.lib")

import folia.markdown.instrumentation;

namespace winrt::Folia {
    struct EditorRendererTestAccess {
        static bool Ready(EditorSurfaceRenderer const& renderer) { return renderer.resources.drawingSurface != nullptr; }
        static auto& Resources(EditorSurfaceRenderer& renderer) { return renderer.resources; }
        static std::optional<D2D1_RECT_F> Rule(EditorSurfaceRenderer const& renderer) {
            for (auto const& block : renderer.interactionMap.blocks)
                if (block.thematicBreak) return block.rect;
            return {};
        }
        static bool PresentPending(EditorSurfaceRenderer const& renderer) { return renderer.presentPending; }
    };
}

namespace winrt::Folia::implementation {
    // Inspect ownership and call the same transfer used by WinUI's native events.
    // Tests close only their own newly created windows, without a save dialog.
    struct WorkspaceWindowTestAccess {
        static MainWindow* Impl(Microsoft::UI::Xaml::Window const& window) {
            return winrt::get_self<MainWindow>(window.as<winrt::Folia::MainWindow>());
        }
        static auto& Documents(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->workspaceDocuments; }
        static auto& Groups(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->editorGroups; }
        static auto DropItems(Microsoft::UI::Xaml::Window const& window, Windows::Foundation::Point point,
                              Windows::ApplicationModel::DataTransfer::DataPackageView data) {
            auto owner = Impl(window);
            return owner->OpenDroppedItemsAsync(data, owner->GroupAtWorkspacePoint(point));
        }
        static void ExternalFile(std::filesystem::path const& path) {
            App::Current()->OpenWorkspaceWindows({false, {path}});
        }
        static auto GroupTabs(Microsoft::UI::Xaml::Window const& window, EditorGroup group) { return Impl(window)->GroupTabs(group); }
        static auto OtherRenderer(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->otherEditorRenderer.get(); }
        static void ResizeGroups(Microsoft::UI::Xaml::Window const& window, double delta) { Impl(window)->ResizeEditorGroups(delta); }
        static void SelectView(Microsoft::UI::Xaml::Window const& window, std::uint64_t view) { Impl(window)->ActivateView(view); }
        static void CloseView(Microsoft::UI::Xaml::Window const& window, std::uint64_t view) { Impl(window)->CloseEditorViewAsync(view); }
        static bool MoveView(Microsoft::UI::Xaml::Window const& window, std::uint64_t view, EditorGroup group, std::size_t index) {
            return Impl(window)->MoveEditorView(view, group, index);
        }
        static auto Tabs(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->DocumentTabs(); }
        static auto NewTabButton(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->NewTabButton(); }
        static auto Handle(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->WindowHandle(); }
        static auto About(Microsoft::UI::Xaml::Window const& window) {
            auto impl = Impl(window);
            impl->OpenSettingsTab();
            impl->settingsView->Navigate(winrt::Folia::SettingsView::Page::About);
            window.Content().as<Microsoft::UI::Xaml::FrameworkElement>().UpdateLayout();
            return impl->SettingsViewHost();
        }
        static void NewChat(Microsoft::UI::Xaml::Window const& window) { Impl(window)->OpenAgentChat(); }
        static void ReturnToDocumentTab(Microsoft::UI::Xaml::Window const& window) { Impl(window)->ReturnToDocumentTab(); }
        static auto ThemeSettings(Microsoft::UI::Xaml::Window const& window) {
            auto impl = Impl(window);
            impl->OpenSettingsTab();
            impl->settingsView->Navigate(winrt::Folia::SettingsView::Page::Themes);
            return impl->SettingsViewHost();
        }
        static void Theme(Microsoft::UI::Xaml::Window const& window, std::string id) {
            auto impl = Impl(window);
            impl->appSettings.themeId = std::move(id);
            impl->UpdateTheme();
        }
        static bool ContainsDragPoint(Microsoft::UI::Xaml::Window const& window, Windows::Graphics::PointInt32 point) {
            return Impl(window)->ContainsTabDragPoint(point);
        }
        static void PointerBegin(Microsoft::UI::Xaml::Window const& window, Windows::Graphics::PointInt32 point) { Impl(window)->BeginTabPointerGesture(point); }
        static bool MiddleMouse(WPARAM message, MOUSEHOOKSTRUCT const& mouse) {
            return App::RouteTabMouse(message, reinterpret_cast<LPARAM>(&mouse));
        }
        static void Sidebar(Microsoft::UI::Xaml::Window const& window, bool open) { Impl(window)->SetSidebarOpen(open); }
        static bool ScrollTabs(Microsoft::UI::Xaml::Window const& window, int delta, EditorGroup group = EditorGroup::Left) {
            return Impl(window)->ScrollTabsWithWheel(delta, group);
        }
        static auto TabScroller(Microsoft::UI::Xaml::Window const& window, EditorGroup group = EditorGroup::Left) {
            return Impl(window)->TabStripScroller(group);
        }
        static void Pane(Microsoft::UI::Xaml::Window const& window, bool outline) {
            Impl(window)->SelectWorkspacePane(outline ? MainWindow::WorkspacePane::Outline : MainWindow::WorkspacePane::Files);
        }
        static void PointerEnd(Microsoft::UI::Xaml::Window const& window) { Impl(window)->EndTabPointerGesture(); }
        static void EnterMove(Microsoft::UI::Xaml::Window const& window) {
            Impl(window)->EnterTabMove(Microsoft::UI::Input::InputNonClientPointerSource::GetForWindowId(window.AppWindow().Id()));
        }
        static void ExitMove(Microsoft::UI::Xaml::Window const& window) { Impl(window)->ExitTabMove(); }
        static void Moving(Microsoft::UI::Xaml::Window const& window, bool value) { Impl(window)->tabMoveInProgress = value; }
        static bool PressedTab(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->tabGrabOffset.has_value(); }
        static bool Idle(Microsoft::UI::Xaml::Window const& window) { return !Impl(window)->tabPointerDown && !Impl(window)->tabMoveInProgress; }
        static bool PendingDropIn(Microsoft::UI::Xaml::Window const& window, Microsoft::UI::Xaml::Window const& target) {
            auto impl = Impl(window);
            return impl->pendingDrop.has_value() && impl->dropWindow.get().get() == Impl(target);
        }
        static auto Items(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->tabSource; }
        static auto Binding(Microsoft::UI::Xaml::Window const& window, std::uint64_t id) { return Impl(window)->tabItems.at(id); }
        static auto View(Microsoft::UI::Xaml::Window const& window, std::uint64_t id) {
            window.Content().as<Microsoft::UI::Xaml::FrameworkElement>().UpdateLayout();
            return Tabs(window).ContainerFromItem(Binding(window, id)->item).as<Microsoft::UI::Xaml::Controls::TabViewItem>();
        }
        static auto Begin(Microsoft::UI::Xaml::Window const& window, std::uint64_t id) {
            return Impl(window)->BeginTabTearOut(Binding(window, id))->get_strong().as<winrt::Folia::MainWindow>().as<Microsoft::UI::Xaml::Window>();
        }
        static bool Complete(Microsoft::UI::Xaml::Window const& window, Windows::Foundation::IInspectable const& item) {
            auto id = unbox_value<std::uint64_t>(item);
            auto owner = App::Current()->FindTabWindow(id);
            return owner && Impl(window)->CompleteTabTearOut(owner->tabItems.at(id));
        }
        static bool AllowDrop(Microsoft::UI::Xaml::Window const& window, Windows::Foundation::IInspectable const& item) {
            auto id = unbox_value<std::uint64_t>(item);
            auto owner = App::Current()->FindTabWindow(id);
            return owner && Impl(window)->CanDockTab(owner->tabItems.at(id));
        }
        static void End(Microsoft::UI::Xaml::Window const& window) { Impl(window)->EndTabDrag(); }
        static DWORD Cloaked(Microsoft::UI::Xaml::Window const& window) {
            DWORD value = 0;
            winrt::check_hresult(::DwmGetWindowAttribute(Impl(window)->WindowHandle(), DWMWA_CLOAKED, &value, sizeof(value)));
            return value;
        }
        static void New(Microsoft::UI::Xaml::Window const& window) { Impl(window)->NewDocument(); }
        static void Edit(Microsoft::UI::Xaml::Window const& window, std::u32string_view text) {
            if (!Impl(window)->ExecuteEditorCommand(folia::Command::InsertText(std::u32string{text}))) throw std::runtime_error("test edit failed");
        }
        static auto TearOutWindow(Microsoft::UI::Xaml::Window const& source) {
            return Impl(source)->CreateTabWindow()->get_strong().as<winrt::Folia::MainWindow>().as<Microsoft::UI::Xaml::Window>();
        }
        static bool Transfer(Microsoft::UI::Xaml::Window const& source, std::uint64_t id,
            Microsoft::UI::Xaml::Window const& target, std::size_t index) {
            return Impl(target)->TransferTab(Binding(source, id), index);
        }
        static void Close(Microsoft::UI::Xaml::Window const& window) { Impl(window)->closeApproved = true; window.Close(); }
        static void RequestNativeClose(Microsoft::UI::Xaml::Window const& window) {
            window.AppWindow().Hide();
            ::PostMessageW(Impl(window)->WindowHandle(), WM_CLOSE, 0, 0);
        }
        static bool Closed(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->windowClosed; }
        static void CompletionStatus(Microsoft::UI::Xaml::Window const& window, winrt::Folia::CompletionState state) {
            Impl(window)->UpdateCompletionStatus(state);
        }
        static void Render(Microsoft::UI::Xaml::Window const& window) { Impl(window)->RenderEditorSurface(); }
        static void Open(Microsoft::UI::Xaml::Window const& window, std::filesystem::path path,
            WorkspaceOpenMode mode = WorkspaceOpenMode::Keep) {
            Impl(window)->OpenWorkspaceFile(std::move(path), mode);
        }
        static void OpenFolder(Microsoft::UI::Xaml::Window const& window, std::filesystem::path const& path) {
            Impl(window)->OpenWorkspaceFolder(path);
        }
        static void SetRecentHistory(Microsoft::UI::Xaml::Window const& window, std::filesystem::path const& path) {
            Impl(window)->recentWorkspace = RecentWorkspace(path);
        }
        static void Bars(Microsoft::UI::Xaml::Window const& window, WorkspaceAppearance settings) {
            Impl(window)->UpdateWorkspaceAppearance(settings);
        }
        static void Hover(Microsoft::UI::Xaml::Window const& window, Windows::Foundation::Point point) {
            Impl(window)->documentTools.OnPointer(point);
        }
        static void Progress(Microsoft::UI::Xaml::Window const& window, bool active, double value) {
            Impl(window)->SetOperationProgress(active, value, active);
            Impl(window)->UpdateLoadingState();
        }
        static auto OpenFileMenu(Microsoft::UI::Xaml::Window const& window) {
            auto button = Impl(window)->DocumentActionsButton();
            auto flyout = button.Flyout();
            flyout.ShowAt(button);
            return flyout;
        }
        static auto RecentFiles(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->RecentFilesMenu().Items(); }
        static auto RecentFolders(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->RecentFoldersMenu().Items(); }
        static auto FolderRoot(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->folderExplorer.RootPath(); }
        static auto Preview(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->WorkspaceImage(); }
        static auto PreviewViewer(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->ImagePreviewViewer(); }
        static auto PreviewMessage(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->ImagePreviewMessage(); }
        static auto Rename(Microsoft::UI::Xaml::Window const& window,
            std::filesystem::path const& before, std::filesystem::path const& after) {
            return Impl(window)->ApplyExplorerActionAsync({folia::platform::WorkspaceFileActionKind::Rename, before, after, false});
        }
        static auto& Renderer(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->editorRenderer; }
        static bool MaterialsReady(Microsoft::UI::Xaml::Window const& window) {
            return Impl(window)->appliedMaterial.has_value() && Impl(window)->appliedReadingMaterial.has_value();
        }
        static auto Notification(Microsoft::UI::Xaml::Window const& window, std::size_t& calls) {
            return Impl(window)->WindowEvent([&calls](auto const&, auto const&) { ++calls; });
        }
    };
}

namespace {
    using namespace boost::ut;
    using namespace document_tests;
    using namespace winrt::Microsoft::UI::Xaml;
    using Access = winrt::Folia::implementation::WorkspaceWindowTestAccess;
    struct OwnedWindow {
        Window window{nullptr};
        explicit OwnedWindow(Window value) : window(value) {
            Access::Bars(window, {});
            window.Activate();
            Pump([&] { return window.Content().as<FrameworkElement>().IsLoaded(); });
            Pump([&] { return winrt::Folia::EditorRendererTestAccess::Ready(Access::Renderer(window)); });
        }
        ~OwnedWindow() { if (window && !Access::Closed(window)) Access::Close(window); }
    };
    auto App() { return winrt::Folia::implementation::App::Current(); }
    FrameworkElement TabPart(DependencyObject const& element, std::wstring_view name) {
        if (auto value = element.try_as<FrameworkElement>(); value && value.Name() == name) return value;
        for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(element); ++i)
            if (auto value = TabPart(Media::VisualTreeHelper::GetChild(element, i), name)) return value;
        return nullptr;
    }
    template<class T> T FirstControl(DependencyObject const& element) {
        if (auto value = element.try_as<T>()) return value;
        for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(element); ++i)
            if (auto value = FirstControl<T>(Media::VisualTreeHelper::GetChild(element, i))) return value;
        return nullptr;
    }

    // Capture the compositor output. RenderTargetBitmap omits native backdrops
    // and cannot detect a seam created by an external DXGI visual.
    auto CaptureColors(Window const& window, std::span<winrt::Windows::Graphics::PointInt32 const> points) {
        auto hwnd = Access::Handle(window);
        winrt::check_hresult(::DwmFlush());
        struct ScreenDc {
            HDC value = ::GetDC(nullptr);
            ~ScreenDc() { if (value) ::ReleaseDC(nullptr, value); }
        } screen;
        winrt::check_bool(screen.value != nullptr);
        std::vector<std::array<int, 3>> colors;
        colors.reserve(points.size());
        for (auto point : points) {
            if (::GetAncestor(::WindowFromPoint({point.X, point.Y}), GA_ROOT) != hwnd)
                throw std::runtime_error("Reading surface test window is obscured");
            auto pixel = ::GetPixel(screen.value, point.X, point.Y);
            if (pixel == CLR_INVALID) throw std::runtime_error("Could not sample the reading surface");
            colors.push_back({GetRValue(pixel), GetGValue(pixel), GetBValue(pixel)});
        }
        return colors;
    }
    using ScreenPoint = winrt::Windows::Graphics::PointInt32;

    FrameworkElement FirstTab(Window const& window, winrt::Folia::EditorGroup group) {
        auto tabs = Access::GroupTabs(window, group);
        FrameworkElement item{nullptr};
        Pump([&] {
            window.Content().as<FrameworkElement>().UpdateLayout();
            item = tabs.ContainerFromIndex(0).try_as<FrameworkElement>();
            return item && item.IsLoaded() && item.ActualWidth() > 0;
        });
        return item;
    }

    ScreenPoint CenterOnScreen(FrameworkElement const& element) {
        element.UpdateLayout();
        return element.XamlRoot().CoordinateConverter().ConvertLocalToScreen(element.TransformToVisual(nullptr).TransformPoint(
            {static_cast<float>(element.ActualWidth() / 2), static_cast<float>(element.ActualHeight() / 2)}));
    }

    // The native move loop is modal. Feed a real mouse gesture from the input thread
    // while the test UI thread dispatches normally, including a physical button release.
    void DragTabAlong(Window const& source, FrameworkElement const& item, std::span<ScreenPoint const> route) {
        auto start = CenterOnScreen(item);
        auto hwnd = Access::Handle(source);
        source.Activate();
        ::SetForegroundWindow(hwnd);
        Pump([&] { return ::GetAncestor(::WindowFromPoint({start.X, start.Y}), GA_ROOT) == hwnd; });
        POINT previous{};
        ::GetCursorPos(&previous);
        auto restore = wil::scope_exit([&] { ::SetCursorPos(previous.x, previous.y); });
        std::atomic<bool> released = false;
        std::jthread mouse([&] {
            auto button = [](DWORD flags) { INPUT input{}; input.type = INPUT_MOUSE; input.mi.dwFlags = flags; ::SendInput(1, &input, sizeof(input)); };
            ::SetCursorPos(start.X, start.Y);
            ::Sleep(65);
            button(MOUSEEVENTF_LEFTDOWN);
            ::Sleep(95);
            auto from = start;
            for (auto point : route) {
                for (int i = 1; i <= 32; ++i) {
                    ::SetCursorPos(from.X + (point.X-from.X)*i/32, from.Y + (point.Y-from.Y)*i/32);
                    ::Sleep(16);
                }
                ::Sleep(160);
                from = point;
            }
            button(MOUSEEVENTF_LEFTUP);
            released = true;
        });
        Pump([&] { return released.load(); });
        Pump([&] { return Access::Idle(source); });
    }

    void DragTab(Window const& source, FrameworkElement const& item, ScreenPoint end) {
        DragTabAlong(source, item, std::span{&end, 1});
    }

    suite tab_transfer_tests = [] {
        "native pointer moves both untitled views into the same editor group"_test = [] {
            using winrt::Folia::EditorGroup;
            OwnedWindow source(App()->CreateMainWindow());
            auto win = source.window;
            auto root = win.Content().as<FrameworkElement>();
            auto& groups = Access::Groups(win);
            Automation::Peers::ButtonAutomationPeer(root.FindName(L"SplitEditorButton").as<Controls::Button>())
                .as<Automation::Provider::IInvokeProvider>().Invoke();
            auto left = groups.Selected(EditorGroup::Left), right = groups.Selected(EditorGroup::Right);
            Pump([&] { return Access::GroupTabs(win, EditorGroup::Right).ContainerFromIndex(0) != nullptr; });
            auto leftTab = FirstTab(win, EditorGroup::Left);
            auto rightTab = FirstTab(win, EditorGroup::Right);
            DragTab(win, rightTab, CenterOnScreen(leftTab));
            Pump([&] { return !groups.IsSplit(); });
            expect(groups.Find(left) && groups.Find(right) && left != right);
            expect(groups.Views(EditorGroup::Left).size() == 2_u && groups.Selected(EditorGroup::Left) == right);
            expect(Access::Documents(win).Items().size() == 1_u);
        };
        "native pointer tears a distinct file out of a split and docks a standalone tab into either side"_test = [] {
            using winrt::Folia::EditorGroup;
            auto base = visual_tests::Window().AppWindow();
            base.Hide();
            auto restoreBase = wil::scope_exit([&] { base.Show(false); });
            OwnedWindow source(App()->CreateMainWindow());
            auto win = source.window;
            auto root = win.Content().as<FrameworkElement>();
            RECT work{};
            ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
            const auto w = (work.right-work.left)/2 - 20, h = (work.bottom-work.top)*2/3;
            auto split = [&] {
                Automation::Peers::ButtonAutomationPeer(root.FindName(L"SplitEditorButton").as<Controls::Button>())
                    .as<Automation::Provider::IInvokeProvider>().Invoke();
                Pump([&] { return Access::Groups(win).IsSplit(); });
            };
            win.AppWindow().MoveAndResize({work.left + 6, work.top + 20, w, h});
            split();
            auto& groups = Access::Groups(win);
            auto duplicate = groups.Selected(EditorGroup::Right);
            Access::New(win);
            auto outgoing = groups.Selected(EditorGroup::Right);
            Access::Edit(win, U"separate unsaved document");
            auto document = Access::Documents(win).Active();
            Access::CloseView(win, duplicate);
            Pump([&] { return groups.IsSplit() && groups.Views(EditorGroup::Right).size() == 1; });
            auto tab = FirstTab(win, EditorGroup::Right);
            DragTab(win, tab, {work.left + w + w/3, work.top + 2*h/3});
            Pump([&] { return App()->FindTabWindow(outgoing) != Access::Impl(win); });
            auto floating = App()->FindTabWindow(outgoing);
            if (!expect(fatal(floating != nullptr))) return;
            auto floatingWin = floating->get_strong().as<winrt::Folia::MainWindow>().as<Window>();
            expect(floatingWin.AppWindow().IsVisible() && Access::Cloaked(floatingWin) == 0_u);
            OwnedWindow detached(floatingWin);
            expect(!groups.IsSplit() && Access::Documents(floatingWin).Active() == document);
            expect(document->editor->session.TextUtf8() == "separate unsaved document");
            split();
            auto dock = [&](Window const& moving, EditorGroup group) {
                auto id = Access::Groups(moving).Selected(EditorGroup::Left);
                moving.AppWindow().MoveAndResize({work.right - w - 6, work.top + 20, w, h});
                win.Activate();
                root.UpdateLayout();
                auto target = FirstTab(win, group);
                auto destination = CenterOnScreen(target);
                auto item = FirstTab(moving, EditorGroup::Left);
                DragTab(moving, item, destination);
                Pump([&] { return groups.GroupOf(id) == group && Access::Closed(moving); });
            };
            dock(floatingWin, EditorGroup::Right);
            expect(groups.Selected(EditorGroup::Right) == outgoing);
            OwnedWindow second(App()->CreateMainWindow());
            auto incoming = Access::Groups(second.window).Selected(EditorGroup::Left);
            dock(second.window, EditorGroup::Left);
            expect(groups.Selected(EditorGroup::Left) == incoming);
        };
        "native single-tab drag can leave a dock preview before release without transferring"_test = [] {
            using winrt::Folia::EditorGroup;
            auto base = visual_tests::Window().AppWindow();
            base.Hide();
            auto restoreBase = wil::scope_exit([&] { base.Show(false); });
            OwnedWindow source(App()->CreateMainWindow()), target(App()->CreateMainWindow());
            RECT work{};
            winrt::check_bool(::SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0));
            const auto width = (work.right - work.left) / 2 - 20;
            const auto height = (work.bottom - work.top) / 2;
            source.window.AppWindow().MoveAndResize({work.left + 6, work.top + 20, width, height});
            target.window.AppWindow().MoveAndResize({work.right - width - 6, work.top + 20, width, height});
            auto document = Access::Documents(source.window).Active();
            auto destination = Access::Documents(target.window).Active();
            auto sourceTab = FirstTab(source.window, EditorGroup::Left);
            auto targetPoint = CenterOnScreen(FirstTab(target.window, EditorGroup::Left));
            std::array route{targetPoint, ScreenPoint{targetPoint.X, work.top + (work.bottom - work.top) * 4 / 5}};
            auto pointer = winrt::Microsoft::UI::Input::InputNonClientPointerSource::GetForWindowId(source.window.AppWindow().Id());
            bool offered = false, leftOffer = false, changedOwner = false;
            auto changing = pointer.WindowRectChanging([&](auto const&, auto const&) {
                if (Access::PendingDropIn(source.window, target.window)) offered = true;
                else if (offered) leftOffer = true;
                changedOwner |= Access::Documents(source.window).Active() != document
                    || Access::Documents(target.window).Find(document->id) != nullptr;
            });
            auto removeChanging = wil::scope_exit([&] { pointer.WindowRectChanging(changing); });
            DragTabAlong(source.window, sourceTab, route);
            expect(offered && leftOffer);
            expect(!changedOwner);
            expect(Access::Documents(source.window).Active() == document);
            expect(Access::Documents(target.window).Active() == destination);
            expect(Access::Documents(source.window).Items().size() == 1_u);
            expect(Access::Documents(target.window).Items().size() == 1_u);
        };
        "split groups can detach one dirty document view while the other window retains its history"_test = [] {
            using winrt::Folia::EditorGroup;
            OwnedWindow source(App()->CreateMainWindow()), target(App()->CreateMainWindow());
            auto win = source.window;
            win.Activate();
            Access::Edit(win, U"shared");
            auto document = Access::Documents(win).Active();
            auto button = win.Content().as<FrameworkElement>().FindName(L"SplitEditorButton").as<Controls::Button>();
            Automation::Peers::ButtonAutomationPeer(button).as<Automation::Provider::IInvokeProvider>().Invoke();
            auto view = Access::Groups(win).Selected(EditorGroup::Right);
            expect(Access::Transfer(win, view, target.window, 0));
            expect(!Access::Groups(win).IsSplit() && Access::Documents(win).Active() == document);
            target.window.Activate();
            Access::Edit(target.window, U" again");
            expect(document->editor->session.TextUtf8() == "shared again");
            Access::CloseView(target.window, view);
            Pump([&] { return !Access::Groups(target.window).Find(view); });
            win.Activate();
            expect(Access::Documents(win).Active() == document && document->IsModified());
            expect(document->editor->session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            expect(document->editor->session.TextUtf8() == "shared");
        };
        "split groups move the reading material when switching right tabs"_test = [] {
            using winrt::Folia::EditorGroup;
            using winrt::Folia::WindowMaterial;
            using winrt::Microsoft::UI::Composition::RectangleClip;
            using winrt::Microsoft::UI::Composition::SystemBackdrops::DesktopAcrylicController;
            using winrt::Microsoft::UI::Composition::SystemBackdrops::MicaController;
            using namespace Controls;
            if (!MicaController::IsSupported() || !DesktopAcrylicController::IsSupported()
                || winrt::Windows::UI::ViewManagement::AccessibilitySettings().HighContrast()) return;
            OwnedWindow workspace(App()->CreateMainWindow());
            auto window = workspace.window;
            auto root = window.Content().as<FrameworkElement>();
            Access::Bars(window, {false, WindowMaterial::Acrylic, WindowMaterial::Mica});
            auto invoke = [](Button const& button) {
                Automation::Peers::ButtonAutomationPeer(button).as<Automation::Provider::IInvokeProvider>().Invoke();
            };
            invoke(root.FindName(L"SplitEditorButton").as<Button>());
            auto right = Access::GroupTabs(window, EditorGroup::Right);
            auto rightAdd = [&] { return right.as<IControlProtected>().GetTemplateChild(L"AddButton").try_as<Button>(); };
            Pump([&] { return rightAdd() && rightAdd().IsLoaded(); });
            invoke(rightAdd());
            invoke(rightAdd());

            auto layer = root.FindName(L"TabBackdropLayer").as<FrameworkElement>();
            auto rightFrame = root.FindName(L"RightTabBackdropFrame").as<FrameworkElement>();
            auto leftFrame = root.FindName(L"SelectedTabBackdropFrame").as<FrameworkElement>();
            auto left = Hosting::ElementCompositionPreview::GetElementVisual(leftFrame);
            auto visual = Hosting::ElementCompositionPreview::GetElementVisual(rightFrame);
            Pump([&] { return left.Clip() && visual.Clip(); });
            const auto leftEdge = left.Clip().as<RectangleClip>().Left();
            for (int index : {1, 2, 0, 2, 1}) {
                right.SelectedIndex(index);
                auto tab = right.ContainerFromIndex(index).as<TabViewItem>();
                const auto x = tab.TransformToVisual(layer).TransformPoint({0, 0}).X;
                Pump([&] {
                    auto clip = visual.Clip().as<RectangleClip>();
                    return std::abs(clip.Left() - x) < 1.f && std::abs(clip.Right() - x - tab.ActualWidth()) < 1.f;
                });
                expect(left.Clip().as<RectangleClip>().Left() == leftEdge);
                expect(rightFrame.TransformToVisual(layer).TransformPoint({0, 0}).X == 0.f);
                expect(rightFrame.ActualWidth() == layer.ActualWidth());
                // WinUI can leave this state on a tab after an ordinary click.
                // Exercise the template state directly as well as selection changes.
                expect(VisualStateManager::GoToState(tab, L"DragDropVisualVisible", false));
                auto drag = TabPart(tab, L"TabDragVisualContainer").as<Grid>();
                expect(drag.Visibility() == Visibility::Visible);
                expect(drag.Background().as<Media::SolidColorBrush>().Color().A == 0);
            }
        };
        "split groups share a document, align their tab strips, and close the duplicate without a save prompt"_test = [] {
            using winrt::Folia::EditorGroup;
            OwnedWindow workspace(App()->CreateMainWindow());
            auto window = workspace.window;
            auto root = window.Content().as<FrameworkElement>();
            auto document = Access::Documents(window).Active();
            auto& groups = Access::Groups(window);
            Access::Edit(window, U"This document has unsaved changes.");
            auto splitter = root.FindName(L"EditorGroupSplitter").as<FrameworkElement>();
            auto splitButton = root.FindName(L"SplitEditorButton").as<Controls::Button>();
            Automation::Peers::ButtonAutomationPeer(splitButton).as<Automation::Provider::IInvokeProvider>().Invoke();
            Pump([&] {
                auto renderer = Access::OtherRenderer(window);
                return groups.IsSplit() && renderer && winrt::Folia::EditorRendererTestAccess::Ready(*renderer);
            });
            auto left = groups.Selected(EditorGroup::Left);
            auto right = groups.Selected(EditorGroup::Right);
            expect(left != right);
            expect(groups.ActiveGroup() == EditorGroup::Right);
            expect(groups.Find(left)->document == document->id && groups.Find(right)->document == document->id);
            expect(Access::Documents(window).Items().size() == 1_u);

            auto card = root.FindName(L"ReadingCard").as<FrameworkElement>();
            auto other = root.FindName(L"OtherReadingCard").as<FrameworkElement>();
            auto leftStrip = root.FindName(L"TabViewport").as<FrameworkElement>();
            auto rightStrip = root.FindName(L"RightTabViewport").as<FrameworkElement>();
            root.UpdateLayout();
            const auto leftWidth = other.ActualWidth();
            const auto rightWidth = card.ActualWidth();
            Access::ResizeGroups(window, 60.);
            root.UpdateLayout();
            expect(other.ActualWidth() > leftWidth && card.ActualWidth() < rightWidth);
            auto splitX = splitter.TransformToVisual(root).TransformPoint({0, 0}).X;
            auto tabSplitX = leftStrip.TransformToVisual(root).TransformPoint({static_cast<float>(leftStrip.ActualWidth()), 0}).X;
            expect(std::abs(splitX - tabSplitX) <= 1.);
            expect(rightStrip.IsLoaded() && rightStrip.ActualWidth() > 0);
            auto caption = root.FindName(L"CaptionInset").as<FrameworkElement>();
            auto rightStripEnd = rightStrip.TransformToVisual(root).TransformPoint({static_cast<float>(rightStrip.ActualWidth()), 0}).X;
            expect(rightStripEnd <= caption.TransformToVisual(root).TransformPoint({0, 0}).X + 1.);
            auto rightTabs = Access::GroupTabs(window, EditorGroup::Right);
            auto rightAdd = rightTabs.as<Controls::IControlProtected>().GetTemplateChild(L"AddButton").try_as<Controls::Button>();
            expect(rightAdd && rightAdd.Content().try_as<Controls::Viewbox>());
            auto rightTab = rightTabs.ContainerFromIndex(0).as<Controls::TabViewItem>();
            auto header = rightTab.as<Controls::IControlProtected>().GetTemplateChild(L"TabContainer").as<FrameworkElement>();
            auto leftEdge = [&](FrameworkElement const& element) { return element.TransformToVisual(root).TransformPoint({0, 0}).X; };
            auto visibleHeaderEdge = [&] { return (std::max)(leftEdge(header), leftEdge(rightStrip)); };
            auto tolerance = .5 / root.XamlRoot().RasterizationScale();
            expect(std::abs(visibleHeaderEdge() - leftEdge(card)) <= tolerance)
                << std::format("visible right header at {:.2f}, reading card at {:.2f}", visibleHeaderEdge(), leftEdge(card));
            expect(card.as<Controls::Grid>().CornerRadius().TopLeft == 0.);
            expect(other.as<Controls::Grid>().CornerRadius().TopLeft > 0.);

            auto originalCaret = document->editor->session.Selection().active;
            auto start = originalCaret;
            start.source_offset = 0;
            document->editor->session.SetSelection(folia::TextSelection::caret(start));
            Access::SelectView(window, left);
            expect(Access::Documents(window).Active() == document);
            expect(document->editor->session.Selection().active == originalCaret);
            expect(other.as<Controls::Grid>().CornerRadius().TopLeft == 0.);
            root.UpdateLayout();
            expect(std::abs(visibleHeaderEdge() - leftEdge(other)) <= tolerance);
            Access::SelectView(window, right);
            expect(document->editor->session.Selection().active == start);
            Access::CloseView(window, right);
            Pump([&] { return !groups.IsSplit() && splitter.Visibility() == Visibility::Collapsed; });
            expect(Access::Documents(window).Active() == document && document->IsModified());
            expect(Access::Documents(window).Items().size() == 1_u);
        };
        "split groups leave both blank title areas draggable"_test = [] {
            using winrt::Folia::EditorGroup;
            using winrt::Microsoft::UI::Input::NonClientRegionKind;
            OwnedWindow workspace(App()->CreateMainWindow());
            auto window = workspace.window;
            auto root = window.Content().as<FrameworkElement>();
            auto button = root.FindName(L"SplitEditorButton").as<Controls::Button>();
            Automation::Peers::ButtonAutomationPeer(button).as<Automation::Provider::IInvokeProvider>().Invoke();
            Pump([&] { return Access::Groups(window).IsSplit(); });
            root.UpdateLayout();

            auto id = window.AppWindow().Id();
            auto island = root.XamlRoot().CoordinateConverter();
            auto native = winrt::Microsoft::UI::Content::ContentCoordinateConverter::CreateForWindowId(id);
            auto input = winrt::Microsoft::UI::Input::InputNonClientPointerSource::GetForWindowId(id);
            auto captionAt = [&](FrameworkElement const& element) {
                auto center = element.TransformToVisual(nullptr).TransformPoint({static_cast<float>(element.ActualWidth() * .5),
                    static_cast<float>(element.ActualHeight() * .5)});
                auto point = native.ConvertScreenToLocal(island.ConvertLocalToScreen(center));
                return std::ranges::any_of(input.GetRegionRects(NonClientRegionKind::Caption), [&](auto rect) {
                    return point.X >= rect.X && point.X < rect.X + rect.Width && point.Y >= rect.Y && point.Y < rect.Y + rect.Height;
                });
            };
            for (auto group : {EditorGroup::Left, EditorGroup::Right}) {
                auto tabs = Access::GroupTabs(window, group);
                auto blank = tabs.TabStripFooter().as<FrameworkElement>();
                auto add = tabs.as<Controls::IControlProtected>().GetTemplateChild(L"AddButton").as<FrameworkElement>();
                auto tab = tabs.ContainerFromIndex(0).as<FrameworkElement>();
                Pump([&] { return captionAt(blank) && !captionAt(add) && captionAt(tab); });
            }
        };
        "split groups use the reading surface only on each selected tab"_test = [] {
            using winrt::Folia::EditorGroup;
            using winrt::Folia::WindowMaterial;
            using namespace Controls;
            OwnedWindow workspace(App()->CreateMainWindow());
            auto window = workspace.window;
            auto root = window.Content().as<FrameworkElement>();
            Access::Theme(window, "folia.light");
            Access::Bars(window, {false, WindowMaterial::MicaAlt, WindowMaterial::Solid});
            auto paper = root.FindName(L"ReadingTint").as<Border>().Background();
            expect(root.FindName(L"SelectedTabTint").as<Border>().Background() == paper);
            expect(root.FindName(L"RightTabTint").as<Border>().Background() == paper);
            auto invoke = [](Button const& button) {
                Automation::Peers::ButtonAutomationPeer(button).as<Automation::Provider::IInvokeProvider>().Invoke();
            };
            Access::New(window);
            invoke(root.FindName(L"SplitEditorButton").as<Button>());
            auto left = Access::GroupTabs(window, EditorGroup::Left);
            auto right = Access::GroupTabs(window, EditorGroup::Right);
            auto rightAdd = [&] { return right.as<IControlProtected>().GetTemplateChild(L"AddButton").try_as<Button>(); };
            Pump([&] { return rightAdd() && rightAdd().IsLoaded(); });
            auto& groups = Access::Groups(window);
            for (int i = 0; i < 2; ++i) invoke(rightAdd());
            auto check = [&](TabView const& strip, std::wstring_view group) {
                root.UpdateLayout();
                if (groups.IsSplit()) expect(!strip.CanDragTabs() && !strip.CanReorderTabs() && !strip.AllowDrop());
                for (std::uint32_t i = 0; i < strip.TabItems().Size(); ++i) {
                    const auto description = winrt::to_string(std::format(L"{} tab {}", group, i));
                    auto tab = strip.ContainerFromIndex(i).try_as<TabViewItem>();
                    expect(fatal(static_cast<bool>(tab))) << description + " loaded container";
                    auto drag = TabPart(tab, L"TabDragVisualContainer").as<Grid>();
                    expect(drag.Background().as<Media::SolidColorBrush>().Color().A == 0) << description + " drag background";
                    auto shape = TabPart(tab, L"SelectedBackgroundPath").try_as<Shapes::Path>();
                    if (strip.SelectedIndex() == static_cast<int>(i)) {
                        expect(shape && shape.Visibility() == Visibility::Visible && shape.Fill().as<Media::SolidColorBrush>().Color().A == 0)
                            << description + " selected background";
                    } else {
                        expect(!shape || shape.Visibility() == Visibility::Collapsed) << description + " inactive background";
                        const auto background = TabPart(tab, L"TabContainer").as<Grid>().Background().try_as<Media::SolidColorBrush>();
                        if (!winrt::Windows::UI::ViewManagement::AccessibilitySettings().HighContrast())
                            expect(!background || background.Color().A == 0) << description + " idle fill";
                    }
                }
            };
            check(left, L"left before transfer");
            check(right, L"right before transfer");
            expect(Access::MoveView(window, groups.Selected(EditorGroup::Right), EditorGroup::Left, 0));
            check(left, L"left after transfer");
            check(right, L"right after transfer");
            while (auto view = groups.Selected(EditorGroup::Right))
                expect(fatal(Access::MoveView(window, view, EditorGroup::Left, 0)));
            expect(!groups.IsSplit());
            check(left, L"left after closing split");
        };
        "dropped storage files open in the hit editor group and stay pinned"_test = [] {
            using winrt::Folia::EditorGroup;
            using namespace winrt::Windows::Storage;
            using namespace winrt::Windows::ApplicationModel::DataTransfer;
            TestDirectory files;
            auto folder = Await(StorageFolder::GetFolderFromPathAsync(files.path.native()));
            std::filesystem::path base(folder.Path().c_str());
            OwnedWindow workspace(App()->CreateMainWindow());
            auto window = workspace.window;
            auto root = window.Content().as<FrameworkElement>();
            expect(root.AllowDrop());
            auto& documents = Access::Documents(window);
            auto& groups = Access::Groups(window);
            auto drop = [&](float x, std::initializer_list<wchar_t const*> names) {
                std::vector<IStorageItem> items;
                for (auto name : names) {
                    auto path = base / name;
                    if (!std::filesystem::exists(path)) std::ofstream(path) << "A document";
                    items.push_back(Await(StorageFile::GetFileFromPathAsync(path.native())));
                }
                DataPackage package;
                package.SetStorageItems(winrt::single_threaded_vector<IStorageItem>(std::move(items)));
                Await(Access::DropItems(window, {x, 100.f}, package.GetView()));
            };
            drop(100, {L"first.md"});
            auto first = documents.Active();
            expect(fatal(first->kept && first->path == base / L"first.md"));
            expect(documents.Find(files.path / L"first.md") == first);
            auto button = root.FindName(L"SplitEditorButton").as<Controls::Button>();
            Automation::Peers::ButtonAutomationPeer(button).as<Automation::Provider::IInvokeProvider>().Invoke();
            Pump([&] { return groups.IsSplit(); });
            Access::ResizeGroups(window, 70.);
            root.UpdateLayout();
            auto divider = root.FindName(L"EditorGroupSplitter").as<FrameworkElement>();
            const auto edge = divider.TransformToVisual(root).TransformPoint({0, 0}).X;
            drop(edge - 12, {L"left.md", L"ignored.bin", L"second.txt"});
            auto left = documents.Active();
            expect(left->kept && groups.ActiveGroup() == EditorGroup::Left);
            expect(fatal(left->path == base / L"second.txt"));
            expect(documents.Find(base / L"ignored.bin") == nullptr);
            drop(edge + 12, {L"right.md", L"left.md", L"left.md"});
            auto both = documents.Active();
            expect(fatal(both->kept && both->path == base / L"left.md"));
            expect(groups.ActiveGroup() == EditorGroup::Right);
            expect(groups.ViewCount(both->id) == 2_u);
            expect(groups.ViewFor(EditorGroup::Left, left->id) != 0_u);
            auto right = documents.Find(base / L"right.md");
            expect(fatal(right != nullptr));
            expect(groups.ViewFor(EditorGroup::Right, right->id) != 0_u);
            auto previewPath = base / L"preview.md";
            std::ofstream(previewPath) << "Preview";
            Access::Open(window, previewPath, winrt::Folia::WorkspaceOpenMode::Preview);
            auto externalPath = base / L"external.md";
            std::ofstream(externalPath) << "External";
            Access::ExternalFile(externalPath);
            expect(documents.Active()->path == externalPath && documents.Active()->kept);
            expect(groups.ActiveGroup() == EditorGroup::Right && documents.Find(previewPath) != nullptr);
            Access::ExternalFile(left->path);
            expect(groups.ActiveGroup() == EditorGroup::Left && documents.Active() == left);
        };

        "split groups move tab identity and collapse either empty group"_test = [] {
            using winrt::Folia::EditorGroup;
            OwnedWindow workspace(App()->CreateMainWindow());
            auto window = workspace.window;
            auto root = window.Content().as<FrameworkElement>();
            auto& groups = Access::Groups(window);
            auto first = Access::Documents(window).Active();
            Access::New(window);
            auto second = Access::Documents(window).Active();
            auto button = root.FindName(L"SplitEditorButton").as<Controls::Button>();
            Automation::Peers::ButtonAutomationPeer(button).as<Automation::Provider::IInvokeProvider>().Invoke();
            Pump([&] { return groups.IsSplit(); });
            auto right = groups.Selected(EditorGroup::Right);
            auto item = Access::Binding(window, first->id)->item;
            expect(Access::MoveView(window, first->id, EditorGroup::Right, 1));
            expect(Access::GroupTabs(window, EditorGroup::Right).SelectedItem() == item);
            expect(Access::Documents(window).Active() == first);
            expect(Access::MoveView(window, first->id, EditorGroup::Right, 0));
            expect(groups.Views(EditorGroup::Right).front().document == first->id);
            expect(Access::MoveView(window, first->id, EditorGroup::Right, 2));
            expect(Access::MoveView(window, second->id, EditorGroup::Right, 0));
            expect(!groups.IsSplit());
            expect(groups.Selected(EditorGroup::Left) == second->id);
            expect(groups.ViewCount(second->id) == 2_u && groups.Find(right));
            expect(Access::Items(window).Size() == 3_u);
            expect(Access::Items(window).GetAt(2) == item);
            expect(Access::Documents(window).Items().size() == 2_u);

            Automation::Peers::ButtonAutomationPeer(button).as<Automation::Provider::IInvokeProvider>().Invoke();
            Pump([&] { return groups.IsSplit(); });
            auto newRight = groups.Selected(EditorGroup::Right);
            expect(Access::MoveView(window, newRight, EditorGroup::Left, 0));
            expect(!groups.IsSplit());
            expect(groups.Selected(EditorGroup::Left) == newRight && groups.ViewCount(second->id) == 3_u);
            expect(Access::Documents(window).Active() == second);
        };
        "split groups retain settings in its visible editor group"_test = [] {
            using winrt::Folia::EditorGroup;
            OwnedWindow workspace(App()->CreateMainWindow());
            auto window = workspace.window;
            auto root = window.Content().as<FrameworkElement>();
            auto& groups = Access::Groups(window);
            auto button = root.FindName(L"SplitEditorButton").as<Controls::Button>();
            Automation::Peers::ButtonAutomationPeer(button).as<Automation::Provider::IInvokeProvider>().Invoke();
            Pump([&] { return groups.IsSplit(); });
            auto fallback = groups.Selected(EditorGroup::Right);
            Access::New(window);
            auto previous = groups.Selected(EditorGroup::Right);
            auto settings = Access::About(window);
            auto settingsView = groups.Selected(EditorGroup::Right);
            Access::SelectView(window, groups.Selected(EditorGroup::Left));
            auto peer = root.FindName(L"OtherDocumentViewport");
            Pump([&] { return settings.Parent() == peer && settings.Visibility() == Visibility::Visible; });
            expect(!settings.IsHitTestVisible());
            Access::SelectView(window, settingsView);
            expect(settings.Parent() == root.FindName(L"WorkspaceHost") && settings.IsHitTestVisible());
            Access::SelectView(window, previous);
            Access::SelectView(window, settingsView);
            Access::CloseView(window, previous);
            Pump([&] { return !groups.Find(previous); });
            Access::ReturnToDocumentTab(window);
            expect(groups.ActiveGroup() == EditorGroup::Right);
            expect(groups.Selected(EditorGroup::Right) == fallback);
        };
        "document tools reveal over a stationary reading surface without moving the tabs"_test = [] {
            OwnedWindow current(App()->CreateMainWindow());
            auto window = current.window;
            auto root = window.Content().as<FrameworkElement>();
            auto toolbar = root.FindName(L"DocumentToolbar").as<FrameworkElement>();
            auto card = root.FindName(L"ReadingCard").as<FrameworkElement>();
            auto top = root.FindName(L"TopChrome").as<FrameworkElement>();
            auto view = root.FindName(L"DocumentViewport").as<FrameworkElement>();
            root.FindName(L"EditorSurface").as<UIElement>().Focus(FocusState::Programmatic);
            Access::Bars(window, {true});
            root.UpdateLayout();
            auto origin = view.TransformToVisual(card).TransformPoint({0,0});
            expect(toolbar.Opacity() == 0. && !toolbar.IsHitTestVisible()) << "auto-hide starts with inactive tools";
            Access::Hover(window, {100,4});
            Pump([&] { return toolbar.Opacity() == 1.; });
            expect(toolbar.IsHitTestVisible()) << "revealed tools receive input";
            expect(view.TransformToVisual(card).TransformPoint({0,0}) == origin) << "hover does not move the viewport";
            expect(top.Visibility() == Visibility::Visible) << "the title strip stays visible";
            expect(view.Clip().as<Media::RectangleGeometry>().Transform().as<Media::TranslateTransform>().Y() > 0.)
                << "the document does not paint under the visible tools";
            Access::Hover(window, {100,160});
            Pump([&] { return toolbar.Opacity() == 0. && !toolbar.IsHitTestVisible(); });
            expect(!toolbar.IsHitTestVisible()) << "hidden tools leave input to the document";
            Access::Bars(window, {});
            root.UpdateLayout();
            expect(toolbar.Opacity() == 1.) << "pinned tools are visible";
            Pump([&] { return std::abs(view.TransformToVisual(card).TransformPoint({0,0}).Y - toolbar.ActualHeight()) <= 1.; });
        };
        "sidebar opens inward with one editor resize per transition"_test = [] {
            OwnedWindow current(App()->CreateMainWindow());
            auto window = current.window;
            auto root = window.Content().as<FrameworkElement>();
            auto pane = root.FindName(L"WorkspaceSidebar").as<FrameworkElement>();
            auto card = root.FindName(L"ReadingCard").as<FrameworkElement>();
            auto rail = root.FindName(L"WorkspaceRail").as<FrameworkElement>();
            auto editor = root.FindName(L"EditorSurface").as<FrameworkElement>();
            auto appWindow = window.AppWindow();
            auto presenter = appWindow.Presenter().as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>();

            for (bool resizable : {true, false}) {
                presenter.IsResizable(resizable);
                root.UpdateLayout();
                const auto position = appWindow.Position();
                const auto size = appWindow.Size();
                const auto width = card.ActualWidth();
                const auto left = card.TransformToVisual(root).TransformPoint({0, 0}).X;
                auto expanded = [&] {
                    return pane.IsHitTestVisible() && std::abs(card.ActualWidth() + pane.Width() - width) < 1.
                        && std::abs(card.TransformToVisual(root).TransformPoint({0, 0}).X
                            - left - pane.Width()) < 1.;
                };
                auto collapsed = [&] {
                    return pane.Visibility() == Visibility::Collapsed && std::abs(card.ActualWidth() - width) < 1.
                        && std::abs(card.TransformToVisual(root).TransformPoint({0, 0}).X - left) < 1.;
                };
                auto expectStationary = [&] {
                    expect(appWindow.Position() == position && appWindow.Size() == size);
                };
                std::size_t layoutChanges = 0;
                auto resized = editor.SizeChanged(winrt::auto_revoke, [&](auto const&, auto const& args) {
                    if (args.NewSize().Width != args.PreviousSize().Width) ++layoutChanges;
                });

                Access::Sidebar(window, true);
                Pump(expanded);
                expectStationary();
                expect(layoutChanges == 1_u);
                expect(rail.TransformToVisual(root).TransformPoint({0, 0}).X == 0.f);
                expect(rail.ActualHeight() == root.ActualHeight());

                Access::Sidebar(window, false);
                Pump(collapsed);
                expectStationary();
                expect(layoutChanges == 2_u);

                Access::Sidebar(window, true);
                Access::Sidebar(window, false);
                Access::Sidebar(window, true);
                Pump(expanded);
                expect(pane.IsHitTestVisible());
                expectStationary();
                Access::Sidebar(window, false);
                Pump(collapsed);
            }
        };
        "sidebar compositor motion keeps the right edge of the reading surface filled"_test = [] {
            using winrt::Folia::WindowMaterial;
            using winrt::Windows::Foundation::Point;
            using winrt::Windows::UI::ViewManagement::AccessibilitySettings;
            using winrt::Windows::UI::ViewManagement::UISettings;
            if (!UISettings().AnimationsEnabled() || AccessibilitySettings().HighContrast()) return;
            OwnedWindow current(App()->CreateMainWindow());
            auto window = current.window;
            window.AppWindow().Presenter().as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>().IsAlwaysOnTop(true);
            Access::Theme(window, "folia.light");
            Access::Bars(window, {false, WindowMaterial::Solid, WindowMaterial::Solid});
            auto root = window.Content().as<FrameworkElement>();
            auto card = root.FindName(L"ReadingCard").as<FrameworkElement>();
            auto pane = root.FindName(L"WorkspaceSidebar").as<FrameworkElement>();
            root.UpdateLayout();
            auto origin = card.TransformToVisual(root).TransformPoint({0, 0});
            auto convert = root.XamlRoot().CoordinateConverter();
            const float y = origin.Y + static_cast<float>(card.ActualHeight()) * .60f;
            const std::array points{
                convert.ConvertLocalToScreen(Point{25.f, y}),
                convert.ConvertLocalToScreen(Point{origin.X + 24.f, y}),
                convert.ConvertLocalToScreen(Point{origin.X + static_cast<float>(pane.Width()) - 20.f, y}),
                convert.ConvertLocalToScreen(Point{origin.X + static_cast<float>(card.ActualWidth()) - 24.f, y}),
            };
            std::vector<std::array<int, 3>> initial;
            const auto waitForFrame = std::chrono::steady_clock::now() + std::chrono::milliseconds(400);
            Pump([&] { return std::chrono::steady_clock::now() >= waitForFrame; });
            initial = CaptureColors(window, points);
            expect(fatal(initial[0] != initial[3] && initial[1] == initial[3] && initial[2] == initial[3]))
                << std::format("base / divider / pane / paper: {}", initial);
            Access::Sidebar(window, true);
            bool hasIntermediateFrame = false;
            bool rightEdgeStayedFilled = true;
            Pump([&] {
                auto colors = CaptureColors(window, points);
                hasIntermediateFrame |= colors[1] == initial[0] && colors[2] == initial[3];
                rightEdgeStayedFilled &= colors[3] == initial[3];
                return pane.IsHitTestVisible() && colors[2] == initial[0];
            });
            expect(hasIntermediateFrame);
            expect(rightEdgeStayedFilled);
        };
        "overflowing tabs scroll by wheel and leave the native caption outside the viewport"_test = [] {
            OwnedWindow current(App()->CreateMainWindow());
            auto window = current.window;
            auto root = window.Content().as<FrameworkElement>();
            auto viewport = root.FindName(L"TabViewport").as<FrameworkElement>();
            auto caption = root.FindName(L"CaptionInset").as<FrameworkElement>();
            auto tabs = Access::Tabs(window);
            auto add = Access::NewTabButton(window);
            auto scale = root.XamlRoot().RasterizationScale();
            auto resize = [&](int width, auto settled) {
                auto size = window.AppWindow().Size();
                size.Width = static_cast<int>(width * scale);
                window.AppWindow().Resize(size);
                Pump([&] { return settled(root.ActualWidth()); });
            };
            auto viewer = Access::TabScroller(window);
            for (int i = 0; i < 3; ++i) Access::New(window);
            auto first = Access::View(window, Access::Documents(window).Items().front()->id);
            auto addX = [&] { return add.TransformToVisual(root).TransformPoint({0, 0}).X; };
            resize(740, [](double width) { return width > 705. && width < 760.; });
            const auto narrowTab = first.ActualWidth();
            const auto narrowAdd = addX();
            resize(1120, [](double width) { return width > 1080.; });
            Pump([&] { return first.ActualWidth() > narrowTab + 35. && addX() > narrowAdd + 140.; });
            resize(740, [](double width) { return width > 705. && width < 760.; });
            Pump([&] { return std::abs(first.ActualWidth() - narrowTab) <= 1. && std::abs(addX() - narrowAdd) <= 2.; });
            for (int i = 0; i < 8; ++i) Access::New(window);
            resize(640, [](double width) { return width < 670.; });
            Pump([&] { return viewer.ScrollableWidth() > 0 && viewer.HorizontalOffset() > 0; });
            auto initial = viewer.HorizontalOffset();
            expect(Access::ScrollTabs(window, WHEEL_DELTA));
            Pump([&] { return viewer.HorizontalOffset() < initial; });
            auto inset = caption.TransformToVisual(root).TransformPoint({0, 0}).X;
            auto right = viewport.TransformToVisual(root).TransformPoint({static_cast<float>(viewport.ActualWidth()), 0}).X;
            expect(std::abs(right - inset) <= 1.) << "the strip ends before the caption buttons";
            expect(addX() + add.ActualWidth() <= inset) << "add remains visible next to the overflowed tabs";
            const auto oldWidth = viewport.ActualWidth();
            const auto overflowAdd = addX();
            resize(840, [](double width) { return width > 800.; });
            Pump([&] { return addX() > overflowAdd + 175.; });
            expect(viewport.ActualWidth() > oldWidth + 180.);
            expect(tabs.ActualWidth() <= viewport.ActualWidth() + 1.);
        };
        "real wheel bursts reverse immediately at either end of a tab strip"_test = [] {
            using winrt::Windows::Foundation::Point;
            OwnedWindow owned(App()->CreateMainWindow());
            auto window = owned.window;
            auto root = window.Content().as<FrameworkElement>();
            const auto scale = root.XamlRoot().RasterizationScale();
            window.AppWindow().Resize({static_cast<int>(720 * scale), static_cast<int>(480 * scale)});
            for (int i = 0; i < 22; ++i) Access::New(window);
            auto viewer = Access::TabScroller(window);
            Pump([&] { return viewer.ScrollableWidth() > viewer.ViewportWidth() * 2; });
            auto presenter = viewer.as<Controls::IControlProtected>().GetTemplateChild(L"ScrollContentPresenter").as<FrameworkElement>();
            app_tests::MouseInput input(window);
            input.Move(presenter, Point{static_cast<float>(presenter.ActualWidth() / 2.), 16});
            auto settle = [] {
                const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(650);
                Pump([&] { return std::chrono::steady_clock::now() >= until; });
            };
            auto wheel = [&](int delta, int count) {
                input.Wheel(delta, count);
                settle();
                return viewer.HorizontalOffset();
            };
            settle();
            auto selection = Access::Tabs(window).SelectedItem();
            const auto end = viewer.ScrollableWidth();
            const auto step = viewer.ViewportWidth() / 3.;
            for (const auto direction : {1, -1}) {
                const auto edge = direction > 0 ? 0. : end;
                viewer.ChangeView(edge, nullptr, nullptr, true);
                settle();
                auto blocked = wheel(direction * WHEEL_DELTA, 3);
                auto one = wheel(-direction * WHEEL_DELTA, 1);
                auto two = wheel(-direction * WHEEL_DELTA, 1);
                auto burst = wheel(-direction * WHEEL_DELTA, 2);
                auto trace = std::format("edge={} offsets={},{},{},{}", edge, blocked, one, two, burst);
                expect(std::abs(blocked - edge) <= 1.) << trace;
                expect(std::abs(one - (edge + direction * step)) <= 2.) << trace;
                expect((two - one) * direction >= step - 2.) << trace;
                expect((burst - two) * direction >= 2 * step - 2.) << trace;
            }
            expect(Access::Tabs(window).SelectedItem() == selection);
        };
        "overflowing right tabs track arrow clicks and scroll boundaries"_test = [] {
            using winrt::Folia::EditorGroup;
            using namespace Controls;
            OwnedWindow current(App()->CreateMainWindow());
            auto window = current.window;
            auto root = window.Content().as<FrameworkElement>();
            auto invokeButton = [](Button const& button) {
                Automation::Peers::ButtonAutomationPeer(button).as<Automation::Provider::IInvokeProvider>().Invoke();
            };
            invokeButton(root.FindName(L"SplitEditorButton").as<Button>());
            auto rightTabs = Access::GroupTabs(window, EditorGroup::Right);
            auto add = [&] { return rightTabs.as<IControlProtected>().GetTemplateChild(L"AddButton").try_as<Button>(); };
            Pump([&] { return add() && add().IsLoaded(); });
            for (int i = 0; i < 8; ++i) invokeButton(add());

            auto viewer = Access::TabScroller(window, EditorGroup::Right);
            auto parts = viewer.as<IControlProtected>();
            auto previous = parts.GetTemplateChild(L"PreviousTabScrollArrow").as<Primitives::RepeatButton>();
            auto next = parts.GetTemplateChild(L"NextTabScrollArrow").as<Primitives::RepeatButton>();
            auto selected = rightTabs.SelectedItem();
            auto invokeArrow = [](Primitives::RepeatButton const& button) {
                Automation::Peers::RepeatButtonAutomationPeer(button).as<Automation::Provider::IInvokeProvider>().Invoke();
            };
            auto atStart = [&] { return viewer.HorizontalOffset() <= .1; };
            auto atEnd = [&] { return viewer.ScrollableWidth() - viewer.HorizontalOffset() <= .1; };
            Pump([&] { return viewer.ScrollableWidth() > 10.; });
            expect(Access::ScrollTabs(window, -60 * WHEEL_DELTA, EditorGroup::Right));
            Pump(atEnd);
            expect(!next.IsEnabled()) << "next is disabled at the end";
            expect(fatal(previous.IsEnabled())) << "previous is enabled at the end";
            const auto fromEnd = viewer.HorizontalOffset();
            invokeArrow(previous);
            Pump([&] { return viewer.HorizontalOffset() < fromEnd - .1; });
            expect(fatal(next.IsEnabled())) << "scrolling back re-enables next";
            const auto beforeNext = viewer.HorizontalOffset();
            invokeArrow(next);
            Pump([&] { return viewer.HorizontalOffset() > beforeNext + .1; });
            expect(Access::ScrollTabs(window, 60 * WHEEL_DELTA, EditorGroup::Right));
            Pump(atStart);
            expect(!previous.IsEnabled() && next.IsEnabled());
            expect(Access::ScrollTabs(window, -60 * WHEEL_DELTA, EditorGroup::Right));
            Pump(atEnd);
            expect(previous.IsEnabled() && !next.IsEnabled());
            expect(rightTabs.SelectedItem() == selected);
        };
        "clicking a disk document tab leaves the tab strip responsive to resizing"_test = [] {
            TestDirectory directory;
            auto file = directory.path / L"resize.md";
            std::ofstream(file) << "# Markdown from disk\n";
            OwnedWindow current(App()->CreateMainWindow());
            auto window = current.window;
            Access::SetRecentHistory(window, directory.path / L"recent.json");
            Access::Open(window, file);
            auto loaded = Access::Documents(window).Active();
            Pump([&] { return loaded->editor->loaded && !loaded->editor->opening; });
            for (int i = 0; i < 3; ++i) Access::New(window);

            auto root = window.Content().as<FrameworkElement>();
            auto tabs = Access::Tabs(window);
            auto first = Access::View(window, loaded->id);
            auto add = Access::NewTabButton(window);
            auto center = first.TransformToVisual(nullptr).TransformPoint(
                {static_cast<float>(first.ActualWidth() / 2), static_cast<float>(first.ActualHeight() / 2)});
            auto point = root.XamlRoot().CoordinateConverter().ConvertLocalToScreen(center);
            int requests = 0;
            auto observe = tabs.TabTearOutWindowRequested(winrt::auto_revoke, [&](auto&&, auto&&) { ++requests; });
            ::SendMessageW(Access::Handle(window), WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.X, point.Y));
            expect(fatal(Access::Documents(window).Active() == loaded));
            expect(requests == 0);

            auto width = first.ActualWidth();
            auto addX = [&] { return add.TransformToVisual(root).TransformPoint({0, 0}).X; };
            auto initialAddX = addX();
            auto size = window.AppWindow().Size();
            size.Width = static_cast<int>(740 * root.XamlRoot().RasterizationScale());
            window.AppWindow().Resize(size);
            Pump([&] { return first.ActualWidth() < width - 35. && addX() < initialAddX - 140.; });
        };
        "visible close buttons keep client input after switching tabs"_test = [] {
            using namespace Controls;
            using winrt::Microsoft::UI::Input::InputNonClientPointerSource;
            using winrt::Microsoft::UI::Input::NonClientRegionKind;
            using winrt::Microsoft::UI::Content::ContentCoordinateConverter;
            OwnedWindow owned(App()->CreateMainWindow());
            auto window = owned.window;
            auto& documents = Access::Documents(window);
            auto first = documents.Active();
            Access::New(window);
            Access::New(window);
            auto last = documents.Active();
            auto root = window.Content().as<FrameworkElement>();
            auto screen = root.XamlRoot().CoordinateConverter();
            auto native = ContentCoordinateConverter::CreateForWindowId(window.AppWindow().Id());
            auto pointer = InputNonClientPointerSource::GetForWindowId(window.AppWindow().Id());
            auto center = [](FrameworkElement const& element) {
                return element.TransformToVisual(nullptr).TransformPoint(
                    {static_cast<float>(element.ActualWidth() / 2), static_cast<float>(element.ActualHeight() / 2)});
            };
            auto point = screen.ConvertLocalToScreen(center(Access::View(window, first->id)));
            ::SendMessageW(Access::Handle(window), WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.X, point.Y));
            expect(fatal(documents.Active() == first));

            auto closeInactive = [&](std::uint64_t id) {
                auto tab = Access::View(window, id);
                expect(VisualStateManager::GoToState(tab, L"CloseButtonVisible", false));
                auto button = tab.as<IControlProtected>().GetTemplateChild(L"CloseButton").as<Button>();
                Pump([&] {
                    auto at = native.ConvertScreenToLocal(screen.ConvertLocalToScreen(center(button)));
                    return button.ActualWidth() > 0 && std::ranges::any_of(pointer.GetRegionRects(NonClientRegionKind::Passthrough), [&](auto rect) {
                        return at.X >= rect.X && at.X < rect.X + rect.Width
                            && at.Y >= rect.Y && at.Y < rect.Y + rect.Height;
                    });
                });
                Automation::Peers::ButtonAutomationPeer(button).as<Automation::Provider::IInvokeProvider>().Invoke();
                Pump([&] { return !documents.Find(id); });
                expect(documents.Active() == first);
            };
            closeInactive(last->id);
            closeInactive(documents.Items().back()->id);
            expect(documents.Items().size() == 1_u);
        };
        "open dropdowns and document menus use the current theme surface"_test = [] {
            using namespace Controls;
            OwnedWindow current(App()->CreateMainWindow());
            auto window = current.window;
            auto root = window.Content().as<FrameworkElement>();
            auto host = Access::ThemeSettings(window);
            ComboBox choice{nullptr};
            Pump([&] { root.UpdateLayout(); choice = FirstControl<ComboBox>(host); return choice && choice.ActualWidth() > 0; });
            auto surface = root.Resources().Lookup(winrt::box_value(L"FoliaShellLayerBrush")).as<Media::SolidColorBrush>();
            for (auto theme : {"folia.dark", "folia.light"}) {
                Access::Theme(window, theme);
                root.UpdateLayout();
                choice.IsDropDownOpen(true);
                auto border = choice.as<IControlProtected>().GetTemplateChild(L"PopupBorder").as<Border>();
                expect(border.Background() == surface && surface.Color().A == 255) << theme << " combo-box background";
                choice.IsDropDownOpen(false);
            }
            Access::Theme(window, "folia.dark");
            Access::Tabs(window).SelectedIndex(0);
            root.UpdateLayout();
            auto menu = Access::OpenFileMenu(window);
            MenuFlyoutPresenter presenter{nullptr};
            Pump([&] {
                for (auto popup : Media::VisualTreeHelper::GetOpenPopupsForXamlRoot(root.XamlRoot())) {
                    presenter = FirstControl<MenuFlyoutPresenter>(popup.Child());
                    if (presenter) return true;
                }
                return false;
            });
            expect(presenter.Background() == surface) << "document-menu background";
            menu.Hide();
        };
        "the reading page keeps equal margins when scrolling and splitting"_test = [] {
            using RendererAccess = winrt::Folia::EditorRendererTestAccess;
            TestDirectory directory;
            OwnedWindow current(App()->CreateMainWindow());
            auto window = current.window;
            auto root = window.Content().as<FrameworkElement>();
            Access::SetRecentHistory(window, directory.path / L"recent.json");
            auto check = [&](auto const& renderer, wchar_t const* cardName, wchar_t const* surfaceName,
                             wchar_t const* barName, bool scrolls) {
                auto card = root.FindName(cardName).as<FrameworkElement>();
                auto surface = root.FindName(surfaceName).as<FrameworkElement>();
                auto bar = root.FindName(barName).as<FrameworkElement>();
                Pump([&] {
                    root.UpdateLayout();
                    return RendererAccess::Rule(renderer) && (bar.Visibility() == Visibility::Visible) == scrolls;
                });
                auto rule = *RendererAccess::Rule(renderer);
                auto x = surface.TransformToVisual(card).TransformPoint({0, 0}).X;
                auto left = x + rule.left;
                auto right = card.ActualWidth() - x - rule.right;
                expect(std::abs(left - right) <= 1. / root.XamlRoot().RasterizationScale())
                    << std::format("rule margins left={} right={} DIP", left, right);
            };
            for (bool scrolls : {false, true}) {
                auto file = directory.path / (scrolls ? L"long.md" : L"short.md");
                {
                    std::ofstream stream(file);
                    stream << "# Margins\n\n---\n\n";
                    for (int n = 0; n < (scrolls ? 200 : 1); ++n) stream << "One paragraph.\n\n";
                }
                Access::Open(window, file);
                auto doc = Access::Documents(window).Active();
                Pump([&] { return doc->editor->loaded && !doc->editor->opening; });
                check(Access::Renderer(window), L"ReadingCard", L"EditorSurface", L"EditorScrollBar", scrolls);
            }
            auto split = root.FindName(L"SplitEditorButton").as<Controls::Button>();
            Automation::Peers::ButtonAutomationPeer(split).as<Automation::Provider::IInvokeProvider>().Invoke();
            Pump([&] { return Access::OtherRenderer(window) != nullptr; });
            check(Access::Renderer(window), L"ReadingCard", L"EditorSurface", L"EditorScrollBar", true);
            check(*Access::OtherRenderer(window), L"OtherReadingCard", L"OtherEditorSurface", L"OtherEditorScrollBar", true);
        };
        "the document renderer presents individual device pixels at native resolution"_test = [] {
            OwnedWindow owned(App()->CreateMainWindow());
            auto window = owned.window;
            window.AppWindow().Presenter().as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>().IsAlwaysOnTop(true);
            auto root = window.Content().as<FrameworkElement>();
            auto editor = root.FindName(L"EditorSurface").as<FrameworkElement>();
            auto& r = winrt::Folia::EditorRendererTestAccess::Resources(Access::Renderer(window));
            Access::Edit(window, U"Native resolution");
            auto settle = std::chrono::steady_clock::now() + std::chrono::milliseconds(400);
            Pump([&] { return std::chrono::steady_clock::now() >= settle && r.renderTexture; });
            std::ofstream report(app_tests::RepositoryRoot() / "build/tests/document-device-pixels.txt");
            auto verify = [&] {
                auto converter = root.XamlRoot().CoordinateConverter();
                auto screen = [&](float x, float y) {
                    return converter.ConvertLocalToScreen(editor.TransformToVisual(root).TransformPoint({x, y}));
                };
                auto origin = screen(0, 0);
                auto end = screen(static_cast<float>(editor.ActualWidth()), static_cast<float>(editor.ActualHeight()));
                auto pixels = r.d2dTarget->GetPixelSize();
                auto surface = r.drawingSurface.Size();
                auto dpi = ::GetDpiForWindow(Access::Handle(window));
                auto scale = root.XamlRoot().RasterizationScale();
                report << std::format("DPI={} XamlRoot={} editor={}x{} DIP screen={}x{} px D2D={}x{} composition={}x{}\n",
                    dpi, scale, editor.ActualWidth(), editor.ActualHeight(), end.X - origin.X, end.Y - origin.Y,
                    pixels.width, pixels.height, surface.Width, surface.Height);
                expect(std::abs(scale - dpi / 96.) < .001);
                expect(std::abs(static_cast<int>(pixels.width) - (end.X - origin.X)) <= 1);
                expect(std::abs(static_cast<int>(pixels.height) - (end.Y - origin.Y)) <= 1);
                expect(surface.Width == pixels.width && surface.Height == pixels.height);

                // Compare a line of actual DirectWrite glyphs before and after composition.
                // The editor draws premultiplied pixels over the window material.
                auto active = Access::Documents(window).Active()->editor->session.Selection().active;
                active.source_offset = static_cast<std::uint32_t>(std::u32string_view(U"Native resolution").size());
                auto finish = Access::Renderer(window).CaretBounds(active).value();
                active.source_offset = 0;
                auto start = Access::Renderer(window).CaretBounds(active).value();
                const auto left = static_cast<unsigned>(std::floor(start.left * scale));
                const auto top = static_cast<unsigned>(std::floor(start.top * scale));
                const auto width = static_cast<unsigned>(std::floor(finish.left * scale)) - left;
                const auto height = static_cast<unsigned>(std::ceil(finish.bottom * scale)) - top;
                expect(fatal(width > 8 && height > 8));
                auto sample = app_tests::MeasurePresentedGlyphs(r, Access::Handle(window), origin, left, top, width, height);
                report << std::format("Glyph samples={} mean error={:.2f} outside tolerance={}\n",
                    sample.samples, sample.mean, sample.outsideTolerance);
                expect(fatal(sample.samples > 100));
                expect(sample.mean < 5.) << "native glyph edges survive desktop composition";
            };
            verify();
            // A composited layer can move between device pixels while a pane is animating.
            // Exercise that position deterministically without racing the desktop frame clock.
            Media::TranslateTransform translation;
            translation.X(.4 / root.XamlRoot().RasterizationScale());
            editor.RenderTransform(translation);
            auto settleLayout = [&] {
                root.UpdateLayout();
                auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(220);
                Pump([&] { return std::chrono::steady_clock::now() >= until; });
            };
            settleLayout();
            verify();
            editor.RenderTransform(nullptr);
            auto size = window.AppWindow().Size();
            window.AppWindow().Resize({size.Width - 53, size.Height - 21});
            settleLayout();
            verify();
            auto split = root.FindName(L"SplitEditorButton").as<Controls::Button>();
            Automation::Peers::ButtonAutomationPeer(split).as<Automation::Provider::IInvokeProvider>().Invoke();
            settleLayout();
            expect(fatal(Access::Groups(window).IsSplit()));
            verify();
        };
        "persisted reading material joins the first selected tab at startup"_test = [] {
            using namespace winrt::Folia;
            using winrt::Windows::Foundation::Point;
            auto original = LoadAppSettings().settings;
            auto restore = wil::scope_exit([&] { SaveAppSettings(original); });
            using enum WindowMaterial;
            for (const WorkspaceAppearance appearance : {
                    WorkspaceAppearance{false, Mica, Mica}, {false, Mica, Acrylic}, {false, Mica, Solid},
                    {false, Acrylic, Acrylic}, {false, Acrylic, std::nullopt}}) {
                auto startup = original;
                startup.appearance = appearance;
                expect(fatal(!SaveAppSettings(startup)));
                auto window = App()->CreateMainWindow();
                auto close = wil::scope_exit([&] { Access::Close(window); });
                window.AppWindow().Presenter().as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>().IsAlwaysOnTop(true);
                window.Activate();
                auto root = window.Content().as<FrameworkElement>();
                auto read = [&](EditorGroup group = EditorGroup::Left, int tolerance = 5) {
                    auto tabs = Access::GroupTabs(window, group);
                    FrameworkElement tab{nullptr};
                    Pump([&] {
                        root.UpdateLayout();
                        tab = tabs.ContainerFromItem(tabs.SelectedItem()).try_as<FrameworkElement>();
                        return tab && tab.IsLoaded() && tab.ActualWidth() > 0;
                    });
                    const bool left = group == EditorGroup::Left;
                    const bool active = group == Access::Groups(window).ActiveGroup();
                    auto card = root.FindName(active ? L"ReadingCard" : L"OtherReadingCard").as<FrameworkElement>();
                    auto selected = root.FindName(left ? L"SelectedTabBackdropFrame" : L"RightTabBackdropFrame").as<FrameworkElement>();
                    auto tint = root.FindName(left ? L"SelectedTabTint" : L"RightTabTint").as<Controls::Border>();
                    auto paper = root.FindName(active ? L"ReadingTint" : L"OtherReadingTint").as<Controls::Border>();
                    auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(350);
                    Pump([&] { return std::chrono::steady_clock::now() >= until; });
                    auto at = card.TransformToVisual(root).TransformPoint({0,0});
                    auto position = tab.TransformToVisual(root).TransformPoint({static_cast<float>(tab.ActualWidth() * .72), 0});
                    auto edge = root.XamlRoot().CoordinateConverter().ConvertLocalToScreen(Point{position.X, at.Y});
                    std::vector<ScreenPoint> points;
                    for (int dy = -6; dy <= 6; ++dy) points.push_back({edge.X, edge.Y + dy});
                    auto colors = CaptureColors(window, points);
                    expect(tint.Background() == paper.Background()) << "selected tab and reading card share the live brush";
                    expect(selected.Visibility() == Visibility::Visible) << "first tab is materialized without changing settings";
                    auto origin = root.FindName(L"TabBackdropLayer").as<FrameworkElement>();
                    auto clip = Hosting::ElementCompositionPreview::GetElementVisual(selected).Clip()
                        .as<winrt::Microsoft::UI::Composition::RectangleClip>();
                    auto rightEdge = tab.TransformToVisual(origin).TransformPoint({static_cast<float>(tab.ActualWidth()), 0});
                    expect(std::abs(clip.Right() - rightEdge.X) < .51f);
                    expect(std::abs(clip.Bottom() - origin.ActualHeight()) < .01f);
                    for (auto const& pixel : colors) for (unsigned channel = 0; channel < 3; ++channel) {
                        expect(std::abs(pixel[channel] - colors.front()[channel]) <= tolerance)
                            << std::format("seam at startup (window {}, reading {}): {}", static_cast<int>(appearance.material),
                                static_cast<int>(appearance.readingMaterial.value_or(appearance.material)), colors);
                    }
                    return colors.front();
                };
                auto checkBaseTransmission = [&](EditorGroup group) {
                    // An opaque test substrate makes a visual that bypasses the shell
                    // detectable regardless of the desktop behind the window.
                    auto shade = root.as<Controls::Grid>().Background().as<Media::SolidColorBrush>();
                    const auto savedColor = shade.Color();
                    auto restoreShade = wil::scope_exit([&] { shade.Color(savedColor); });
                    shade.Color({255, 32, 64, 192});
                    auto cool = read(group, 1);
                    shade.Color({255, 192, 64, 32});
                    auto warm = read(group, 1);
                    expect(warm[0] - cool[0] > 32 && cool[2] - warm[2] > 32)
                        << "the selected tab and paper transmit the shared base";
                };
                read();
                const bool sharedAcrylic = appearance.material == Acrylic &&
                    winrt::Microsoft::UI::Composition::SystemBackdrops::DesktopAcrylicController::IsSupported() &&
                    !winrt::Windows::UI::ViewManagement::AccessibilitySettings().HighContrast();
                if (sharedAcrylic) {
                    expect(fatal(window.SystemBackdrop().try_as<Media::DesktopAcrylicBackdrop>() != nullptr));
                    checkBaseTransmission(EditorGroup::Left);
                }
                auto first = FirstTab(window, EditorGroup::Left);
                first.MaxWidth(89);
                read();
                Access::NewChat(window);
                read();
                auto changed = startup.appearance;
                changed.readingMaterial = appearance.readingMaterial == Solid ? Mica : Solid;
                Access::Bars(window, changed);
                Access::Bars(window, startup.appearance);
                read();

                if (sharedAcrylic) {
                    auto split = root.FindName(L"SplitEditorButton").as<Controls::Button>();
                    Automation::Peers::ButtonAutomationPeer(split).as<Automation::Provider::IInvokeProvider>().Invoke();
                    Pump([&] { return Access::Groups(window).IsSplit(); });
                    checkBaseTransmission(EditorGroup::Right);
                }
            }
        };
        "the selected tab toolbar and editor present one reading surface"_test = [] {
            using winrt::Folia::WindowMaterial;
            using winrt::Windows::Foundation::Point;
            using winrt::Microsoft::UI::Composition::SystemBackdrops::DesktopAcrylicController;
            using winrt::Microsoft::UI::Composition::SystemBackdrops::MicaController;
            OwnedWindow owned(App()->CreateMainWindow());
            auto window = owned.window;
            window.AppWindow().Presenter().as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>().IsAlwaysOnTop(true);
            auto root = window.Content().as<FrameworkElement>();
            Access::Theme(window, "folia.dark");
            auto editor = root.FindName(L"EditorSurface").as<FrameworkElement>();
            auto card = root.FindName(L"ReadingCard").as<FrameworkElement>();
            auto tabBackdrop = root.FindName(L"SelectedTabBackdrop").as<Controls::SystemBackdropElement>();
            auto readingBackdrop = root.FindName(L"ReadingBackdrop").as<Controls::SystemBackdropElement>();
            auto tab = Access::View(window, Access::Documents(window).Active()->id);
            Access::Edit(window, U"MMMM");
            auto document = Access::Documents(window).Active();
            auto end = document->editor->session.Selection().active;
            auto start = end;
            start.source_offset = 0;
            auto& renderer = Access::Renderer(window);
            auto textStart = renderer.CaretBounds(start);
            auto textEnd = renderer.CaretBounds(end);
            expect(fatal(textStart.has_value() && textEnd.has_value()));
            auto origin = [&](FrameworkElement const& element) {
                return element.TransformToVisual(root).TransformPoint({0, 0});
            };
            auto delta = [](auto const& a, auto const& b) {
                return (std::max)({std::abs(a[0] - b[0]), std::abs(a[1] - b[1]), std::abs(a[2] - b[2])});
            };
            auto supported = [](WindowMaterial value) {
                if (value == WindowMaterial::Solid) return true;
                return value == WindowMaterial::Acrylic ? DesktopAcrylicController::IsSupported() : MicaController::IsSupported();
            };
            auto verifySurface = [&](WindowMaterial base, WindowMaterial reading) {
                if (!supported(base) || !supported(reading)) return;
                Access::Bars(window, {false, base, reading});
                root.UpdateLayout();
                auto settle = std::chrono::steady_clock::now() + std::chrono::milliseconds(350);
                Pump([&] { return std::chrono::steady_clock::now() >= settle; });
                expect(tabBackdrop.SystemBackdrop() == readingBackdrop.SystemBackdrop());
                if (readingBackdrop.SystemBackdrop()) {
                    expect(tabBackdrop.ActualWidth() > 0. && tabBackdrop.ActualHeight() > 0.) << "the tab receives layout bounds";
                    auto placement = Hosting::ElementCompositionPreview::GetElementChildVisual(tabBackdrop);
                    auto size = placement ? placement.Size() : winrt::Windows::Foundation::Numerics::float2{};
                    expect(size.x > 0.f && size.y > 0.f) << std::format("tab backdrop visual occupies {} x {} pixels", size.x, size.y);
                }
                auto convert = root.XamlRoot().CoordinateConverter();
                float x = origin(tab).X + static_cast<float>(tab.ActualWidth() * .75);
                float tabEnd = origin(card).Y;
                float editorTop = origin(editor).Y;
                std::vector points{
                    convert.ConvertLocalToScreen(Point{x, tabEnd - 4}),
                    convert.ConvertLocalToScreen(Point{x, tabEnd + 4}),
                };
                for (auto fraction : {.15f, .50f, .87f, .95f}) {
                    auto sampleX = origin(card).X + static_cast<float>(card.ActualWidth()) * fraction;
                    points.push_back(convert.ConvertLocalToScreen(Point{sampleX, editorTop - 3}));
                    points.push_back(convert.ConvertLocalToScreen(Point{sampleX, editorTop + 3}));
                }
                auto firstTextSample = points.size();
                auto editorOrigin = origin(editor);
                for (float y = textStart->top; y < textStart->bottom; y += 2.f)
                    for (float dx = 0; dx < (textEnd->left - textStart->left) / 2; dx += 2.f)
                        points.push_back(convert.ConvertLocalToScreen(Point{
                            editorOrigin.X + textStart->left + dx, editorOrigin.Y + y}));
                auto colors = CaptureColors(window, points);
                expect(delta(colors[0], colors[1]) <= 5) << std::format("tab to toolbar: {} vs {}", colors[0], colors[1]);
                for (std::size_t i = 2; i < firstTextSample; i += 2)
                    expect(delta(colors[i], colors[i+1]) <= 5) << std::format("toolbar to editor: {} vs {}", colors[i], colors[i+1]);
                auto visible = std::ranges::count_if(std::span{colors}.subspan(firstTextSample), [&](auto const& pixel) {
                    return delta(pixel, colors[3]) >= 48;
                });
                expect(visible >= 6) << "text is visible within its expected DIP bounds";
            };
            for (auto theme : {"folia.dark", "folia.light"}) {
                Access::Theme(window, theme);
                verifySurface(WindowMaterial::Mica, WindowMaterial::Mica);
                verifySurface(WindowMaterial::Solid, WindowMaterial::Solid);
                verifySurface(WindowMaterial::Acrylic, WindowMaterial::Acrylic);
                verifySurface(WindowMaterial::Mica, WindowMaterial::Acrylic);
                verifySurface(WindowMaterial::MicaAlt, WindowMaterial::Acrylic);
                verifySurface(WindowMaterial::Acrylic, WindowMaterial::Mica);
                verifySurface(WindowMaterial::Solid, WindowMaterial::MicaAlt);
            }
        };
        "changing the reading backdrop leaves the inactive tabs and title strip on the window material"_test = [] {
            using namespace winrt::Microsoft::UI::Composition::SystemBackdrops;
            using winrt::Folia::WindowMaterial;
            using winrt::Windows::Foundation::Point;
            if (!MicaController::IsSupported() || !DesktopAcrylicController::IsSupported()
                || winrt::Windows::UI::ViewManagement::AccessibilitySettings().HighContrast()) return;
            OwnedWindow owned(App()->CreateMainWindow());
            auto window = owned.window;
            window.AppWindow().Presenter().as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>().IsAlwaysOnTop(true);
            Access::Theme(window, "folia.light");
            Access::Sidebar(window, false);
            auto inactiveId = Access::Documents(window).Active()->id;
            Access::New(window);
            auto inactive = Access::View(window, inactiveId);
            auto root = window.Content().as<FrameworkElement>();
            auto chrome = root.FindName(L"TopChrome").as<FrameworkElement>();
            auto sample = [&](WindowMaterial reading) {
                Access::Bars(window, {false, WindowMaterial::Mica, reading});
                auto settle = std::chrono::steady_clock::now() + std::chrono::milliseconds(350);
                Pump([&] { return std::chrono::steady_clock::now() >= settle; });
                auto convert = root.XamlRoot().CoordinateConverter();
                auto point = [&](FrameworkElement const& element, float x, float y) {
                    return convert.ConvertLocalToScreen(element.TransformToVisual(root).TransformPoint(Point{x, y}));
                };
                std::array points{
                    point(inactive, static_cast<float>(inactive.ActualWidth() * .75), 15.f),
                    point(chrome, static_cast<float>(chrome.ActualWidth() * .70), 15.f),
                };
                return CaptureColors(window, points);
            };
            auto solid = sample(WindowMaterial::Solid);
            auto acrylic = sample(WindowMaterial::Acrylic);
            for (std::size_t i = 0; i < solid.size(); ++i) {
                bool unchanged = solid[i] == acrylic[i];
                expect(unchanged) << std::format("base pixel with solid paper {} and acrylic paper {}", solid[i], acrylic[i]);
            }
        };
        "workspace material can leave the reading surface opaque independently of the window"_test = [] {
            using namespace winrt::Microsoft::UI::Composition::SystemBackdrops;
            using winrt::Folia::WindowMaterial;
            auto window = App()->CreateMainWindow();
            OwnedWindow owned(window);
            expect(Access::MaterialsReady(window)) << "both materials are attached once the root is loaded";
            auto root = window.Content().as<FrameworkElement>();
            auto readingBackdrop = root.FindName(L"ReadingBackdrop").as<Controls::SystemBackdropElement>();
            auto selectedBackdrop = root.FindName(L"SelectedTabBackdrop").as<Controls::SystemBackdropElement>();
            auto backdropFrame = root.FindName(L"SelectedTabBackdropFrame").as<FrameworkElement>();
            Access::Sidebar(window, false);
            auto tabs = Access::Tabs(window);
            auto first = tabs.SelectedItem();
            Access::New(window);
            auto second = tabs.SelectedItem();
            Pump([&] { return Access::View(window, Access::Documents(window).Active()->id).IsLoaded(); });
            auto selected = [&] {
                auto tint = root.FindName(L"SelectedTabTint").as<Controls::Border>();
                expect(tint.Background() == root.FindName(L"ReadingTint").as<Controls::Border>().Background())
                    << "the selected tab uses the reading surface tint";
            };
            bool highContrast = winrt::Windows::UI::ViewManagement::AccessibilitySettings().HighContrast();
            for (auto material : {WindowMaterial::Solid, WindowMaterial::Mica, WindowMaterial::MicaAlt, WindowMaterial::Acrylic}) {
                bool supported = material == WindowMaterial::Acrylic ? DesktopAcrylicController::IsSupported()
                    : material != WindowMaterial::Solid && MicaController::IsSupported();
                Access::Bars(window, {false, material, {}});
                selected();
                expect(readingBackdrop.SystemBackdrop() == nullptr);
                auto base = root.as<Controls::Grid>().Background();
                auto paper = root.FindName(L"ReadingTint").as<Controls::Border>().Background().as<Media::SolidColorBrush>();
                if (!supported || highContrast) {
                    expect(window.SystemBackdrop() == nullptr);
                    expect(paper.Opacity() == 1.);
                    expect(base.as<Media::SolidColorBrush>().Color() != paper.Color());
                    continue;
                }
                if (material == WindowMaterial::Acrylic)
                    expect(window.SystemBackdrop().try_as<Media::DesktopAcrylicBackdrop>() != nullptr);
                else expect(window.SystemBackdrop().as<Media::MicaBackdrop>().Kind()
                    == (material == WindowMaterial::Mica ? MicaKind::Base : MicaKind::BaseAlt));
                expect(paper.Opacity() < 1.);
                Access::Bars(window, {false, material, WindowMaterial::Solid});
                expect(window.SystemBackdrop() != nullptr && readingBackdrop.SystemBackdrop() == nullptr);
                expect(root.as<Controls::Grid>().Background() == base) << "reading material leaves the window's base shade unchanged";
                expect(paper.Opacity() == 1.) << "solid paper takes effect immediately";
                Access::Bars(window, {false, WindowMaterial::Solid, material});
                expect(window.SystemBackdrop() == nullptr);
                expect(readingBackdrop.SystemBackdrop() != nullptr);
                Pump([&] { return backdropFrame.Visibility() == Visibility::Visible && selectedBackdrop.ActualWidth() > 0; });
                expect(selectedBackdrop.SystemBackdrop() == readingBackdrop.SystemBackdrop());
                expect(paper.Opacity() < 1.);
                auto visual = Hosting::ElementCompositionPreview::GetElementVisual(backdropFrame);
                auto clip = visual.Clip().as<winrt::Microsoft::UI::Composition::RectangleClip>();
                auto old = clip.Left();
                tabs.SelectedItem(tabs.SelectedItem() == first ? second : first);
                Pump([&] { return clip.Left() != old; });
                selected();
            }
            Access::Bars(window, {true, WindowMaterial::Mica, WindowMaterial::Acrylic});
            auto reading = readingBackdrop.SystemBackdrop();
            auto base = window.SystemBackdrop();
            for (auto [open, distance] : {std::pair{false, 16.f}, {false, 200.f}, {true, 200.f}, {false, 16.f}}) {
                Access::Sidebar(window, open);
                Access::Hover(window, {600.f, distance});
                auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
                Pump([&] { return std::chrono::steady_clock::now() >= until; });
                expect(readingBackdrop.SystemBackdrop() == reading && window.SystemBackdrop() == base);
            }
        };
        "workspace notifications remain actionable between the revealed edge bars"_test = [] {
            using namespace winrt::Microsoft::UI::Xaml::Automation;
            TestDirectory directory;
            OwnedWindow owned(App()->CreateMainWindow());
            auto window = owned.window;
            auto root = window.Content().as<Controls::Grid>();
            Access::Bars(window, {true});
            auto error = root.FindName(L"ErrorInfoBar").as<Controls::InfoBar>();
            auto external = root.FindName(L"ExternalChangeInfoBar").as<Controls::InfoBar>();
            auto paper = root.FindName(L"WorkspaceHost").as<FrameworkElement>();
            root.UpdateLayout();
            auto initialHeight = paper.ActualHeight();
            Access::SetRecentHistory(window, directory.path / L"recent.json");
            Access::Open(window, directory.path / L"missing.md");
            Pump([&] { return error.IsOpen(); });
            expect(std::wstring_view(error.Message()).find(L"missing.md") != std::wstring_view::npos);
            external.IsOpen(true);
            root.UpdateLayout();
            expect(paper.ActualHeight() == initialHeight) << "showing notifications does not move the document";
            auto close = TabPart(error, L"CloseButton").as<Controls::Button>();
            auto reachable = [&](FrameworkElement const& button) {
                auto center = button.TransformToVisual(nullptr).TransformPoint({
                    static_cast<float>(button.ActualWidth() / 2), static_cast<float>(button.ActualHeight() / 2)});
                for (auto const& hit : Media::VisualTreeHelper::FindElementsInHostCoordinates(center, root)) {
                    for (DependencyObject p = hit; p; p = Media::VisualTreeHelper::GetParent(p))
                        if (p == button) return true;
                    break;
                }
                return false;
            };
            Access::Hover(window, {100,4});
            auto tools = root.FindName(L"DocumentToolbar").as<FrameworkElement>();
            auto keep = root.FindName(L"KeepLocalButton").as<FrameworkElement>();
            // Hit testing becomes available after the InfoBar's opening transition.
            Pump([&] { return tools.Opacity() == 1. && reachable(close) && reachable(keep); });
            expect(reachable(close)) << "the close button is the pointer target";
            expect(reachable(keep));
            Peers::ButtonAutomationPeer(close).GetPattern(Peers::PatternInterface::Invoke).as<Provider::IInvokeProvider>().Invoke();
            Pump([&] { return !error.IsOpen(); });
            expect(external.IsOpen()) << "dismissing an error preserves an outstanding disk change";
        };
        "tab drag targets remain available while document tools are hidden"_test = [] {
            OwnedWindow source(App()->CreateMainWindow()), target(App()->CreateMainWindow());
            Access::Bars(target.window, {true});
            auto root = target.window.Content().as<FrameworkElement>();
            auto tools = root.FindName(L"DocumentToolbar").as<FrameworkElement>();
            root.UpdateLayout();
            auto id = Access::Documents(source.window).Active()->id;
            auto destination = Access::View(target.window, Access::Documents(target.window).Active()->id);
            auto point = destination.TransformToVisual(nullptr).TransformPoint({
                static_cast<float>(destination.ActualWidth() / 2), static_cast<float>(destination.ActualHeight() / 2)});
            expect(tools.Opacity() == 0.);
            expect(Access::ContainsDragPoint(target.window, root.XamlRoot().CoordinateConverter().ConvertLocalToScreen(point)));
            expect(Access::AllowDrop(target.window, Access::Binding(source.window, id)->item));
            expect(Access::Transfer(source.window, id, target.window, 0));
            expect(Access::Documents(target.window).Active()->id == id);
        };
        "workspace operation progress presents the reported percentage and cancellation in the editor"_test = [] {
            OwnedWindow current(App()->CreateMainWindow());
            auto root = current.window.Content().as<FrameworkElement>();
            auto panel = root.FindName(L"OperationPanel").as<FrameworkElement>();
            auto ring = root.FindName(L"OperationRing").as<Controls::ProgressRing>();
            Access::Progress(current.window, true, .41);
            expect(panel.Visibility() == Visibility::Visible);
            expect(ring.Value() == 41.);
            expect(!ring.IsIndeterminate() && ring.IsActive());
            expect(root.FindName(L"OperationPercent").as<Controls::TextBlock>().Text() == L"41%");
            expect(root.FindName(L"CancelOperationButton").as<Controls::Button>().Visibility() == Visibility::Visible);
            Access::Progress(current.window, false, 0.);
            expect(panel.Visibility() == Visibility::Collapsed);
            expect(!ring.IsActive());
        };

        "workspace sidebar switches between the folder tree and active document headings"_test = [] {
            TestDirectory directory;
            auto folder = directory.path / L"Notebook";
            std::filesystem::create_directories(folder / L"Figures");
            auto file = folder / L"Observations.md";
            std::ofstream(file) << "# Observations\n\nA short entry.\n\n## Results\n\nNotes.\n";
            std::ofstream(folder / L"Reading list.md") << "# Reading list";
            OwnedWindow current(App()->CreateMainWindow());
            auto window = current.window;
            auto root = window.Content().as<Controls::Grid>();
            Access::SetRecentHistory(window, directory.path / L"recent.json");
            Access::OpenFolder(window, folder);
            Access::Open(window, file);
            auto pane = root.FindName(L"WorkspaceSidebar").as<FrameworkElement>();
            auto list = root.FindName(L"OutlineList").as<Controls::ListView>();
            auto tree = root.FindName(L"FileTree").as<Controls::TreeView>();
            Pump([&] { return pane.Visibility() == Visibility::Visible && tree.RootNodes().Size() && list.Items().Size() == 2; });
            root.UpdateLayout();
            auto treeRoot = tree.RootNodes().GetAt(0);
            auto header = root.FindName(L"FolderHeader").as<FrameworkElement>();
            auto newFile = root.FindName(L"FolderNewFileButton").as<Controls::Button>();
            auto sidebarButton = root.FindName(L"SidebarButton").as<FrameworkElement>();
            auto centerY = [&](FrameworkElement const& element) {
                return element.TransformToVisual(root).TransformPoint({0, static_cast<float>(element.ActualHeight() / 2.)}).Y;
            };
            expect(header.Visibility() == Visibility::Visible && newFile.Command() != nullptr);
            expect(std::abs(centerY(newFile) - centerY(sidebarButton)) <= 1.f);
            expect(tree.RootNodes().Size() == 3 && treeRoot.Depth() == 0);
            auto filesTab = root.FindName(L"FilesTab").as<Controls::Primitives::ToggleButton>();
            auto outlineTab = root.FindName(L"OutlineTab").as<Controls::Primitives::ToggleButton>();
            expect(filesTab.IsChecked().GetBoolean());
            Access::Pane(window, true);
            expect(outlineTab.IsChecked().GetBoolean() && !filesTab.IsChecked().GetBoolean());
            expect(list.Visibility() == Visibility::Visible);
            expect(root.FindName(L"OutlineEmptyText").as<FrameworkElement>().Visibility() == Visibility::Collapsed);
            Access::Pane(window, false);
            expect(tree.RootNodes().GetAt(0) == treeRoot);
        };

        "workspace recent menus reopen a file and a folder in a new window"_test = [] {
            TestDirectory directory;
            auto folder = directory.path / L"资料";
            std::filesystem::create_directory(folder);
            auto file = folder / L"笔记.md";
            std::ofstream(file) << "# Saved note";
            auto history = directory.path / L"recent.json";
            auto sameEntry = [](std::filesystem::path const& actual, std::filesystem::path const& expected) {
                std::error_code error;
                return !actual.empty() && std::filesystem::equivalent(actual, expected, error);
            };
            {
                OwnedWindow first(App()->CreateMainWindow());
                Access::SetRecentHistory(first.window, history);
                Access::Open(first.window, file);
                Access::OpenFolder(first.window, folder);
                Pump([&] {
                    auto saved = winrt::Folia::RecentWorkspace(history).Read();
                    return !saved.files.empty() && !saved.folders.empty();
                });
            }

            OwnedWindow second(App()->CreateMainWindow());
            Access::SetRecentHistory(second.window, history);
            auto flyout = Access::OpenFileMenu(second.window);
            auto files = Access::RecentFiles(second.window);
            expect(fatal(files.Size() > 0_u));
            auto recentFile = files.GetAt(0).as<Controls::MenuFlyoutItem>();
            expect(recentFile.Text() == L"笔记.md" && recentFile.Visibility() == Visibility::Visible);
            recentFile.Command().Execute(recentFile.CommandParameter());
            flyout.Hide();
            Pump([&] {
                auto active = Access::Documents(second.window).Active();
                return active->editor && active->editor->loaded && sameEntry(active->path, file);
            });

            flyout = Access::OpenFileMenu(second.window);
            auto folders = Access::RecentFolders(second.window);
            expect(fatal(folders.Size() > 0_u));
            auto recentFolder = folders.GetAt(0).as<Controls::MenuFlyoutItem>();
            expect(recentFolder.Text() == L"资料" && recentFolder.Visibility() == Visibility::Visible);
            recentFolder.Command().Execute(recentFolder.CommandParameter());
            flyout.Hide();
            Pump([&] { return sameEntry(Access::FolderRoot(second.window), folder); });
        };
        "workspace image tabs decode PNG and SVG and keep the editor view separate"_test = [] {
            using namespace winrt::Folia;
            using namespace winrt::Microsoft::UI::Xaml::Media::Imaging;
            TestDirectory directory;
            auto raster = directory.path / L"photo 中文.PNG";
            auto vector = directory.path / L"picture.svg";
            auto invalid = directory.path / L"invalid.png";
            auto data = folia::platform::image_source::DecodeImageDataUri(
                "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAICAIAAAB/FOjAAAAAGElEQVR4nGN0r1rPQApgIkk1w6gGBqIAAD4eAYCPKMJsAAAAAElFTkSuQmCC");
            expect(fatal(data.has_value()));
            std::ofstream(raster, std::ios::binary).write(reinterpret_cast<const char*>(data->data()), data->size());
            std::ofstream(vector) << R"(<svg xmlns="http://www.w3.org/2000/svg" width="40" height="20"><rect width="40" height="20" fill="navy"/></svg>)";
            std::ofstream(invalid) << "not a PNG";
            OwnedWindow source(App()->CreateMainWindow());
            expect(bool(WorkspaceIconXaml(WorkspaceIcon::Markdown) != WorkspaceIconXaml(WorkspaceIcon::Image)));
            expect(bool(WorkspaceIconXaml(WorkspaceIcon::Document) != WorkspaceIconXaml(WorkspaceIcon::Image)));
            Access::New(source.window);
            auto document = Access::Documents(source.window).Active();
            Access::Edit(source.window, U"kept text");

            Access::Open(source.window, raster, WorkspaceOpenMode::Preview);
            auto photo = Access::Documents(source.window).Active();
            expect(photo->path == raster && !photo->editor);
            Pump([&] {
                auto bitmap = Access::Preview(source.window).Source().try_as<BitmapImage>();
                return bitmap && bitmap.PixelWidth() == 16 && Access::PreviewMessage(source.window).Text().empty()
                    && Access::Preview(source.window).ActualWidth() > 0. && Access::PreviewViewer(source.window).ViewportWidth() > 0.;
            });
            expect(Access::PreviewViewer(source.window).ChangeView(nullptr, nullptr, 2.0f, true)) << "PNG receives a zoom request";
            Pump([&] { return Access::PreviewViewer(source.window).ZoomFactor() == 2.0f; });
            Access::Open(source.window, raster, WorkspaceOpenMode::Keep);
            expect(photo == Access::Documents(source.window).Active() && photo->kept);
            auto renamed = directory.path / L"renamed.png";
            Await(Access::Rename(source.window, raster, renamed));
            expect(photo->path == renamed && std::filesystem::exists(renamed));

            Access::Open(source.window, invalid, WorkspaceOpenMode::Preview);
            Pump([&] { return Access::PreviewMessage(source.window).Text() == Localize(L"ImagePreviewFailed"); });
            Access::Open(source.window, vector, WorkspaceOpenMode::Preview);
            Pump([&] {
                return Access::Preview(source.window).Source().try_as<SvgImageSource>()
                    && Access::PreviewMessage(source.window).Text().empty();
            });
            expect(Access::PreviewViewer(source.window).ZoomFactor() == 1.0f) << "SVG opens at its default zoom";
            expect(Access::Documents(source.window).Items().size() == 3_u);
            Access::Tabs(source.window).SelectedItem(Access::Binding(source.window, document->id)->item);
            expect(Access::Documents(source.window).Active() == document);
            expect(!Access::Preview(source.window).Source());
            expect(document->editor->session.TextUtf8() == "kept text" && document->IsModified());
        };
        "caption move after selecting another tab rechecks a click without a nonclient release"_test = [] {
            OwnedWindow source(App()->CreateMainWindow());
            Access::Bars(source.window, {});
            const auto a = Access::Documents(source.window).Active();
            Access::New(source.window);
            const auto b = Access::Documents(source.window).Active();
            auto tabs = Access::Tabs(source.window);
            auto root = source.window.Content().as<FrameworkElement>();
            const auto point = [&](FrameworkElement const& element) {
                root.UpdateLayout();
                auto bounds = element.TransformToVisual(nullptr).TransformBounds({0, 0,
                    static_cast<float>(element.ActualWidth()), static_cast<float>(element.ActualHeight())});
                return root.XamlRoot().CoordinateConverter().ConvertLocalToScreen(
                    {bounds.X + bounds.Width / 2, bounds.Y + bounds.Height / 2});
            };
            for (const auto& document : {a, b, a, b}) {
                Access::PointerBegin(source.window, point(Access::View(source.window, document->id)));
                tabs.SelectedItem(Access::Binding(source.window, document->id)->item);
                expect(Access::Documents(source.window).Active() == document) << "selected document" << document->id;
                expect(Access::PressedTab(source.window)) << "visible tab owns the native move" << document->id;
                // Selection can route the release to XAML instead. A new native
                // move must use its own pointer origin even without PointerEnd.
                Access::PointerBegin(source.window, point(root.FindName(L"TitleDragArea").as<FrameworkElement>()));
                expect(!Access::PressedTab(source.window)) << "a previous tab press cannot own a later caption move";
                expect(Access::Documents(source.window).Items().size() == 2_u);
                Access::PointerEnd(source.window);
                expect(Access::Idle(source.window));
            }
        };
        "the blank title bar moves the window with the sidebar open or closed"_test = [] {
            OwnedWindow source(App()->CreateMainWindow());
            auto window = source.window;
            Access::Bars(window, {});
            Access::New(window);
            auto root = window.Content().as<FrameworkElement>();
            auto drag = root.FindName(L"TitleDragArea").as<FrameworkElement>();
            auto add = Access::NewTabButton(window);
            auto id = window.AppWindow().Id();
            auto native = winrt::Microsoft::UI::Content::ContentCoordinateConverter::CreateForWindowId(id);
            auto input = winrt::Microsoft::UI::Input::InputNonClientPointerSource::GetForWindowId(id);
            auto center = [&](FrameworkElement const& element) {
                return root.XamlRoot().CoordinateConverter().ConvertLocalToScreen(
                    element.TransformToVisual(nullptr).TransformPoint({static_cast<float>(element.ActualWidth()/2),
                        static_cast<float>(element.ActualHeight()/2)}));
            };
            auto captionAt = [&](auto screen) {
                auto point = native.ConvertScreenToLocal(screen);
                return std::ranges::any_of(input.GetRegionRects(winrt::Microsoft::UI::Input::NonClientRegionKind::Caption),
                    [&](auto rect) { return point.X >= rect.X && point.X < rect.X + rect.Width && point.Y >= rect.Y && point.Y < rect.Y + rect.Height; });
            };
            for (bool open : {false, true, false}) {
                Access::Sidebar(window, open);
                root.UpdateLayout();
                Pump([&] { return captionAt(center(drag)) && !captionAt(center(add)); });
                Access::PointerBegin(window, center(drag));
                expect(!Access::PressedTab(window));
                expect(captionAt(center(drag)));
                Access::PointerEnd(window);
                expect(Access::Idle(window));
            }
        };
        "middle mouse closes the pressed caption tab only on a matching release"_test = [] {
            OwnedWindow current(App()->CreateMainWindow());
            auto window = current.window;
            auto root = window.Content().as<FrameworkElement>();
            auto tabs = Access::Tabs(window);
            auto selected = tabs.SelectedItem();
            Access::New(window);
            auto inactive = tabs.SelectedItem();
            tabs.SelectedItem(selected);
            root.UpdateLayout();
            auto getCenter = [&](FrameworkElement const& element) {
                return root.XamlRoot().CoordinateConverter().ConvertLocalToScreen(
                    element.TransformToVisual(root).TransformPoint({static_cast<float>(element.ActualWidth() / 2),
                        static_cast<float>(element.ActualHeight() / 2)}));
            };
            auto target = getCenter(tabs.ContainerFromItem(inactive).as<FrameworkElement>());
            auto other = getCenter(tabs.ContainerFromItem(selected).as<FrameworkElement>());
            auto blank = getCenter(root.FindName(L"TitleDragArea").as<FrameworkElement>());
            auto hwnd = Access::Handle(window);
            auto id = window.AppWindow().Id();
            auto nonClient = winrt::Microsoft::UI::Input::InputNonClientPointerSource::GetForWindowId(id);
            auto native = winrt::Microsoft::UI::Content::ContentCoordinateConverter::CreateForWindowId(id);
            auto captionPoint = native.ConvertScreenToLocal(target);
            Pump([&] {
                return std::ranges::any_of(nonClient.GetRegionRects(winrt::Microsoft::UI::Input::NonClientRegionKind::Caption),
                    [&](auto rect) {
                        return captionPoint.X >= rect.X && captionPoint.X < rect.X + rect.Width
                            && captionPoint.Y >= rect.Y && captionPoint.Y < rect.Y + rect.Height;
                    });
            });
            // Resolve the caption HWND from the screen point, as Windows does,
            // without moving the desktop pointer in the regression suite.
            auto press = [&](auto point) {
                POINT screen{point.X, point.Y};
                return Access::MiddleMouse(WM_NCMBUTTONDOWN, {screen, ::WindowFromPoint(screen), HTCAPTION, 0});
            };
            auto release = [&](auto point) {
                return Access::MiddleMouse(WM_MBUTTONUP, {{point.X, point.Y}, hwnd, HTCLIENT, 0});
            };
            expect(!press(blank));
            expect(::GetCapture() != hwnd);
            expect(press(target));
            expect(::GetCapture() == hwnd);
            expect(Access::Items(window).Size() == 2_u);
            expect(release(other));
            expect(::GetCapture() != hwnd);
            expect(press(target));
            ::SendMessageW(hwnd, WM_CANCELMODE, 0, 0);
            expect(::GetCapture() != hwnd);
            expect(!release(target));
            expect(Access::Items(window).Size() == 2_u);
            expect(press(target));
            expect(release(target));
            Pump([&] { return Access::Items(window).Size() == 1; });
            expect(tabs.SelectedItem() == selected);
            expect(Access::Items(window).GetAt(0) == selected);
        };
        "caption gestures after tab selection cannot enter the native tab tear out loop"_test = [] {
            OwnedWindow source(App()->CreateMainWindow());
            Access::Bars(source.window, {});
            for (int i = 0; i < 10; ++i) Access::New(source.window);
            auto tabs = Access::Tabs(source.window);
            auto root = source.window.Content().as<FrameworkElement>();
            auto settle = [](std::chrono::milliseconds duration) {
                const auto end = std::chrono::steady_clock::now() + duration;
                Pump([&] { return std::chrono::steady_clock::now() >= end; });
            };
            auto screenCenter = [&](FrameworkElement const& element) {
                root.UpdateLayout();
                const auto bounds = element.TransformToVisual(nullptr).TransformBounds({0, 0,
                    static_cast<float>(element.ActualWidth()), static_cast<float>(element.ActualHeight())});
                return root.XamlRoot().CoordinateConverter().ConvertLocalToScreen({bounds.X + bounds.Width / 2, bounds.Y + bounds.Height / 2});
            };
            for (auto index : {0u, 10u, 2u, 9u}) {
                tabs.SelectedItem(Access::Items(source.window).GetAt(index));
                Pump([&] { return tabs.ContainerFromItem(tabs.SelectedItem()) != nullptr; });
                // Allow the native bring-into-view operation to finish layout.
                settle(std::chrono::milliseconds(80));
                auto active = Access::Documents(source.window).Active()->id;
                const auto tab = Access::View(source.window, active);
                Access::PointerBegin(source.window, screenCenter(tab));
                expect(Access::PressedTab(source.window));
                Access::PointerEnd(source.window);
                expect(Access::Idle(source.window));
                const auto caption = screenCenter(root.FindName(L"TitleDragArea").as<FrameworkElement>());
                Access::PointerBegin(source.window, caption);
                expect(!Access::PressedTab(source.window));
                Access::Moving(source.window, true);
                Access::PointerEnd(source.window);
                settle(std::chrono::milliseconds(20));
                expect(!Access::PressedTab(source.window)) << "release must not reattach native tab callbacks during caption movement";
                Access::Moving(source.window, false);
                Access::PointerEnd(source.window);
                expect(Access::Idle(source.window));
                expect(Access::Documents(source.window).Active()->id == active);
                expect(Access::Documents(source.window).Items().size() == 11_u);
                // A queued idle callback from a previous gesture cannot arm a
                // new caption gesture that started before it was delivered.
                Access::PointerBegin(source.window, caption);
                Access::PointerEnd(source.window);
                Access::PointerBegin(source.window, caption);
                settle(std::chrono::milliseconds(20));
                expect(!Access::PressedTab(source.window));
                Access::PointerEnd(source.window);
                expect(Access::Idle(source.window));
            }
        };
        "completion status uses the native ring only while a request is running"_test = [] {
            OwnedWindow owned(App()->CreateMainWindow());
            auto root = owned.window.Content().as<FrameworkElement>();
            auto ring = root.FindName(L"CompletionProgressRing").as<Controls::ProgressRing>();
            auto panel = root.FindName(L"CompletionStatusPanel").as<Controls::StackPanel>();
            auto text = root.FindName(L"CompletionStatusText").as<Controls::TextBlock>();
            using winrt::Folia::CompletionState;
            for (auto state : {CompletionState::Requesting, CompletionState::Ready, CompletionState::Requesting,
                CompletionState::Empty, CompletionState::BudgetExhausted, CompletionState::Failed, CompletionState::Idle}) {
                Access::CompletionStatus(owned.window, state);
                expect(ring.IsActive() == (state == CompletionState::Requesting));
                expect(panel.Visibility() == (state == CompletionState::Idle ? Visibility::Collapsed : Visibility::Visible));
                expect(!text.Text().empty());
            }
        };
        "closing a source window releases its unused tab preparation windows"_test = [] {
            OwnedWindow source(App()->CreateMainWindow());
            OwnedWindow other(App()->CreateMainWindow());
            auto otherPrepared = Access::TearOutWindow(other.window);
            std::vector<Window> prepared;
            std::vector<winrt::Microsoft::UI::WindowId> ids;
            for (int i = 0; i < 3; ++i) {
                auto window = Access::TearOutWindow(source.window);
                ids.push_back(window.AppWindow().Id());
                prepared.push_back(window);
            }
            Access::RequestNativeClose(source.window);
            Pump([&] { return Access::Closed(source.window); });
            for (std::size_t i = 0; i < prepared.size(); ++i) {
                expect(Access::Closed(prepared[i])) << "unused hidden window must close with its owner";
                expect(App()->FindMainWindow(ids[i]) == nullptr);
                if (!Access::Closed(prepared[i])) Access::Close(prepared[i]);
            }
            expect(!Access::Closed(other.window));
            expect(!Access::Closed(otherPrepared)) << "another source still owns its prepared window";
            Access::Close(other.window);
            expect(Access::Closed(otherPrepared));
            if (!Access::Closed(otherPrepared)) Access::Close(otherPrepared);
        };
        "a revealed tab window survives closing its original source"_test = [] {
            OwnedWindow source(App()->CreateMainWindow());
            auto document = Access::Documents(source.window).Active();
            Access::Edit(source.window, U"detached document");
            Access::New(source.window);
            auto receiving = Access::TearOutWindow(source.window);
            expect(Access::Transfer(source.window, document->id, receiving, 0));
            OwnedWindow target(receiving);
            auto id = target.window.AppWindow().Id();
            Access::RequestNativeClose(source.window);
            Pump([&] { return Access::Closed(source.window); });
            expect(!Access::Closed(target.window));
            expect(App()->FindMainWindow(id) != nullptr);
            expect(Access::Documents(target.window).Active() == document);
            expect(document->editor->session.TextUtf8() == "detached document");
        };
        "tab activation coalesces intermediate drawing until the shell frame"_test = [] {
            OwnedWindow source(App()->CreateMainWindow());
            Access::Edit(source.window, U"first");
            auto first = Access::Documents(source.window).Active();
            Access::New(source.window);
            Access::Edit(source.window, U"second document");
            auto second = Access::Documents(source.window).Active();
            auto tabs = Access::Tabs(source.window);
            auto root = source.window.Content().as<FrameworkElement>(); root.UpdateLayout();
            auto count = root.FindName(L"CharacterCountText").as<Controls::TextBlock>();
            auto& renderer = Access::Renderer(source.window);
            auto presentPending = [&] { return winrt::Folia::EditorRendererTestAccess::PresentPending(renderer); };
            std::vector<Visibility> selectionDuringPresentation;
            auto token = count.RegisterPropertyChangedCallback(Controls::TextBlock::TextProperty(), [&](auto const&, auto const&) {
                auto tab = tabs.ContainerFromItem(tabs.SelectedItem()).as<Controls::TabViewItem>();
                selectionDuringPresentation.push_back(TabPart(tab, L"SelectedBackgroundPath").Visibility());
            });
            for (auto document : {first, second, first, second}) tabs.SelectedItem(Access::Binding(source.window, document->id)->item);
            count.UnregisterPropertyChangedCallback(Controls::TextBlock::TextProperty(), token);
            expect(presentPending()) << "the final document buffer waits for the shell frame";
            expect(Access::Documents(source.window).Active() == second);
            expect(selectionDuringPresentation.size() == 4_u);
            for (auto visibility : selectionDuringPresentation) expect(visibility == Visibility::Visible) << "active tab during document update";
            std::optional<bool> presentedWithShell;
            auto rendered = Media::CompositionTarget::Rendered(winrt::auto_revoke, [&](auto const&, auto const&) {
                if (!presentedWithShell) presentedWithShell = !presentPending();
            });
            root.InvalidateArrange();
            Pump([&] { return presentedWithShell.has_value(); });
            rendered.revoke();
            expect(*presentedWithShell) << "the final buffer is submitted with the shell frame";
        };
        "tab modified indicator uses the Unicode codepoint independently of the build codepage"_test = [] {
            OwnedWindow owned(App()->CreateMainWindow());
            auto id = Access::Documents(owned.window).Active()->id;
            Access::Edit(owned.window, U"x");
            auto header = Access::View(owned.window, id).Header().as<Controls::Panel>();
            auto dirty = header.Children().GetAt(2).as<Controls::TextBlock>();
            expect(dirty.Visibility() == Visibility::Visible);
            expect(dirty.Text().size() == 1_u);
            if (!dirty.Text().empty()) expect(static_cast<unsigned>(dirty.Text()[0]) == 0x25cf);
            auto about = Access::About(owned.window);
            auto findLink = [&](auto& self, DependencyObject const& node) -> Controls::HyperlinkButton {
                if (auto link = node.try_as<Controls::HyperlinkButton>()) return link;
                for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(node); ++i)
                    if (auto link = self(self, Media::VisualTreeHelper::GetChild(node, i))) return link;
                return nullptr;
            };
            auto link = findLink(findLink, about);
            expect(fatal(bool(link)));
            expect(winrt::unbox_value<winrt::hstring>(link.Content()) == L"water2004/Folia  \u2197");
            expect(link.NavigateUri().AbsoluteUri() == L"https://github.com/water2004/Folia");
        };
        "hidden and preparation-only tab windows cannot receive an external merge"_test = [] {
            OwnedWindow source(App()->CreateMainWindow()), target(App()->CreateMainWindow());
            auto document = Access::Documents(source.window).Active();
            auto item = Access::Binding(source.window, document->id)->item;
            target.window.AppWindow().Hide();
            expect(!Access::AllowDrop(target.window, item));
            target.window.Activate();
            expect(Access::AllowDrop(target.window, item));
            Access::End(source.window);
            auto pending = Access::TearOutWindow(source.window);
            pending.AppWindow().Show(false);
            expect(!Access::AllowDrop(pending, item));
            expect(Access::Documents(source.window).Active() == document);
            Access::Close(pending);
        };
        "tab merge hit region agrees with native reorder region after layout and overflow"_test = [] {
            OwnedWindow source(App()->CreateMainWindow()), target(App()->CreateMainWindow());
            auto check = [&] {
                auto tabs = Access::Tabs(target.window);
                target.window.Content().as<FrameworkElement>().UpdateLayout();
                auto list = tabs.as<Controls::IControlProtected>().GetTemplateChild(L"TabListView").as<FrameworkElement>();
                auto bounds = list.TransformToVisual(nullptr).TransformBounds({0, 0,
                    static_cast<float>(list.ActualWidth()), static_cast<float>(list.ActualHeight())});
                auto screen = [&](float x, float y) {
                    return list.XamlRoot().CoordinateConverter().ConvertLocalToScreen(winrt::Windows::Foundation::Point{x, y});
                };
                auto middleY = bounds.Y + bounds.Height / 2;
                expect(Access::ContainsDragPoint(target.window, screen(bounds.X + bounds.Width / 2, middleY)));
                expect(!Access::ContainsDragPoint(target.window, screen(bounds.X - 3, middleY)));
                expect(!Access::ContainsDragPoint(target.window, screen(bounds.X + bounds.Width + 3, middleY)));
                expect(!Access::ContainsDragPoint(target.window, screen(bounds.X + 3, bounds.Y - 3)));
                expect(!Access::ContainsDragPoint(target.window, screen(bounds.X + 3, bounds.Y + bounds.Height + 3)));
            };
            check();
            auto id = Access::Documents(source.window).Active()->id;
            expect(Access::Transfer(source.window, id, target.window, 0));
            check();
            for (int i = 0; i < 10; ++i) Access::New(target.window);
            target.window.AppWindow().Resize({720, 600});
            check();
            target.window.AppWindow().Resize({1200, 800});
            check();
        };
        "native tab preparatory show must not deactivate the document window"_test = [] {
            OwnedWindow source(App()->CreateMainWindow());
            Access::New(source.window);
            auto document = Access::Documents(source.window).Active();
            source.window.Activate();
            auto sourceHandle = Access::Handle(source.window);
            expect(::GetActiveWindow() == sourceHandle);
            auto pending = Access::Begin(source.window, document->id);
            std::size_t deactivated = 0;
            auto token = source.window.Activated([&](auto const&, WindowActivatedEventArgs const& args) {
                if (args.WindowActivationState() == WindowActivationState::Deactivated) ++deactivated;
            });
            RECT bounds{};
            ::GetWindowRect(sourceHandle, &bounds);
            // Reproduce the SDK call, including its lack of SWP_NOACTIVATE.
            // Show(false) did not exercise the window activation regression.
            expect(::SetWindowPos(Access::Handle(pending), sourceHandle, bounds.left, bounds.top,
                bounds.right - bounds.left, bounds.bottom - bounds.top, SWP_SHOWWINDOW) != FALSE);
            RECT placed{};
            expect(::GetWindowRect(Access::Handle(pending), &placed) != FALSE);
            expect(placed.left == bounds.left && placed.top == bounds.top);
            expect(::GetActiveWindow() == sourceHandle);
            pending.AppWindow().Hide();
            expect(deactivated == 0_u);
            source.window.Activated(token);
            Access::End(source.window);
            Access::Close(pending);
        };
        "native tab gesture retains its host across merge and repeated tear-out using the stable data item"_test = [] {
            OwnedWindow source(App()->CreateMainWindow()), other(App()->CreateMainWindow());
            auto document = Access::Documents(source.window).Active();
            Access::Edit(source.window, U"same document");
            auto item = Access::Binding(source.window, document->id)->item;
            auto firstView = Access::View(source.window, document->id);
            Access::New(source.window);
            auto pending = Access::Begin(source.window, document->id);
            expect((Access::Cloaked(pending) & DWM_CLOAKED_APP) != 0_u);
            expect(!pending.AppWindow().IsShownInSwitchers());
            expect(Access::Complete(source.window, item));
            OwnedWindow target(pending);
            expect(Access::Documents(target.window).Active() == document);
            expect(Access::Binding(target.window, document->id)->item == item);
            expect(Access::View(target.window, document->id) != firstView);
            expect(Access::View(target.window, document->id).XamlRoot() == target.window.Content().as<FrameworkElement>().XamlRoot());
            expect(Access::Cloaked(target.window) == 0_u && target.window.AppWindow().IsShownInSwitchers());
            expect(::GetActiveWindow() == Access::Handle(target.window));
            for (int i = 0; i < 3; ++i) {
                expect(Access::AllowDrop(other.window, item));
                expect(Access::Transfer(target.window, document->id, other.window, 0));
                // WinUI continues the same gesture on the new owner. Its
                // original UIElement and NewWindowId=0 are not document identity.
                expect(Access::Complete(other.window, item));
                expect(Access::Documents(target.window).Active() == document);
                expect(document->editor->session.TextUtf8() == "same document");
            }
            Access::End(source.window);
            expect(!Access::Complete(target.window, item));
            Access::Edit(target.window, U" continues");
            expect(document->editor->session.TextUtf8() == "same document continues");
        };
        "click-only tab window preparation stays cloaked until the unused host closes"_test = [] {
            OwnedWindow source(App()->CreateMainWindow());
            auto document = Access::Documents(source.window).Active();
            Access::New(source.window);
            for (int i = 0; i < 4; ++i) {
                auto pending = Access::Begin(source.window, document->id);
                auto id = pending.AppWindow().Id();
                pending.AppWindow().Show(false);
                expect((Access::Cloaked(pending) & DWM_CLOAKED_APP) != 0_u);
                expect(!pending.AppWindow().IsShownInSwitchers());
                Access::End(source.window);
                Access::RequestNativeClose(pending);
                Pump([&] { return Access::Closed(pending); });
                expect(App()->FindMainWindow(id) == nullptr);
                expect(Access::Documents(source.window).Find(document->id) == document);
            }
        };
        "empty tabs merge before the receiving window is loaded"_test = [] {
            OwnedWindow source(App()->CreateMainWindow());
            auto document = Access::Documents(source.window).Active();
            auto original = Access::View(source.window, document->id);
            auto receiving = Access::TearOutWindow(source.window);
            expect(!receiving.Content().as<FrameworkElement>().IsLoaded());
            expect(Access::Transfer(source.window, document->id, receiving, 0));
            OwnedWindow target(receiving);
            expect(Access::Documents(target.window).Active() == document);
            expect(Access::View(target.window, document->id) != original);
            expect(Access::Documents(target.window).Items().size() == 1_u);
            Access::Edit(target.window, U"original empty document");
            expect(document->editor->session.TextUtf8() == "original empty document");
        };
        "two untouched empty windows merge and close their emptied source"_test = [] {
            OwnedWindow source(App()->CreateMainWindow()), target(App()->CreateMainWindow());
            auto document = Access::Documents(source.window).Active();
            auto existing = Access::Documents(target.window).Active();
            expect(Access::Transfer(source.window, document->id, target.window, 1)) << "merge into the second empty window";
            Access::RequestNativeClose(source.window);
            Pump([&] { return Access::Closed(source.window); });
            source.window = nullptr;
            for (int i = 0; i < 6; ++i) {
                Access::Tabs(target.window).SelectedItem(Access::Binding(target.window, existing->id)->item);
                expect(Access::Documents(target.window).Active() == existing) << "select the pre-existing tab" << i;
                Access::Tabs(target.window).SelectedItem(Access::Binding(target.window, document->id)->item);
                expect(Access::Documents(target.window).Active() == document) << "select the merged tab" << i;
            }
            auto receiving = Access::TearOutWindow(target.window);
            expect(Access::Transfer(target.window, document->id, receiving, 0)) << "tear the merged tab out again";
            OwnedWindow detached(receiving);
            Access::Edit(detached.window, U"moved empty document");
            expect(document->editor->session.TextUtf8() == "moved empty document") << "focused editor receives input";
        };
        "closed window notifications cannot access detached or destroyed controllers"_test = [] {
            OwnedWindow owned(App()->CreateMainWindow());
            std::size_t calls = 0;
            auto notification = Access::Notification(owned.window, calls);
            notification(nullptr, nullptr);
            expect(calls == 1_u);
            Access::Close(owned.window);
            Access::Render(owned.window);
            notification(nullptr, nullptr);
            expect(calls == 1_u);
            owned.window = nullptr;
            notification(nullptr, nullptr);
            expect(calls == 1_u);
        };
        "tabs tear into a native window and retain dirty source undo selection and window-local tab containers"_test = [] {
            OwnedWindow source(App()->CreateMainWindow());
            auto document = Access::Documents(source.window).Active();
            Access::Edit(source.window, U"**原文** $x+1$\n");
            expect(document->editor->session.EnterSourceMode());
            Access::Edit(source.window, U"prefix ");
            auto before = document->editor->session.TextUtf8();
            auto selection = document->editor->session.Selection();
            auto state = document->editor->session.ContentState();
            auto originalTab = Access::View(source.window, document->id);
            Access::New(source.window);
            OwnedWindow target(Access::TearOutWindow(source.window));
            expect(App()->FindMainWindow(target.window.AppWindow().Id()) == Access::Impl(target.window));
            folia::reset_core_operation_counters();
            expect(Access::Transfer(source.window, document->id, target.window, 0));
            expect(Access::Documents(source.window).Items().size() == 1_u);
            expect(Access::Documents(target.window).Items().size() == 1_u);
            expect(Access::Documents(target.window).Active().get() == document.get());
            expect(Access::View(target.window, document->id) != originalTab);
            expect(Access::Binding(target.window, document->id)->owner.get().get() == Access::Impl(target.window));
            expect(document->editor->session.TextUtf8() == before && document->editor->session.Selection() == selection);
            expect(document->editor->session.ContentState() == state && document->editor->controller.IsModified() && document->editor->session.IsSourceMode());
            auto firstCounters = folia::read_core_operation_counters();
            expect(firstCounters.full_document_parses == 0_u && firstCounters.full_document_serializations == 0_u)
                << "moving an existing session rebuilt its source";
            for (int i = 0; i < 4; ++i) {
                expect(Access::Transfer(target.window, document->id, source.window, 0));
                expect(Access::Transfer(source.window, document->id, target.window, 0));
            }
            // Later round trips create new empty placeholders when a window
            // loses its final tab. Their initialization is not a document edit.
            expect(document->editor->session.TextUtf8() == before) << "source changed on repeated transfer";
            expect(document->editor->session.Selection() == selection) << "selection changed on repeated transfer";
            expect(document->editor->session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            expect(document->editor->session.ExecuteCommand(folia::Command{folia::CommandKind::Redo}));
            expect(document->editor->session.TextUtf8() == before) << "source changed after transfer undo/redo";
            expect(document->editor->session.Selection() == selection) << "selection changed after transfer undo/redo";
            if (document->editor->session.Selection() != selection) {
                auto now = document->editor->session.Selection();
                std::fprintf(stderr, "source selection before=%llu:%zu/%d,%llu:%zu/%d after=%llu:%zu/%d,%llu:%zu/%d\n",
                    selection.anchor.container_id.v, selection.anchor.source_offset, static_cast<int>(selection.anchor.affinity),
                    selection.active.container_id.v, selection.active.source_offset, static_cast<int>(selection.active.affinity),
                    now.anchor.container_id.v, now.anchor.source_offset, static_cast<int>(now.anchor.affinity),
                    now.active.container_id.v, now.active.source_offset, static_cast<int>(now.active.affinity));
            }
            Access::Edit(target.window, U"target ");
            expect(document->editor->controller.IsModified());
            expect(!Access::Documents(source.window).Find(document->id));
        };
        "tabs transferred from the last source tab remove only implicit placeholders on return"_test = [] {
            OwnedWindow source(App()->CreateMainWindow());
            auto document = Access::Documents(source.window).Active();
            Access::Edit(source.window, U"unsaved last tab");
            OwnedWindow target(Access::TearOutWindow(source.window));
            expect(Access::Transfer(source.window, document->id, target.window, 0));
            expect(Access::Documents(source.window).Items().size() == 1_u);
            auto placeholder = Access::Documents(source.window).Active();
            expect(placeholder != document && !placeholder->editor->controller.IsModified());
            expect(Access::Transfer(target.window, document->id, source.window, 0));
            expect(Access::Documents(source.window).Items().size() == 1_u);
            expect(Access::Documents(source.window).Active() == document);
            expect(document->editor->session.TextUtf8() == "unsaved last tab" && document->editor->controller.IsModified());
            expect(!Access::Documents(source.window).Find(placeholder->id));
        };
        "native tab collection reorder updates model order after tear-out mode replaces old drag events"_test = [] {
            OwnedWindow source(App()->CreateMainWindow());
            auto first = Access::Documents(source.window).Active();
            Access::New(source.window); Access::New(source.window);
            auto tabs = Access::Items(source.window);
            auto view = tabs.GetAt(0);
            tabs.RemoveAt(0); tabs.Append(view);
            Pump([&] { return Access::Documents(source.window).Items().back() == first; });
            expect(Access::Documents(source.window).Items().back() == first);
            expect(tabs.Size() == 3_u);
        };
        "native reorder of the selected tab does not reenter the observable collection notification"_test = [] {
            OwnedWindow source(App()->CreateMainWindow());
            auto first = Access::Documents(source.window).Active();
            Access::New(source.window); Access::New(source.window);
            auto item = Access::Binding(source.window, first->id)->item;
            auto tabs = Access::Items(source.window);
            Access::Tabs(source.window).SelectedItem(item);
            expect(Access::Documents(source.window).Active() == first);
            tabs.RemoveAt(0);
            tabs.Append(item);
            Access::Tabs(source.window).SelectedItem(item);
            Pump([&] { return Access::Documents(source.window).Items().back() == first; });
            expect(Access::Documents(source.window).Active() == first);
            expect(Access::Documents(source.window).Items().back() == first);
            expect(tabs.Size() == 3_u);
        };
        "cross-window tabs reject duplicate file owners and preserve both edited sessions"_test = [] {
            TestDirectory directory;
            OwnedWindow source(App()->CreateMainWindow()), target(App()->CreateMainWindow());
            auto path = directory.path / L"shared.md";
            std::ofstream(path, std::ios::binary) << "original";
            Access::Open(source.window, path); Access::Open(target.window, path);
            Pump([&] { return Access::Documents(source.window).Active()->editor->loaded && Access::Documents(target.window).Active()->editor->loaded
                && Access::Documents(source.window).Active()->editor->session.HasFile() && Access::Documents(target.window).Active()->editor->session.HasFile(); });
            auto a = Access::Documents(source.window).Active();
            auto b = Access::Documents(target.window).Active();
            Access::Edit(source.window, U"first edit"); Access::Edit(target.window, U"second edit");
            auto aText = a->editor->session.TextUtf8(), bText = b->editor->session.TextUtf8();
            expect(!Access::Transfer(source.window, a->id, target.window, 0));
            expect(Access::Documents(source.window).Active() == a && Access::Documents(target.window).Active() == b);
            expect(a->editor->session.TextUtf8() == aText) << "first text=" << a->editor->session.TextUtf8();
            expect(b->editor->session.TextUtf8() == bText) << "second text=" << b->editor->session.TextUtf8();
            expect(a->editor->controller.IsModified()) << "first baseline lost";
            expect(b->editor->controller.IsModified()) << "second baseline lost";
        };
        "tab transfer restores the visible source anchor and its viewport inset in the receiving window"_test = [] {
            TestDirectory directory;
            auto path = directory.path / L"long-document.md";
            std::ofstream text(path, std::ios::binary);
            for (int i=0;i<500;++i) text << "Paragraph " << i << " keeps its stable block position while moving between windows.\n\n";
            text.close();
            OwnedWindow source(App()->CreateMainWindow());
            Access::Open(source.window,path);
            Pump([&] { return Access::Documents(source.window).Active()->editor->loaded && Access::Documents(source.window).Active()->editor->session.HasFile(); });
            auto document=Access::Documents(source.window).Active();
            OwnedWindow target(Access::TearOutWindow(source.window));
            auto& renderer=Access::Renderer(source.window);
            renderer.SetScrollOffset(renderer.MaximumScrollOffset()*.55f);
            Access::Render(source.window);
            auto anchor=renderer.ViewportPosition();
            expect(fatal(anchor.has_value()));
            auto bounds=renderer.CaretBounds(*anchor);
            expect(fatal(bounds.has_value()));
            expect(renderer.ScrollOffset()>100.f);
            auto selection=document->editor->session.Selection();
            expect(Access::Transfer(source.window,document->id,target.window,0));
            auto settled=std::chrono::steady_clock::now()+std::chrono::milliseconds(200);
            Pump([&] { return std::chrono::steady_clock::now()>=settled; });
            auto& receiving=Access::Renderer(target.window);
            auto restored=receiving.CaretBounds(*anchor);
            expect(fatal(restored.has_value()));
            expect(std::abs(restored->top-bounds->top)<2.f) << "visible block shifted after transfer";
            expect(receiving.ScrollOffset()>100.f);
            expect(document->editor->session.Selection()==selection);
            expect(!document->editor->controller.IsModified());
        };
    };
}
