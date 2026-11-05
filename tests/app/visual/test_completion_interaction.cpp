#include "pch.h"
#include "support/folia_test.hpp"
#include "../support/DocumentControllerFixture.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"
#include "../support/CompletionReply.h"

import folia.markdown.utf;
import folia.markdown.instrumentation;

namespace winrt::Folia::implementation {
    struct CompletionWindowTestAccess {
        static auto Impl(Microsoft::UI::Xaml::Window const& window) {
            return get_self<MainWindow>(window.as<winrt::Folia::MainWindow>());
        }
        static auto& Session(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->editorSession(); }
        static auto& AI(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->inlineCompletion; }
        static auto& Latex(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->latexCompletionController; }
        static auto& Keyboard(Microsoft::UI::Xaml::Window const& window) { return Impl(window)->keyboardController; }
        static void Configure(Microsoft::UI::Xaml::Window const& window, CompletionService::Transport transport) {
            auto impl = Impl(window);
            impl->appSettings.completion = CompletionConfig{true, "https://fixture.invalid/v1", "fixture", 0};
            impl->latexCompletionController.SetEnabled(true);
            impl->ConfigureInlineCompletion(std::move(transport));
            impl->inlineCompletion->SetFocused(true);
        }
        static void Type(Microsoft::UI::Xaml::Window const& window, std::u32string text) {
            Impl(window)->EditorSurface().Focus(Microsoft::UI::Xaml::FocusState::Programmatic);
            AI(window)->SetFocused(true);
            auto command = folia::Command::InsertText(std::move(text)); command.typed_input = true;
            if (!Impl(window)->ExecuteEditorCommand(command)) throw std::runtime_error("test input failed");
            Latex(window).Refresh();
        }
        static bool Key(Microsoft::UI::Xaml::Window const& window, folia::platform::editor::EditorKeyGesture key) {
            return Impl(window)->HandleCompletionKey(key);
        }
        static void Render(Microsoft::UI::Xaml::Window const& window) { Impl(window)->RenderEditorSurface(); }
        static bool Execute(Microsoft::UI::Xaml::Window const& window, folia::Command command) {
            return Impl(window)->ExecuteEditorCommand(command);
        }
        static void Close(Microsoft::UI::Xaml::Window const& window) {
            Impl(window)->closeApproved = true; window.Close();
        }
    };
}

namespace {
    using namespace boost::ut;
    using namespace document_tests;
    using namespace winrt::Folia;
    using namespace winrt::Microsoft::UI::Xaml;
    using namespace folia::platform::editor;
    using Access = winrt::Folia::implementation::CompletionWindowTestAccess;

    using completion_tests::Reply;
    struct CompletionWindow {
        Window window = winrt::Folia::implementation::App::Current()->CreateMainWindow();
        std::shared_ptr<std::atomic<int>> requests = std::make_shared<std::atomic<int>>(0);
        explicit CompletionWindow(std::u32string text = U"ac{a}{b}", std::u32string old = {}) {
            window.Activate();
            Pump([&] { return window.Content().as<FrameworkElement>().IsLoaded(); });
            Access::Configure(window, [count = requests, text = std::move(text), old = std::move(old)](auto const&, auto const& prompt, auto) {
                ++*count; return CompletionResult{Reply(prompt, text, old)};
            });
        }
        ~CompletionWindow() { Access::Close(window); }
        auto& Session() { return Access::Session(window); }
        void Begin(std::wstring_view markdown = L"$\\f$") {
            Session().SetText(winrt::hstring{markdown});
            auto caret = Session().Selection().active;
            auto source = Session().EditableSource(caret.container_id);
            if (!source) throw std::runtime_error("missing test source");
            const auto at = source->find(U"\\f");
            if (at == source->npos) throw std::runtime_error("missing test command");
            caret.source_offset = at + 2;
            auto anchor = caret; anchor.affinity = folia::TextAffinity::Downstream;
            caret.affinity = folia::TextAffinity::Upstream;
            Session().SetSelection({anchor, caret});
            expect(Session().LatexCompletionSourceAtCaret().has_value());
            Access::AI(window)->Cancel();
            Access::Render(window);
            Access::Type(window, U"r");
        }
        void Ready() {
            try { Pump([&] { return Access::AI(window)->Active(); }); }
            catch (...) {
                auto selection = Session().Selection();
                auto status = window.Content().as<FrameworkElement>().FindName(L"CompletionStatusText").as<Controls::TextBlock>().Text();
                std::cerr << "AI fixture requests=" << requests->load() << ", selection="
                    << selection.anchor.source_offset << '/' << int(selection.anchor.affinity) << ".."
                    << selection.active.source_offset << '/' << int(selection.active.affinity)
                    << ", status=" << winrt::to_string(status) << ", source=" << Session().TextUtf8() << '\n';
                throw;
            }
        }
        auto Hint() {
            auto value = window.Content().as<FrameworkElement>().FindName(L"LatexSuggestionHint")
                .as<Controls::TextBlock>().Text();
            return std::wstring{value.c_str(), value.size()};
        }
    };

    suite completion_interaction = [] {
        "automatic TeX lists allow AI requests preview and Tab acceptance in math containers"_test = [] {
            for (auto markdown : {L"$\\f$", L"$$\n\\f\n$$", L"> $\\f$"}) {
                CompletionWindow f; f.Begin(markdown);
                expect(Access::Latex(f.window).Active());
                f.Ready();
                expect(f.requests->load() == 1);
                expect(Access::Latex(f.window).Active());
                auto before = f.Session().TextUtf8(); auto selection = f.Session().Selection();
                expect(f.Hint().find(L"Tab") != std::wstring::npos);
                expect(f.Hint().find(L"AI") != std::wstring::npos);
                folia::reset_core_operation_counters();
                expect(Access::Key(f.window, {.key = EditorKey::Tab}));
                expect(!Access::Latex(f.window).Active());
                expect(!Access::AI(f.window)->Active());
                expect(f.Session().EditableSource(selection.active.container_id)->find(U"\\frac{a}{b}") != std::u32string::npos);
                expect(Access::Execute(f.window, {.kind = folia::CommandKind::Undo}));
                auto counts = folia::read_core_operation_counters();
                expect(counts.full_document_parses == 0_u && counts.full_document_serializations == 0_u);
                expect(f.Session().TextUtf8() == before);
                expect(f.Session().Selection() == selection);
            }
        };
        "Enter and arrow keys keep TeX selection available beside visible AI"_test = [] {
            CompletionWindow f(U"AI_ONLY"); f.Begin(); f.Ready();
            auto before = f.Session().TextUtf8();
            expect(Access::Key(f.window, {.key = EditorKey::Down}));
            expect(Access::AI(f.window)->Active());
            expect(Access::Key(f.window, {.key = EditorKey::Up}));
            expect(Access::Key(f.window, {.key = EditorKey::Enter}));
            expect(!Access::AI(f.window)->Active());
            expect(!Access::Latex(f.window).Active());
            expect(f.Session().TextUtf8() != before);
            expect(f.Session().TextUtf8().find("AI_ONLY") == std::string::npos);
            expect(Access::Execute(f.window, {.kind = folia::CommandKind::Undo}));
            expect(f.Session().TextUtf8() == before);
        };
        "Ctrl Right accepts AI by word despite an open TeX list"_test = [] {
            CompletionWindow f(U"ac alpha beta"); f.Begin(); f.Ready();
            expect(Access::Key(f.window, {.key = EditorKey::Right, .control = true}));
            expect(Access::AI(f.window)->Active());
            expect(!Access::Latex(f.window).Active());
            expect(f.Session().TextUtf8().find("alpha") == std::string::npos);
            expect(Access::Key(f.window, {.key = EditorKey::Tab}));
            expect(f.Session().TextUtf8().find("\\frac alpha beta") != std::string::npos);
        };
        "Escape dismisses AI first and leaves TeX Tab acceptance available"_test = [] {
            CompletionWindow f(U"AI_ONLY"); f.Begin(); f.Ready();
            auto before = f.Session().TextUtf8();
            expect(Access::Key(f.window, {.key = EditorKey::Escape}));
            expect(!Access::AI(f.window)->Active());
            expect(Access::Latex(f.window).Active());
            expect(f.Session().TextUtf8() == before);
            expect(f.Hint().find(L"AI") == std::wstring::npos);
            expect(Access::Key(f.window, {.key = EditorKey::Tab}));
            expect(f.Session().TextUtf8() != before);
            expect(f.Session().TextUtf8().find("AI_ONLY") == std::string::npos);
        };
        "AI fills a snippet argument without preventing the next parameter jump"_test = [] {
            CompletionWindow f(U"+1");
            f.Session().SetText(L"$x$");
            auto owner = f.Session().Selection().active.container_id;
            f.Session().SetSelection(folia::TextSelection::caret({owner, 1}));
            Access::AI(f.window)->Cancel();
            expect(Access::Keyboard(f.window).InsertSnippetReplacing(owner, {1, 2}, U"\\frac{${1:a}}{${2:b}}$0"));
            Access::Type(f.window, U"x");
            expect(Access::Keyboard(f.window).HasSnippetSession());
            f.Ready();
            expect(Access::Key(f.window, {.key = EditorKey::Tab}));
            expect(f.Session().TextUtf8() == "$\\frac{x+1}{b}$");
            expect(Access::Key(f.window, {.key = EditorKey::Tab}));
            expect(bool(f.Session().SelectedSource() == U"b"));
            expect(Access::Key(f.window, {.key = EditorKey::Tab, .shift = true}));
            expect(!Access::AI(f.window)->Active());
        };
        "AI edits crossing snippet arguments retire their obsolete tab stops"_test = [] {
            CompletionWindow f(U"+1}{c}", U"}{b}");
            f.Session().SetText(L"$x$");
            auto owner = f.Session().Selection().active.container_id;
            f.Session().SetSelection(folia::TextSelection::caret({owner, 1}));
            Access::AI(f.window)->Cancel();
            expect(Access::Keyboard(f.window).InsertSnippetReplacing(owner, {1, 2}, U"\\frac{${1:a}}{${2:b}}$0"));
            Access::Type(f.window, U"x"); f.Ready();
            expect(Access::Key(f.window, {.key = EditorKey::Tab}));
            expect(f.Session().TextUtf8() == "$\\frac{x+1}{c}$");
            expect(!Access::Keyboard(f.window).HasSnippetSession());
            expect(f.Session().Selection().active.source_offset <= f.Session().EditableSource(owner)->size());
        };
    };
}
