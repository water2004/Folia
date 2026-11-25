#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "windows/media/images/ImageSource.h"
#include "workspace/WorkspaceIcons.h"
#include <dwmapi.h>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>

import folia.markdown.instrumentation;

namespace winrt::Folia {
    struct EditorRendererTestAccess {
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
            impl->SetSettingsMode(true);
            auto nav = impl->settingsView->Root();
            for (auto item : nav.MenuItems()) {
                auto entry = item.as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
                if (winrt::unbox_value<winrt::hstring>(entry.Tag()) == L"about") nav.SelectedItem(entry);
            }
            window.Content().as<Microsoft::UI::Xaml::FrameworkElement>().UpdateLayout();
            return nav;
        }
        static bool ContainsDragPoint(Microsoft::UI::Xaml::Window const& window, Windows::Graphics::PointInt32 point) {
            return Impl(window)->ContainsTabDragPoint(point);
        }
        static void PointerBegin(Microsoft::UI::Xaml::Window const& window, Windows::Graphics::PointInt32 point) { Impl(window)->BeginTabPointerGesture(point); }
        static void PointerEnd(Microsoft::UI::Xaml::Window const& window) { Impl(window)->EndTabPointerGesture(); }
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
        static void Bars(Microsoft::UI::Xaml::Window const& window, WorkspaceBarSettings settings) {
            Impl(window)->UpdateBarSettings(settings);
        }
        static void Hover(Microsoft::UI::Xaml::Window const& window, Windows::Foundation::Point point) {
            Impl(window)->topBar.OnPointer(point);
            Impl(window)->bottomBar.OnPointer(point);
        }
        static auto OpenFileMenu(Microsoft::UI::Xaml::Window const& window) {
            auto button = Impl(window)->MainMenuButton();
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
        "workspace edge bars reveal over a stationary viewport and follow their own settings"_test = [] {
            OwnedWindow current(App()->CreateMainWindow());
            auto window = current.window;
            auto root = window.Content().as<Controls::Grid>();
            root.UpdateLayout();
            auto workspace = root.FindName(L"WorkspaceHost").as<FrameworkElement>();
            auto top = root.FindName(L"TopChrome").as<FrameworkElement>();
            auto bottom = root.FindName(L"StatusBar").as<FrameworkElement>();
            auto offset = [](auto bar) { return bar.RenderTransform().template as<Media::TranslateTransform>().Y(); };
            auto move = [&](float x, float y) {
                Access::Hover(window, {x, y});
            };
            auto waitForOffset = [&](FrameworkElement const& bar, double expected, char const* step) {
                try { Pump([&] { return std::abs(offset(bar) - expected) < .01; }); }
                catch (std::runtime_error const&) {
                    throw std::runtime_error(std::format("{}: offset {}, expected {}", step, offset(bar), expected));
                }
            };
            auto middle = static_cast<float>(root.ActualWidth() * .5);
            move(middle, 100);
            Access::Bars(window, {true, false});
            root.UpdateLayout();
            expect(offset(top) <= -top.ActualHeight());
            expect(offset(bottom) == 0.);
            expect(workspace.ActualHeight() == root.ActualHeight() - bottom.ActualHeight());
            Access::Bars(window, {true, true});
            root.UpdateLayout();
            const auto height = workspace.ActualHeight();
            expect(height == root.ActualHeight());
            expect(offset(bottom) >= bottom.ActualHeight());

            move(middle, 6);
            waitForOffset(top, 0., "hovering at the top edge");
            expect(window.AppWindow().TitleBar().PreferredHeightOption() == winrt::Microsoft::UI::Windowing::TitleBarHeightOption::Standard);
            expect(workspace.ActualHeight() == height);
            auto menu = Access::OpenFileMenu(window);
            move(middle, 120);
            const auto afterDelay = std::chrono::steady_clock::now() + std::chrono::milliseconds(550);
            Pump([&] { return std::chrono::steady_clock::now() > afterDelay; });
            expect(offset(top) == 0.);
            bool closed = false;
            auto closedEvent = menu.Closed(winrt::auto_revoke, [&](auto const&, auto const&) { closed = true; });
            menu.Hide();
            Pump([&] { return closed; });
            root.FindName(L"EditorSurface").as<Controls::SwapChainPanel>().Focus(FocusState::Programmatic);
            move(middle, 120);
            waitForOffset(top, -top.ActualHeight(), "leaving the top menu");

            move(middle, static_cast<float>(root.ActualHeight() - 3));
            waitForOffset(bottom, 0., "hovering at the bottom edge");
            expect(workspace.ActualHeight() == height);
            move(middle, 120);
            waitForOffset(bottom, bottom.ActualHeight(), "leaving the bottom bar");
            Access::Bars(window, {false, false});
            root.UpdateLayout();
            expect(workspace.ActualHeight() == height - top.ActualHeight() - bottom.ActualHeight());
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
                expect(Access::Documents(source.window).Active() == document);
                expect(tabs.CanTearOutTabs());
                // Selection can route the release to XAML instead. A new native
                // move must use its own pointer origin even without PointerEnd.
                Access::PointerBegin(source.window, point(root.FindName(L"TitleDragArea").as<FrameworkElement>()));
                expect(!tabs.CanTearOutTabs()) << "a previous tab press cannot own a later caption move";
                expect(Access::Documents(source.window).Items().size() == 2_u);
                Access::PointerEnd(source.window);
                Pump([&] { return tabs.CanTearOutTabs(); });
            }
        };
        "caption gestures after tab selection cannot enter the native tab tear out loop"_test = [] {
            OwnedWindow source(App()->CreateMainWindow());
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
            std::vector<double> opacityDuringPresentation;
            auto token = count.RegisterPropertyChangedCallback(Controls::TextBlock::TextProperty(), [&](auto const&, auto const&) {
                auto tab = tabs.ContainerFromItem(tabs.SelectedItem()).as<Controls::TabViewItem>();
                opacityDuringPresentation.push_back(TabPart(tab, L"SelectionIndicator").Opacity());
            });
            for (auto document : {first, second, first, second}) tabs.SelectedItem(Access::Binding(source.window, document->id)->item);
            count.UnregisterPropertyChangedCallback(Controls::TextBlock::TextProperty(), token);
            expect(presentCount() == before) << "switching must not submit intermediate document buffers";
            expect(Access::Documents(source.window).Active() == second);
            expect(opacityDuringPresentation.size() == 4_u);
            for (auto opacity : opacityDuringPresentation) expect(opacity == 1.) << "blue indicator during document update";
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
            auto header = Access::View(owned.window, id).Header().as<Controls::StackPanel>();
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
