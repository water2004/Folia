#include "pch.h"
#include "support/folia_test.hpp"
#include "completion/InlineCompletionController.h"
#include "support/CompletionReply.h"
#include "editor/rendering/EditorContentPreparation.h"
import folia.markdown.instrumentation;
import folia.markdown.block_source;
import folia.markdown.utf;

namespace {
using namespace boost::ut;
using namespace winrt::Folia;
using completion_tests::Reply;
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
    std::vector<CompletionState> states;
    ~Fixture() { controller.reset(); }
    Fixture(std::u32string proposal = U" hello world", bool source = false, CompletionService::Transport transport = {}, std::u32string old = {}) {
        session.SetText(L"abc");
        if (source) session.EnterSourceMode();
        auto owner = session.RenderModel().blocks.front().source_span.container_id;
        session.SetSelection(folia::TextSelection::caret({owner, 3}));
        controller = std::make_shared<InlineCompletionController>(session,
            CompletionConfig{true, "https://fixture.invalid/v1", "fixture", 0},
            winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread(),
            [this](auto const& command) { return Execute(command); }, [] {},
            [this](CompletionState state, auto const& value) { states.push_back(state); if (state == CompletionState::Failed) error = value; },
            [this, proposal = std::move(proposal), old = std::move(old), transport = std::move(transport)](auto const& config, auto const& prompt, auto stop) {
                ++requests; return transport ? transport(config, prompt, stop) : CompletionResult{Reply(prompt, proposal, old)};
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
    "completion context preserves nearby source beyond three neighbors and exact source-mode line breaks"_test = [] {
        for (bool sourceMode : {false, true}) {
            Fixture f;
            f.session.SetText(sourceMode ? L"nearby intent\none\ntwo\nthree\nfour\nx_\nafter"
                : L"nearby intent\n\none\n\ntwo\n\nthree\n\nfour\n\nx_\n\nafter");
            if (sourceMode) f.session.EnterSourceMode();
            auto model = f.session.RenderModel();
            auto owner = model.blocks[model.blocks.size() - 2].source_span.container_id;
            f.session.SetSelection(folia::TextSelection::caret({owner, 2}));
            folia::reset_core_operation_counters();
            auto c = f.session.CaptureCompletionContext(); expect(fatal(c.has_value()));
            expect(c->preceding.find("nearby intent") != std::string::npos);
            expect(c->prefix == "x_");
            if (sourceMode) expect(c->preceding == "nearby intent\none\ntwo\nthree\nfour\n");
            auto counts = folia::read_core_operation_counters();
            expect(counts.full_document_parses == 0_u && counts.full_document_serializations == 0_u);
        }
    };
    "literal completion replies preserve leading newlines and source suffixes through undo"_test = [] {
        for (bool sourceMode : {false, true}) {
            Fixture f(U"\nnext paragraph", sourceMode);
            f.Type(U"x"); expect(f.Wait());
            auto before = f.session.TextUtf8(); auto selection = f.session.Selection();
            expect(f.controller->Accept(false)); f.controller->Cancel();
            expect(f.session.TextUtf8().find("abcx\n") != std::string::npos);
            expect(f.Execute({.kind = folia::CommandKind::Undo}));
            expect(f.session.TextUtf8() == before && f.session.Selection() == selection);
        }
    };
    "literal TeX completion inserts its whole argument and keeps existing closing source"_test = [] {
        struct Case { wchar_t const* text; bool source; };
        for (auto example : {Case{L"$a$", false}, Case{L"$$\na\n$$", false}, Case{L"> $a$", false}, Case{L"$a$", true}}) {
            Fixture f(U"{n}"); f.session.SetText(example.text);
            if (example.source) f.session.EnterSourceMode();
            auto owner = f.session.Selection().active.container_id;
            auto source = *f.session.EditableSource(owner);
            auto at = source.find(U'a'); expect(fatal(at != source.npos));
            f.session.SetSelection(folia::TextSelection::caret({owner, at + 1})); f.controller->Cancel();
            f.Type(U"_"); expect(f.Wait());
            auto before = f.session.TextUtf8(); auto selection = f.session.Selection();
            folia::reset_core_operation_counters();
            expect(f.controller->Accept(false)); f.controller->Cancel();
            auto counts = folia::read_core_operation_counters();
            expect(counts.full_document_parses == 0_u && counts.full_document_serializations == 0_u);
            expect(f.session.TextUtf8().find("a_{n}") != std::string::npos);
            folia::reset_core_operation_counters();
            expect(f.Execute({.kind = folia::CommandKind::Undo}));
            counts = folia::read_core_operation_counters();
            expect(counts.full_document_parses == 0_u && counts.full_document_serializations == 0_u);
            expect(f.session.TextUtf8() == before && f.session.Selection() == selection);
        }
    };
    "typing over a selection schedules AI and collapsed affinities remain a caret"_test = [] {
        for (bool selected : {false, true}) {
            Fixture f;
            auto owner = f.session.Selection().active.container_id;
            f.session.SetSelection({{owner, selected ? 1u : 2u, folia::TextAffinity::Downstream},
                {owner, 2, folia::TextAffinity::Upstream}});
            f.controller->Cancel();
            if (!selected) expect(f.session.CaptureCompletionContext().has_value());
            f.Type(U"x"); expect(f.Wait()); expect(f.requests.load() == 1);
            expect(f.controller->Accept(false));
        }
    };
    "AI edits reuse existing delimiters and preserve exact source and caret through undo"_test = [] {
        for (auto markdown : {L"abc OLD rest", L"$abc OLD rest$", L"$$\nabc OLD rest\n$$", L"```rust\nabc OLD rest\n```"}) {
            Fixture f(U"new", false, {}, U"OLD");
            f.session.SetText(markdown);
            auto owner = f.session.Selection().active.container_id;
            auto text = *f.session.EditableSource(owner);
            const auto at = text.find(U"OLD"); expect(fatal(at != text.npos));
            f.session.SetSelection(folia::TextSelection::caret({owner, at})); f.controller->Cancel();
            f.Type(U"x"); expect(f.Wait());
            const auto original = f.session.TextUtf8(); const auto before = f.session.Selection();
            folia::reset_core_operation_counters();
            expect(f.controller->Accept(false)); f.controller->Cancel();
            auto after = f.session.Selection();
            expect(after.active.source_offset == before.active.source_offset + 3);
            expect(f.Execute({.kind = folia::CommandKind::Undo}));
            expect(f.session.Selection() == before);
            expect(f.Execute({.kind = folia::CommandKind::Redo}));
            expect(f.session.Selection() == after);
            auto counts = folia::read_core_operation_counters();
            expect(counts.full_document_parses == 0_u && counts.full_document_serializations == 0_u);
            expect(f.session.TextUtf8().find("new") != std::string::npos);
            expect(f.session.TextUtf8().find("OLD") == std::string::npos);
            expect(f.Execute({.kind = folia::CommandKind::Undo}));
            expect(f.session.TextUtf8() == original);
        }
        Fixture f(U"a}{b}", false, {}, U"}{}"); f.session.SetText(L"$\\frac{}{}$");
        auto owner = f.session.Selection().active.container_id;
        f.session.SetSelection(folia::TextSelection::caret({owner, 7})); f.controller->Cancel();
        f.Type(U"x"); expect(f.Wait());
        expect(f.controller->Accept(true)); expect(f.controller->Active());
        expect(f.controller->Accept(false)); f.controller->Cancel();
        expect(f.session.TextUtf8().find("\\frac{xa}{b}") != std::string::npos);
    };
    "source mode AI replacement and deletion keep the original caret in history"_test = [] {
        for (auto replacement : {U"new", U""}) {
            Fixture f(replacement, true, {}, U"OLD");
            f.session.SetText(L"abc OLD rest"); f.session.EnterSourceMode();
            auto owner = f.session.Selection().active.container_id;
            f.session.SetSelection(folia::TextSelection::caret({owner, 4})); f.controller->Cancel();
            f.Type(U"x"); expect(f.Wait());
            auto original = f.session.TextUtf8(); auto before = f.session.Selection();
            expect(f.controller->Accept(false)); f.controller->Cancel();
            auto accepted = f.session.TextUtf8(); expect(accepted.find("OLD") == std::string::npos);
            expect(f.Execute({.kind = folia::CommandKind::Undo}));
            expect(f.session.TextUtf8() == original); expect(f.session.Selection() == before);
            expect(f.Execute({.kind = folia::CommandKind::Redo})); expect(f.session.TextUtf8() == accepted);
        }
    };
    "diff preview inserts only new text and marks replaced source without modifying it"_test = [] {
        folia::NodeId owner{7}; DisplayInlineText display;
        AppendSourceText(display, U"abc}{}", {owner, {0, 6}}, folia::InlineStyle::plain(), false);
        display.displayToSource.push_back({owner, 6, folia::TextAffinity::Downstream});
        InsertCompletionPreview(display, {owner, 3}, U"a}{b}", U"}{}");
        expect(bool(display.text == U"abca}{b}"));
        expect(std::ranges::count_if(display.ranges, [](auto const& r) { return r.completion; }) == 2);
        DisplayInlineText changed;
        AppendSourceText(changed, U"OLD", {owner, {0, 3}}, folia::InlineStyle::plain(), false);
        changed.displayToSource.push_back({owner, 3, folia::TextAffinity::Downstream});
        InsertCompletionPreview(changed, {owner, 0}, U"new", U"OLD");
        expect(bool(changed.text == U"newOLD"));
        expect(std::ranges::any_of(changed.ranges, [](auto const& r) { return r.completion && r.style.strikethrough; }));
    };
    "completion activity distinguishes suggestions empty output failures and token limits"_test = [] {
        for (auto terminal : {CompletionState::Ready, CompletionState::Empty, CompletionState::Failed, CompletionState::BudgetExhausted}) {
            Fixture f({}, false, [terminal](auto const&, auto const& prompt, auto) {
                CompletionResult result;
                if (terminal == CompletionState::Ready) result.text = Reply(prompt, U"next");
                if (terminal == CompletionState::Failed) result.error = "fixture failure";
                result.truncated = terminal == CompletionState::BudgetExhausted;
                return result;
            });
            f.Type(U"x");
            expect(Pump([&] { return !f.states.empty() && f.states.back() == terminal; }));
            expect(std::ranges::find(f.states, CompletionState::Requesting) != f.states.end());
            expect(f.controller->Active() == (terminal == CompletionState::Ready));
            expect(f.requests.load() == 1_i);
            f.controller->SetFocused(false); expect(f.states.back() == CompletionState::Idle);
        }
    };
    "completion cancellation stops activity and late responses cannot restore it"_test = [] {
        std::atomic<bool> cancelled = false;
        Fixture f({}, false, [&](auto const&, auto const& prompt, std::stop_token stop) {
            std::mutex mutex; std::unique_lock lock(mutex); std::condition_variable_any wake;
            wake.wait(lock, stop, [] { return false; }); cancelled = true;
            return CompletionResult{Reply(prompt, U"obsolete")};
        });
        f.Type(U"x"); expect(Pump([&] { return f.requests.load() == 1; }));
        expect(!f.states.empty() && f.states.back() == CompletionState::Requesting);
        f.controller->SetFocused(false); expect(f.states.back() == CompletionState::Idle);
        expect(Pump([&] { return cancelled.load(); }));
        expect(!f.controller->Active()); expect(f.states.back() == CompletionState::Idle);
    };
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
    "inline completion leaves an editable code body after accepting an opening fence"_test = [] {
        Fixture f(U"\n\n```markdown\n"); f.Type(U"s"); expect(f.Wait());
        auto original = f.session.TextUtf8(); auto before = f.session.Selection();
        expect(f.controller->Accept(false)); f.controller->Cancel();
        auto accepted = f.session.TextUtf8(); auto after = f.session.Selection();
        auto frame = f.session.RenderFrame();
        if (frame.materializeBlocks) frame.materializeBlocks(0, frame.renderModel.blocks.size());
        auto const& block = frame.renderModel.blocks.back();
        expect(block.kind == folia::RenderBlockKind::Code);
        expect(after.active.container_id == block.source_span.container_id);
        expect(block.special().language == std::optional<std::string>{"markdown"});
        expect(block.special().code_text.empty());
        expect(after.active.source_offset == block.content_span.source_range.end);
        f.Type(U"x"); f.controller->Cancel();
        auto typed = f.session.RenderFrame();
        if (typed.materializeBlocks) typed.materializeBlocks(0, typed.renderModel.blocks.size());
        expect(typed.renderModel.blocks.back().special().language == std::optional<std::string>{"markdown"});
        expect(typed.renderModel.blocks.back().special().code_text == U"x");
        expect(f.session.TextUtf8().find("```markdown\nx") != std::string::npos);
        expect(f.Execute({.kind = folia::CommandKind::Undo}));
        expect(f.session.TextUtf8() == accepted); expect(f.session.Selection() == after);
        expect(f.Execute({.kind = folia::CommandKind::Undo}));
        expect(f.session.TextUtf8() == original); expect(f.session.Selection() == before);
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
        if (context) expect(context->following.find("nested") != std::string::npos);
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
        auto firstOwner = source.RenderModel().blocks.front().source_span.container_id;
        source.SetSelection(folia::TextSelection::caret({firstOwner, 2}));
        auto following = source.CaptureCompletionContext(); expect(following.has_value());
        if (following) expect(following->following.find("second") != std::string::npos && following->following.find("third") != std::string::npos);
    };
};
}
