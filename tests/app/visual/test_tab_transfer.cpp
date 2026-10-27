#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

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
        static auto Binding(Microsoft::UI::Xaml::Window const& window, std::uint64_t id) { return Impl(window)->tabItems.at(id); }
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
        "tabs tear into a native window and retain dirty source undo selection and original tab container"_test = [] {
            OwnedWindow source(App()->CreateMainWindow());
            auto document = Access::Documents(source.window).Active();
            Access::Edit(source.window, U"**原文** $x+1$\n");
            expect(document->session.EnterSourceMode());
            Access::Edit(source.window, U"prefix ");
            auto before = document->session.TextUtf8();
            auto selection = document->session.Selection();
            auto state = document->session.ContentState();
            auto originalTab = Access::Binding(source.window, document->id)->view;
            Access::New(source.window);
            OwnedWindow target(Access::TearOutWindow(source.window));
            expect(Access::Tabs(source.window).CanTearOutTabs() && Access::Tabs(target.window).CanTearOutTabs());
            expect(App()->FindMainWindow(target.window.AppWindow().Id()) == Access::Impl(target.window));
            folia::reset_core_operation_counters();
            expect(Access::Transfer(source.window, document->id, target.window, 0));
            expect(Access::Documents(source.window).Items().size() == 1_u);
            expect(Access::Documents(target.window).Items().size() == 1_u);
            expect(Access::Documents(target.window).Active().get() == document.get());
            expect(Access::Binding(target.window, document->id)->view == originalTab);
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
            auto tabs = Access::Tabs(source.window).TabItems();
            auto view = tabs.GetAt(0);
            tabs.RemoveAt(0); tabs.Append(view);
            Pump([&] { return Access::Documents(source.window).Items().back() == first; });
            expect(Access::Documents(source.window).Items().back() == first);
            expect(Access::Tabs(source.window).TabItems().Size() == 3_u);
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
