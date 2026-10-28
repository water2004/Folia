#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include <dwmapi.h>
#include <winrt/Microsoft.UI.Content.h>

import folia.markdown.instrumentation;

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
        static bool ContainsDragPoint(Microsoft::UI::Xaml::Window const& window, Windows::Graphics::PointInt32 point) {
            return Impl(window)->ContainsTabDragPoint(point);
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
        static void Render(Microsoft::UI::Xaml::Window const& window) { Impl(window)->RenderEditorSurface(); }
        static void Open(Microsoft::UI::Xaml::Window const& window, std::filesystem::path path) {
            Impl(window)->OpenWorkspaceFile(std::move(path), winrt::Folia::WorkspaceOpenMode::Keep);
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
    suite tab_transfer_tests = [] {
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
                expect(document->session.TextUtf8() == "same document");
            }
            Access::End(source.window);
            expect(!Access::Complete(target.window, item));
            Access::Edit(target.window, U" continues");
            expect(document->session.TextUtf8() == "same document continues");
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
            expect(document->session.TextUtf8() == "original empty document");
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
            expect(document->session.TextUtf8() == "moved empty document");
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
            expect(document->session.EnterSourceMode());
            Access::Edit(source.window, U"prefix ");
            auto before = document->session.TextUtf8();
            auto selection = document->session.Selection();
            auto state = document->session.ContentState();
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
            expect(document->session.TextUtf8() == before && document->session.Selection() == selection);
            expect(document->session.ContentState() == state && document->controller.IsModified() && document->session.IsSourceMode());
            auto firstCounters = folia::read_core_operation_counters();
            expect(firstCounters.full_document_parses == 0_u && firstCounters.full_document_serializations == 0_u)
                << "moving an existing session rebuilt its source";
            for (int i = 0; i < 4; ++i) {
                expect(Access::Transfer(target.window, document->id, source.window, 0));
                expect(Access::Transfer(source.window, document->id, target.window, 0));
            }
            // Later round trips create new empty placeholders when a window
            // loses its final tab. Their initialization is not a document edit.
            expect(document->session.TextUtf8() == before) << "source changed on repeated transfer";
            expect(document->session.Selection() == selection) << "selection changed on repeated transfer";
            expect(document->session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            expect(document->session.ExecuteCommand(folia::Command{folia::CommandKind::Redo}));
            expect(document->session.TextUtf8() == before) << "source changed after transfer undo/redo";
            expect(document->session.Selection() == selection) << "selection changed after transfer undo/redo";
            if (document->session.Selection() != selection) {
                auto now = document->session.Selection();
                std::fprintf(stderr, "source selection before=%llu:%zu/%d,%llu:%zu/%d after=%llu:%zu/%d,%llu:%zu/%d\n",
                    selection.anchor.container_id.v, selection.anchor.source_offset, static_cast<int>(selection.anchor.affinity),
                    selection.active.container_id.v, selection.active.source_offset, static_cast<int>(selection.active.affinity),
                    now.anchor.container_id.v, now.anchor.source_offset, static_cast<int>(now.anchor.affinity),
                    now.active.container_id.v, now.active.source_offset, static_cast<int>(now.active.affinity));
            }
            Access::Edit(target.window, U"target ");
            expect(document->controller.IsModified());
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
            expect(placeholder != document && !placeholder->controller.IsModified());
            expect(Access::Transfer(target.window, document->id, source.window, 0));
            expect(Access::Documents(source.window).Items().size() == 1_u);
            expect(Access::Documents(source.window).Active() == document);
            expect(document->session.TextUtf8() == "unsaved last tab" && document->controller.IsModified());
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
            Pump([&] { return Access::Documents(source.window).Active()->loaded && Access::Documents(target.window).Active()->loaded
                && Access::Documents(source.window).Active()->session.HasFile() && Access::Documents(target.window).Active()->session.HasFile(); });
            auto a = Access::Documents(source.window).Active();
            auto b = Access::Documents(target.window).Active();
            Access::Edit(source.window, U"first edit"); Access::Edit(target.window, U"second edit");
            auto aText = a->session.TextUtf8(), bText = b->session.TextUtf8();
            expect(!Access::Transfer(source.window, a->id, target.window, 0));
            expect(Access::Documents(source.window).Active() == a && Access::Documents(target.window).Active() == b);
            expect(a->session.TextUtf8() == aText) << "first text=" << a->session.TextUtf8();
            expect(b->session.TextUtf8() == bText) << "second text=" << b->session.TextUtf8();
            expect(a->controller.IsModified()) << "first baseline lost";
            expect(b->controller.IsModified()) << "second baseline lost";
        };
        "tab transfer restores the visible source anchor and its viewport inset in the receiving window"_test = [] {
            TestDirectory directory;
            auto path = directory.path / L"long-document.md";
            std::ofstream text(path, std::ios::binary);
            for (int i=0;i<500;++i) text << "Paragraph " << i << " keeps its stable block position while moving between windows.\n\n";
            text.close();
            OwnedWindow source(App()->CreateMainWindow());
            Access::Open(source.window,path);
            Pump([&] { return Access::Documents(source.window).Active()->loaded && Access::Documents(source.window).Active()->session.HasFile(); });
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
            auto selection=document->session.Selection();
            expect(Access::Transfer(source.window,document->id,target.window,0));
            auto settled=std::chrono::steady_clock::now()+std::chrono::milliseconds(200);
            Pump([&] { return std::chrono::steady_clock::now()>=settled; });
            auto& receiving=Access::Renderer(target.window);
            auto restored=receiving.CaretBounds(*anchor);
            expect(fatal(restored.has_value()));
            expect(std::abs(restored->top-bounds->top)<2.f) << "visible block shifted after transfer";
            expect(receiving.ScrollOffset()>100.f);
            expect(document->session.Selection()==selection);
            expect(!document->controller.IsModified());
        };
    };
}
