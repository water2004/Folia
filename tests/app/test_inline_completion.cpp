#include "pch.h"
#include "support/folia_test.hpp"
#include "completion/InlineCompletionController.h"
#include "editor/rendering/EditorContentPreparation.h"
import folia.markdown.instrumentation;

namespace {
using namespace boost::ut;
using namespace winrt::Folia;
bool Pump(std::function<bool()> done, std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
    auto end = std::chrono::steady_clock::now() + timeout;
    while (!done() && std::chrono::steady_clock::now() < end) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
        MsgWaitForMultipleObjectsEx(0, nullptr, 2, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
    return done();
}
struct Fixture {
    EditorSession session;
    std::shared_ptr<InlineCompletionController> controller;
    std::atomic<int> requests = 0;
    std::string error;
    Fixture(std::u32string proposal = U" hello world", bool source = false) {
        session.SetText(L"abc");
        if (source) session.EnterSourceMode();
        auto owner = session.RenderModel().blocks.front().source_span.container_id;
        session.SetSelection(folia::TextSelection::caret({owner, 3}));
        controller = std::make_shared<InlineCompletionController>(session,
            CompletionConfig{true, "https://fixture.invalid/complete", "fixture", 0},
            winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread(),
            [this](auto const& command) { return Execute(command); }, [] {},
            [this](auto const& value) { error = value; },
            [this, proposal = std::move(proposal)](auto const&, auto const&, auto stop) {
                ++requests; return CompletionResult{proposal};
            });
        controller->SetFocused(true);
    }
    bool Execute(folia::Command const& command) {
        auto before = session.Selection();
        if (!session.ExecuteCommand(command)) return false;
        if (controller) controller->Edited(before, command);
        return true;
    }
    void Type(std::u32string text) {
        auto command = folia::Command::InsertText(std::move(text)); command.typed_input = true;
        expect(Execute(command));
    }
    bool Wait() { return Pump([&] { return controller->Active() || !error.empty(); }); }
};
suite inline_completion_tests = [] {
    "completion worker replaces obsolete requests without delivering cancelled results"_test = [] {
        struct Shared {
            std::atomic<int> started = 0, active = 0, maximum = 0;
            std::mutex mutex; std::condition_variable_any wake;
        };
        auto shared = std::make_shared<Shared>();
        CompletionService service([shared](auto const& config, auto const&, std::stop_token stop) {
            auto active = ++shared->active;
            shared->maximum.store((std::max)(shared->maximum.load(), active));
            ++shared->started;
            if (config.model == "old") {
                std::unique_lock lock(shared->mutex);
                shared->wake.wait(lock, stop, [] { return false; });
            }
            --shared->active; return CompletionResult{config.model == "old" ? U"obsolete" : U"current"};
        });
        std::atomic<int> oldDeliveries = 0, newDeliveries = 0;
        auto prompt = folia::completion::make_prompt({});
        service.Submit({true, {}, "old", 0}, prompt, [&](auto) { ++oldDeliveries; });
        expect(Pump([&] { return shared->started.load() == 1; }));
        service.Submit({true, {}, "new", 0}, prompt, [&](auto result) {
            if (result.text == U"current") ++newDeliveries;
        });
        expect(Pump([&] { return newDeliveries.load() == 1; }));
        expect(oldDeliveries.load() == 0_i); expect(shared->maximum.load() == 1_i);
    };
    "inline completion debounces typing and partially accepts through ordinary undo transactions"_test = [] {
        Fixture f;
        f.Type(U"x"); f.Type(U"y"); f.Type(U"z");
        expect(f.requests.load() == 0_i);
        expect(f.Wait()); expect(f.error.empty()); expect(f.requests.load() == 1_i);
        auto original = f.session.TextUtf8(); auto selection = f.session.Selection();
        folia::reset_core_operation_counters();
        expect(f.controller->Accept(true));
        expect(f.session.SelectedSource().empty());
        expect(f.session.TextUtf8().starts_with("abcxyz hello"));
        expect(f.controller->Active()); expect(f.requests.load() == 1_i);
        expect(f.controller->Accept(false));
        expect(f.session.TextUtf8().starts_with("abcxyz hello world"));
        f.controller->Cancel();
        expect(f.Execute({.kind = folia::CommandKind::Undo}));
        expect(f.Execute({.kind = folia::CommandKind::Undo}));
        expect(f.session.TextUtf8() == original); expect(f.session.Selection() == selection);
        // Serialization above is an explicit test observation, not an edit path.
        folia::reset_core_operation_counters();
        expect(f.Execute({.kind = folia::CommandKind::Redo}));
        auto counts = folia::read_core_operation_counters();
        expect(counts.full_document_parses == 0_u && counts.full_document_serializations == 0_u);
    };
    "inline completion typed prefix reuses proposal and focus loss retires it"_test = [] {
        Fixture f; f.Type(U"x"); expect(f.Wait());
        f.Type(U" hello");
        expect(f.controller->Active()); expect(f.requests.load() == 1_i);
        f.controller->SetFocused(false);
        expect(!f.controller->Active());
        f.controller->SetFocused(true); f.Type(U"q");
        f.controller->CompositionChanged(true);
        expect(!Pump([&] { return f.requests.load() > 1; }, std::chrono::milliseconds(550)));
        f.controller->CompositionChanged(false); expect(f.Wait()); expect(f.requests.load() == 2_i);
    };
    "inline completion ignores late results after selection change"_test = [] {
        Fixture f; f.Type(U"x");
        expect(Pump([&] { return f.requests.load() == 1; }));
        f.session.SetSelection(folia::TextSelection::caret({f.session.Selection().active.container_id, 0}));
        f.controller->Validate();
        expect(!Pump([&] { return f.controller->Active(); }, std::chrono::milliseconds(100)));
    };
    "inline completion reuses cached context after an edit is removed"_test = [] {
        Fixture f; f.Type(U"x"); expect(f.Wait());
        f.Type(U"q"); expect(!f.controller->Active());
        expect(f.Execute({.kind = folia::CommandKind::DeleteBackward}));
        expect(f.Wait()); expect(f.requests.load() == 1_i); f.controller->Cancel();
    };
    "inline completion retires typed proposals when semantic pairing changes following source"_test = [] {
        Fixture f(U"(x)"); f.session.SetText(L"$a$"); f.controller->Cancel();
        auto owner = f.session.RenderModel().blocks.front().source_span.container_id;
        f.session.SetSelection(folia::TextSelection::caret({owner, 2}));
        f.Type(U"x"); expect(f.Wait());
        f.Type(U"("); expect(!f.controller->Active());
        expect(f.session.TextUtf8().find("()") != std::string::npos); f.controller->Cancel();
    };
    "inline completion accepts multiline Markdown in source view with exact undo"_test = [] {
        Fixture f(U" one\nsecond", true); f.Type(U"x"); expect(f.Wait());
        auto original = f.session.TextUtf8(); auto selection = f.session.Selection();
        expect(f.controller->Accept(false)); f.controller->Cancel();
        expect(f.session.TextUtf8().find("one\nsecond") != std::string::npos);
        expect(f.Execute({.kind = folia::CommandKind::Undo}));
        expect(f.session.TextUtf8() == original); expect(f.session.Selection() == selection);
    };
    "inline completion accepts Unicode words without splitting emoji or combining sequences"_test = [] {
        Fixture f(U" 世界 👩‍💻 e\u0301"); f.Type(U"x"); expect(f.Wait());
        expect(f.controller->Accept(true)); expect(f.session.TextUtf8().find(" 世界") != std::string::npos);
        expect(f.controller->Accept(true)); expect(f.session.TextUtf8().find(" 👩‍💻") != std::string::npos);
        expect(f.controller->Accept(true)); expect(f.session.TextUtf8().find(" e\xcc\x81") != std::string::npos);
        expect(f.requests.load() == 1_i); f.controller->Cancel();
    };
    "completion display insertion wraps in flow without editing source mappings"_test = [] {
        DisplayInlineText display; folia::NodeId owner{7};
        auto style = folia::InlineStyle::plain(); style.bold = true;
        AppendSourceText(display, U"a\U0001f642b", {owner, {0, 3}}, style, false);
        display.displayToSource.push_back({owner, 3, folia::TextAffinity::Downstream});
        auto caret = folia::TextPosition{owner, 2};
        InsertCompletionPreview(display, caret, U" next\nline");
        expect(display.text == U"a\U0001f642 next\nlineb");
        expect(display.displayToSource.size() == 15_u);
        expect(folia::platform::editor::DisplayPositionForSource(display.displayToSource, caret) == 3_u);
        expect(display.displayToSource.back().source_offset == 3_u);
        auto ghost = std::ranges::find_if(display.ranges, [](auto const& r) { return r.completion; });
        expect(ghost != display.ranges.end());
        if (ghost != display.ranges.end()) expect(!ghost->style.bold);
    };
    "completion capture uses block views and never serializes the document"_test = [] {
        EditorSession session; session.SetText(L"first\n\n> nested **value**\n");
        folia::reset_core_operation_counters();
        auto context = session.CaptureCompletionContext();
        auto counts = folia::read_core_operation_counters();
        expect(context.has_value());
        expect(counts.full_document_parses == 0_u && counts.full_document_serializations == 0_u);
        auto position = session.Selection().active, anchor = position;
        anchor.source_offset = 3; session.SetSelection(anchor, position);
        expect(!session.CaptureCompletionContext());
        EditorSession source; source.SetText(L"first\nsecond\nthird"); source.EnterSourceMode();
        auto owner = source.RenderModel().blocks.back().source_span.container_id;
        source.SetSelection(folia::TextSelection::caret({owner, 2}));
        auto sourceContext = source.CaptureCompletionContext(); expect(sourceContext.has_value());
        if (sourceContext) {
            expect(sourceContext->prefix == "th");
            expect(sourceContext->preceding.find("first") != std::string::npos);
            expect(sourceContext->preceding.find("second") != std::string::npos);
        }
    };
};
}
