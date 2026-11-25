#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "App.xaml.h"
#include "visual_window.h"
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

import folia.markdown.instrumentation;

namespace winrt::Folia {
    struct EditorRendererTestAccess {
        static std::uint8_t PaperAlpha(EditorSurfaceRenderer const& renderer) {
            auto const& resources = renderer.resources;
            ::Microsoft::WRL::ComPtr<ID3D11Texture2D> buffer, pixel;
            winrt::check_hresult(resources.swapChain->GetBuffer(0, IID_PPV_ARGS(&buffer)));
            D3D11_TEXTURE2D_DESC desc{};
            buffer->GetDesc(&desc);
            auto x = desc.Width / 2, y = desc.Height / 2;
            D3D11_BOX sample{x, y, 0, x + 1, y + 1, 1};
            desc.Width = desc.Height = 1;
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = desc.MiscFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            winrt::check_hresult(resources.d3dDevice->CreateTexture2D(&desc, nullptr, &pixel));
            resources.d3dContext->CopySubresourceRegion(pixel.Get(), 0, 0, 0, 0, buffer.Get(), 0, &sample);
            D3D11_MAPPED_SUBRESOURCE mapped{};
            winrt::check_hresult(resources.d3dContext->Map(pixel.Get(), 0, D3D11_MAP_READ, 0, &mapped));
            auto alpha = static_cast<std::uint8_t const*>(mapped.pData)[3];
            resources.d3dContext->Unmap(pixel.Get(), 0);
            return alpha;
        }
        static UINT PresentCount(EditorSurfaceRenderer const& renderer) {
            UINT count{};
            winrt::check_hresult(renderer.resources.swapChain->GetLastPresentCount(&count));
            return count;
        }
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
        static auto Tabs(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->DocumentTabs(); }
        static auto Handle(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->WindowHandle(); }
        static auto About(Microsoft::UI::Xaml::Window const& window) {
            auto impl = Impl(window);
            impl->OpenSettingsTab();
            impl->settingsView->Navigate(winrt::Folia::SettingsView::Page::About);
            window.Content().as<Microsoft::UI::Xaml::FrameworkElement>().UpdateLayout();
            return impl->SettingsViewHost();
        }
        static bool ContainsDragPoint(Microsoft::UI::Xaml::Window const& window, Windows::Graphics::PointInt32 point) {
            return Impl(window)->ContainsTabDragPoint(point);
        }
        static void PointerBegin(Microsoft::UI::Xaml::Window const& window, Windows::Graphics::PointInt32 point) { Impl(window)->BeginTabPointerGesture(point); }
        static void Sidebar(Microsoft::UI::Xaml::Window const& window, bool open) { Impl(window)->SetSidebarOpen(open); }
        static void Pane(Microsoft::UI::Xaml::Window const& window, bool outline) { Impl(window)->SelectWorkspacePane(outline); }
        static void PointerEnd(Microsoft::UI::Xaml::Window const& window) { Impl(window)->EndTabPointerGesture(); }
        static void EnterMove(Microsoft::UI::Xaml::Window const& window) {
            Impl(window)->EnterTabMove(Microsoft::UI::Input::InputNonClientPointerSource::GetForWindowId(window.AppWindow().Id()));
        }
        static void ExitMove(Microsoft::UI::Xaml::Window const& window) { Impl(window)->ExitTabMove(); }
        static void Moving(Microsoft::UI::Xaml::Window const& window, bool value) { Impl(window)->tabMoveInProgress = value; }
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
            return Impl(window)->CompleteTabTearOut(Impl(window)->FindTransferTab(item));
        }
        static bool AllowDrop(Microsoft::UI::Xaml::Window const& window, Windows::Foundation::IInspectable const& item) {
            return Impl(window)->PrepareExternalTab(item);
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
            Pump([&] { return Access::Renderer(window).FrameLatencyWaitableObject() != nullptr; });
        }
        ~OwnedWindow() { if (window) Access::Close(window); }
    };
    auto App() { return winrt::Folia::implementation::App::Current(); }
    FrameworkElement TabPart(DependencyObject const& element, std::wstring_view name) {
        if (auto value = element.try_as<FrameworkElement>(); value && value.Name() == name) return value;
        for (int i = 0; i < Media::VisualTreeHelper::GetChildrenCount(element); ++i)
            if (auto value = TabPart(Media::VisualTreeHelper::GetChild(element, i), name)) return value;
        return nullptr;
    }
    suite tab_transfer_tests = [] {
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
        "the outward sidebar opens immediately and preserves the reading width"_test = [] {
            OwnedWindow current(App()->CreateMainWindow());
            auto window = current.window;
            auto root = window.Content().as<FrameworkElement>();
            auto card = root.FindName(L"ReadingCard").as<FrameworkElement>();
            auto rail = root.FindName(L"WorkspaceRail").as<FrameworkElement>();
            auto sidebar = root.FindName(L"WorkspaceSidebar").as<FrameworkElement>();
            auto appWindow = window.AppWindow();
            auto display = winrt::Microsoft::UI::Windowing::DisplayArea::GetFromWindowId(appWindow.Id(),
                winrt::Microsoft::UI::Windowing::DisplayAreaFallback::Nearest);
            auto work = display ? display.WorkArea() : winrt::Windows::Graphics::RectInt32{};
            const double scale = root.XamlRoot().RasterizationScale();
            const bool room = work.Width > 820. * scale;
            if (room) {
                const int width = static_cast<int>(std::lround(560. * scale));
                appWindow.MoveAndResize({work.X + static_cast<int>(225. * scale), work.Y + 20,
                    width, (std::min)(appWindow.Size().Height, work.Height - 30)});
                Pump([&] { return appWindow.Size().Width == width && root.ActualWidth() < 570.; });
            }
            root.UpdateLayout();
            const auto original = card.ActualWidth();
            auto bounds = [&] {
                RECT rect{};
                winrt::check_bool(::GetWindowRect(Access::Handle(window), &rect));
                return rect;
            };
            const auto closed = bounds();
            auto readingOrigin = [&] {
                return root.XamlRoot().CoordinateConverter().ConvertLocalToScreen(
                    card.TransformToVisual(nullptr).TransformPoint({0, 0}));
            };
            const int originalLeft = readingOrigin().X;
            Access::Sidebar(window, true);
            auto visual = Hosting::ElementCompositionPreview::GetElementVisual(sidebar);
            expect(sidebar.Visibility() == Visibility::Visible && !visual.Clip()) << "the pane is shown immediately";
            auto expanded = bounds();
            expect(expanded.left < closed.left && expanded.right == closed.right) << "only the left window edge changes";
            // Native bounds change synchronously; XAML arranges at the next layout pass.
            Pump([&] { return std::abs(card.ActualWidth() - original) <= 1. && std::abs(readingOrigin().X - originalLeft) <= 1; });
            expect(rail.ActualHeight() == root.ActualHeight());
            expect(rail.TransformToVisual(root).TransformPoint({0,0}).X == 0.f);
            Access::Sidebar(window, false);
            expect(sidebar.Visibility() == Visibility::Collapsed && !visual.Clip());
            auto contracted = bounds();
            expect(contracted.left == closed.left && contracted.right == closed.right) << "the bounds are restored immediately";
            Pump([&] { return std::abs(card.ActualWidth() - original) <= 1. && std::abs(readingOrigin().X - originalLeft) <= 1; });
        };
        "the inward sidebar animates independently of window size"_test = [] {
            OwnedWindow current(App()->CreateMainWindow());
            auto window = current.window;
            auto appWindow = window.AppWindow();
            appWindow.Presenter().as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>().IsResizable(false);
            auto root = window.Content().as<FrameworkElement>();
            auto card = root.FindName(L"ReadingCard").as<FrameworkElement>();
            auto sidebar = root.FindName(L"WorkspaceSidebar").as<FrameworkElement>();
            auto visual = Hosting::ElementCompositionPreview::GetElementVisual(sidebar);
            root.UpdateLayout();
            const auto position = appWindow.Position();
            const auto size = appWindow.Size();
            const auto width = card.ActualWidth();
            auto expectStationary = [&] {
                expect(appWindow.Position() == position && appWindow.Size() == size);
                expect(card.ActualWidth() == width);
            };
            const bool animated = winrt::Windows::UI::ViewManagement::UISettings().AnimationsEnabled();
            Access::Sidebar(window, true);
            expect(sidebar.Visibility() == Visibility::Visible);
            if (animated) {
                auto clip = visual.Clip().as<winrt::Microsoft::UI::Composition::InsetClip>();
                Pump([&] { return clip.RightInset() == 0.f; });
            }
            expectStationary();
            Access::Sidebar(window, false);
            if (animated) {
                expect(sidebar.Visibility() == Visibility::Visible) << "the panel stays on screen through the closing animation";
                Access::Sidebar(window, true);
                auto clip = visual.Clip().as<winrt::Microsoft::UI::Composition::InsetClip>();
                Pump([&] { return clip.RightInset() == 0.f; });
                expect(sidebar.Visibility() == Visibility::Visible) << "reopening keeps the panel visible";
                Access::Sidebar(window, false);
            }
            Pump([&] { return sidebar.Visibility() == Visibility::Collapsed; });
            expect(!visual.Clip());
            expectStationary();
        };
        "workspace material can leave the reading surface opaque independently of the window"_test = [] {
            using namespace winrt::Microsoft::UI::Composition::SystemBackdrops;
            using winrt::Folia::WindowMaterial;
            auto window = App()->CreateMainWindow();
            expect(Access::MaterialsReady(window)) << "both materials are resolved before the window is activated";
            OwnedWindow owned(window);
            auto root = window.Content().as<FrameworkElement>();
            auto readingBackdrop = root.FindName(L"ReadingBackdrop").as<Controls::SystemBackdropElement>();
            Access::Sidebar(window, false);
            auto tabs = Access::Tabs(window);
            auto first = tabs.SelectedItem();
            Access::New(window);
            auto second = tabs.SelectedItem();
            Pump([&] { return Access::View(window, Access::Documents(window).Active()->id).IsLoaded(); });
            auto paperAlpha = [&] {
                auto& renderer = Access::Renderer(window);
                auto frame = renderer.UpdateWithXaml();
                Access::Render(window);
                return winrt::Folia::EditorRendererTestAccess::PaperAlpha(renderer);
            };
            auto selected = [&]() -> Media::SystemBackdrop {
                auto tab = Access::View(window, Access::Documents(window).Active()->id);
                auto shape = TabPart(tab, L"SelectedBackgroundPath").as<Shapes::Path>();
                expect(shape.Fill() == root.Resources().Lookup(winrt::box_value(L"FoliaReadingSurfaceBrush")).as<Media::Brush>())
                    << "the native tab silhouette uses the reading surface tint";
                auto surface = TabPart(tab, L"SelectedTabBackdrop").try_as<Controls::SystemBackdropElement>();
                return surface ? surface.SystemBackdrop() : nullptr;
            };
            bool highContrast = winrt::Windows::UI::ViewManagement::AccessibilitySettings().HighContrast();
            for (auto material : {WindowMaterial::Solid, WindowMaterial::Mica, WindowMaterial::MicaAlt, WindowMaterial::Acrylic}) {
                bool supported = material == WindowMaterial::Acrylic ? DesktopAcrylicController::IsSupported()
                    : material != WindowMaterial::Solid && MicaController::IsSupported();
                Access::Bars(window, {false, material, {}});
                expect(selected() == nullptr && readingBackdrop.SystemBackdrop() == nullptr);
                auto base = root.as<Controls::Grid>().Background().as<Media::SolidColorBrush>();
                auto paper = root.FindName(L"ReadingTint").as<Controls::Border>().Background().as<Media::SolidColorBrush>();
                expect(base.Color() != paper.Color()) << "the reading card remains distinct when both surfaces share a material";
                if (!supported || highContrast) {
                    expect(window.SystemBackdrop() == nullptr);
                    expect(paperAlpha() == 255);
                    continue;
                }
                if (material == WindowMaterial::Acrylic)
                    expect(window.SystemBackdrop().try_as<Media::DesktopAcrylicBackdrop>() != nullptr);
                else expect(window.SystemBackdrop().as<Media::MicaBackdrop>().Kind()
                    == (material == WindowMaterial::Mica ? MicaKind::Base : MicaKind::BaseAlt));
                expect(paperAlpha() < 255) << "the editor background transmits the window material with the sidebar closed";
                expect(base.Opacity() < 1. && paper.Opacity() < 1.) << "both tints keep the shared material visible";
                Access::Bars(window, {false, material, WindowMaterial::Solid});
                expect(window.SystemBackdrop() != nullptr && readingBackdrop.SystemBackdrop() == nullptr && selected() == nullptr);
                expect(paperAlpha() == 255) << "solid paper takes effect on the next frame";
                Access::Bars(window, {false, WindowMaterial::Solid, material});
                expect(window.SystemBackdrop() == nullptr);
                expect(selected() == readingBackdrop.SystemBackdrop() && readingBackdrop.SystemBackdrop() != nullptr);
                expect(paperAlpha() < 255);
                auto old = Access::View(window, Access::Documents(window).Active()->id);
                tabs.SelectedItem(tabs.SelectedItem() == first ? second : first);
                expect(selected() == readingBackdrop.SystemBackdrop()) << "the active tab immediately inherits the page backdrop";
                expect(!TabPart(old, L"SelectedTabBackdrop")) << "the old tab releases the reading material";
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
            Pump([&] { return tools.Opacity() == 1.; });
            expect(reachable(close)) << "the close button is the pointer target";
            expect(reachable(root.FindName(L"KeepLocalButton").as<FrameworkElement>()));
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
                return bitmap && bitmap.PixelWidth() == 16 && Access::PreviewMessage(source.window).Text().empty();
            });
            expect(Access::Preview(source.window).ActualWidth() > 0.0);
            expect(Access::PreviewViewer(source.window).ViewportWidth() > 0.0);
            expect(Access::PreviewViewer(source.window).ChangeView(nullptr, nullptr, 2.0f, true));
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
            expect(Access::PreviewViewer(source.window).ZoomFactor() == 1.0f);
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
                expect(tabs.CanTearOutTabs()) << "visible tab owns the native move" << document->id;
                // Selection can route the release to XAML instead. A new native
                // move must use its own pointer origin even without PointerEnd.
                Access::PointerBegin(source.window, point(root.FindName(L"TitleDragArea").as<FrameworkElement>()));
                expect(!tabs.CanTearOutTabs()) << "a previous tab press cannot own a later caption move";
                expect(Access::Documents(source.window).Items().size() == 2_u);
                Access::PointerEnd(source.window);
                Pump([&] { return tabs.CanTearOutTabs(); });
            }
        };
        "the blank title bar moves the window with the sidebar open or closed"_test = [] {
            OwnedWindow source(App()->CreateMainWindow());
            auto window = source.window;
            Access::Bars(window, {});
            Access::New(window);
            auto root = window.Content().as<FrameworkElement>();
            auto drag = root.FindName(L"TitleDragArea").as<FrameworkElement>();
            auto split = root.FindName(L"NewTabButton").as<FrameworkElement>();
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
                Pump([&] { return captionAt(center(drag)) && !captionAt(center(split)); });
                Access::PointerBegin(window, center(drag));
                expect(!Access::Tabs(window).CanTearOutTabs());
                expect(captionAt(center(drag)));
                Access::PointerEnd(window);
                Pump([&] { return Access::Tabs(window).CanTearOutTabs(); });
            }
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
                expect(tabs.CanTearOutTabs());
                Access::PointerEnd(source.window);
                Pump([&] { return tabs.CanTearOutTabs(); }); expect(tabs.CanTearOutTabs());
                const auto caption = screenCenter(root.FindName(L"TitleDragArea").as<FrameworkElement>());
                Access::PointerBegin(source.window, caption);
                expect(!tabs.CanTearOutTabs());
                Access::Moving(source.window, true);
                Access::PointerEnd(source.window);
                settle(std::chrono::milliseconds(20));
                expect(!tabs.CanTearOutTabs()) << "release must not reattach native tab callbacks during caption movement";
                Access::Moving(source.window, false);
                Access::PointerEnd(source.window);
                Pump([&] { return tabs.CanTearOutTabs(); }); expect(tabs.CanTearOutTabs());
                expect(Access::Documents(source.window).Active()->id == active);
                expect(Access::Documents(source.window).Items().size() == 11_u);
                // A queued idle callback from a previous gesture cannot arm a
                // new caption gesture that started before it was delivered.
                Access::PointerBegin(source.window, caption);
                Access::PointerEnd(source.window);
                Access::PointerBegin(source.window, caption);
                settle(std::chrono::milliseconds(20));
                expect(!tabs.CanTearOutTabs());
                Access::PointerEnd(source.window);
                Pump([&] { return tabs.CanTearOutTabs(); }); expect(tabs.CanTearOutTabs());
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
            auto presentCount = [&] { return winrt::Folia::EditorRendererTestAccess::PresentCount(renderer); };
            auto before = presentCount();
            std::vector<Visibility> selectionDuringPresentation;
            auto token = count.RegisterPropertyChangedCallback(Controls::TextBlock::TextProperty(), [&](auto const&, auto const&) {
                auto tab = tabs.ContainerFromItem(tabs.SelectedItem()).as<Controls::TabViewItem>();
                selectionDuringPresentation.push_back(TabPart(tab, L"SelectedBackgroundPath").Visibility());
            });
            for (auto document : {first, second, first, second}) tabs.SelectedItem(Access::Binding(source.window, document->id)->item);
            count.UnregisterPropertyChangedCallback(Controls::TextBlock::TextProperty(), token);
            expect(presentCount() == before) << "switching must not submit intermediate document buffers";
            expect(Access::Documents(source.window).Active() == second);
            expect(selectionDuringPresentation.size() == 4_u);
            for (auto visibility : selectionDuringPresentation) expect(visibility == Visibility::Visible) << "active tab during document update";
            std::optional<UINT> presentedWithShell;
            auto rendered = Media::CompositionTarget::Rendered(winrt::auto_revoke, [&](auto const&, auto const&) {
                if (!presentedWithShell) presentedWithShell = presentCount();
            });
            root.InvalidateArrange();
            Pump([&] { return presentedWithShell.has_value(); });
            rendered.revoke();
            expect(*presentedWithShell == before + 1) << "one final buffer is submitted after the shell frame";
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
        "single-tab native merge can tear back into its original window before release"_test = [] {
            OwnedWindow source(App()->CreateMainWindow()), target(App()->CreateMainWindow());
            auto document = Access::Documents(source.window).Active();
            auto item = Access::Binding(source.window, document->id)->item;
            expect(Access::AllowDrop(target.window, item));
            expect(Access::Transfer(source.window, document->id, target.window, 0));
            expect(Access::Complete(target.window, item));
            expect(Access::Documents(source.window).Active() == document);
            expect(Access::Documents(source.window).Items().size() == 1_u);
            Access::End(source.window);
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
            expect(Access::Transfer(source.window, document->id, target.window, 1));
            Access::RequestNativeClose(source.window);
            Pump([&] { return Access::Closed(source.window); });
            source.window = nullptr;
            for (int i = 0; i < 6; ++i) {
                Access::Tabs(target.window).SelectedItem(Access::Binding(target.window, existing->id)->item);
                expect(Access::Documents(target.window).Active() == existing);
                Access::Tabs(target.window).SelectedItem(Access::Binding(target.window, document->id)->item);
                expect(Access::Documents(target.window).Active() == document);
            }
            auto receiving = Access::TearOutWindow(target.window);
            expect(Access::Transfer(target.window, document->id, receiving, 0));
            OwnedWindow detached(receiving);
            Access::Edit(detached.window, U"moved empty document");
            expect(document->editor->session.TextUtf8() == "moved empty document");
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
            expect(Access::Tabs(source.window).CanTearOutTabs() && Access::Tabs(target.window).CanTearOutTabs());
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
