#include "pch.h"
#include "completion/InlineCompletionController.h"
#include <unicode/ubrk.h>
#include "editor/rendering/EditorContentPreparation.h"
#include "settings/CompletionPromptSettings.h"
import folia.markdown.utf;

namespace winrt::Folia {
namespace {
std::size_t nextWord(std::u32string_view text) {
    auto wide = ToWide(text);
    UErrorCode error = U_ZERO_ERROR;
    std::unique_ptr<UBreakIterator, decltype(&ubrk_close)> iterator(
        ubrk_open(UBRK_WORD, "", reinterpret_cast<UChar const*>(wide.data()), static_cast<int32_t>(wide.size()), &error), &ubrk_close);
    if (U_FAILURE(error)) return text.size();
    auto boundary = ubrk_first(iterator.get());
    // Leading whitespace belongs to the next word, punctuation is a segment.
    while ((boundary = ubrk_next(iterator.get())) != UBRK_DONE) {
        auto chars = folia::utf8_to_cps(winrt::to_string(std::wstring_view(wide).substr(0, boundary)));
        if (std::ranges::any_of(chars, [](char32_t c) { return c != U' ' && c != U'\t'; })) return chars.size();
    }
    return text.size();
}
}
InlineCompletionController::InlineCompletionController(EditorSession& session, CompletionConfig config,
    Microsoft::UI::Dispatching::DispatcherQueue queue, Execute execute, std::function<void()> render,
    std::function<void(CompletionState, std::string const&)> status, CompletionService::Transport transport)
    : session_(session), config_(std::move(config)), queue_(queue), execute_(std::move(execute)),
      render_(std::move(render)), status_(std::move(status)) {
    position_ = session.Selection().active; content_ = session.ContentState();
    // Disabled completion has no worker, timer or cached document text.
    if (config_.enabled && ValidateCompletionConfig(config_).empty()) {
        try {
            if (config_.promptError.empty()) {
                if (!config_.prompt) config_.prompt = LoadCompletionPromptSettings();
                if (auto error = folia::completion::validate_prompt_settings(*config_.prompt)) config_.promptError = *error;
            }
        } catch (std::exception const& error) { config_.promptError = error.what(); }
        catch (winrt::hresult_error const& error) { config_.promptError = winrt::to_string(error.message()); }
        budget_ = folia::completion::make_budget(config_.contextKB * 1000, config_.outputTokens);
        service_ = std::make_unique<CompletionService>(std::move(transport));
    }
}
InlineCompletionController::~InlineCompletionController() { Cancel(); }
void InlineCompletionController::SetState(CompletionState state, std::string const& error) {
    if (state_ == state && error.empty()) return;
    state_ = state;
    status_(state, error);
}
void InlineCompletionController::Cancel() {
    ++generation_;
    if (timer_) timer_.Stop();
    if (service_) service_->Cancel();
    proposal_.clear();
    proposalSuffix_.clear();
    position_ = session_.Selection().active; content_ = session_.ContentState();
    SetState(CompletionState::Idle);
}
void InlineCompletionController::SetFocused(bool value) {
    focused_ = value;
    if (!value) Cancel();
}
void InlineCompletionController::CompositionChanged(bool value) {
    composing_ = value;
    Cancel();
    if (!value) Schedule();
}
void InlineCompletionController::Validate() {
    if (accepting_) return;
    auto selection = session_.Selection();
    if (selection.active != position_ || !selection.is_caret() || content_ != session_.ContentState()) Cancel();
}
void InlineCompletionController::Edited(folia::TextSelection before, folia::Command const& command) {
    if (accepting_) return;
    ++generation_;
    if (service_) service_->Cancel();
    if (timer_) timer_.Stop();
    auto after = session_.Selection();
    bool typed = command.typed_input && command.kind == folia::CommandKind::InsertText;
    const auto continuing = typed && before.is_caret();
    if (continuing && proposal_.at(before.active.container_id.v, before.active.source_offset)
        && proposal_.text().starts_with(command.text)) {
        const auto consumed = folia::completion::prefix_edit(proposal_.edit(), command.text.size());
        const auto reused = folia::cps_to_utf8(consumed.before);
        if (proposalSuffix_.starts_with(reused)) proposalSuffix_.erase(0, reused.size());
    }
    if (!(continuing && proposal_.consume(before.active.container_id.v, before.active.source_offset, command.text))) proposal_.clear();
    position_ = after.active; content_ = session_.ContentState();
    if (proposal_.at(after.active.container_id.v, after.active.source_offset)) {
        // Typing may have applied semantic pairing/structure edits as well.
        // Retain a typed-through proposal only when its following source agrees.
        auto context = session_.CaptureCompletionContext(budget_);
        if (context && context->suffix == proposalSuffix_) return;
    }
    proposal_.clear(); proposalSuffix_.clear();
    SetState(CompletionState::Idle);
    if (typed || command.kind == folia::CommandKind::InsertNewline
        || command.kind == folia::CommandKind::InsertSoftBreak
        || command.kind == folia::CommandKind::DeleteBackward
        || command.kind == folia::CommandKind::DeleteForward) Schedule();
}
void InlineCompletionController::Schedule() {
    if (!focused_ || composing_ || !service_ || !config_.enabled || !ValidateCompletionConfig(config_).empty()) return;
    timer_ = queue_.CreateTimer(); timer_.IsRepeating(false);
    auto wait = (std::max)(std::chrono::milliseconds(450),
        std::chrono::duration_cast<std::chrono::milliseconds>(nextRequest_ - std::chrono::steady_clock::now()));
    timer_.Interval(wait);
    timer_.Tick([weak = weak_from_this()](auto const&, auto const&) { if (auto self = weak.lock()) self->Request(); });
    timer_.Start();
}
void InlineCompletionController::Request() {
    auto beforeValidation = generation_;
    Validate();
    if (beforeValidation != generation_ || !focused_ || composing_ || !service_) return;
    if (!config_.promptError.empty()) { SetState(CompletionState::Failed, config_.promptError); return; }
    auto context = session_.CaptureCompletionContext(budget_);
    if (!context) return;
    if (auto cached = cache_.get(*context)) {
        proposalSuffix_ = context->suffix;
        proposal_.set(position_.container_id.v, position_.source_offset, std::move(*cached));
        SetState(Active() ? CompletionState::Ready : CompletionState::Empty); render_(); return;
    }
    auto generation = ++generation_;
    nextRequest_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
    folia::completion::Prompt prompt;
    try {
        prompt = folia::completion::make_prompt(*context, *config_.prompt, budget_, config_.eagerness);
    } catch (std::exception const& error) { SetState(CompletionState::Failed, error.what()); return; }
    catch (winrt::hresult_error const& error) { SetState(CompletionState::Failed, winrt::to_string(error.message())); return; }
    SetState(CompletionState::Requesting);
    service_->Submit(config_, std::move(prompt), [weak = weak_from_this(), queue = queue_, generation, context = std::move(*context)](CompletionResult result) mutable {
        queue.TryEnqueue([weak, generation, context = std::move(context), result = std::move(result)]() mutable {
            auto self = weak.lock(); if (!self) return;
            self->Validate();
            if (generation != self->generation_ || !self->focused_) return;
            if (!result.error.empty()) {
                self->nextRequest_ = std::chrono::steady_clock::now() + std::chrono::seconds(10);
                self->SetState(CompletionState::Failed, result.error); return;
            }
            auto edit = folia::completion::decode_edit_response(result.text, context, self->budget_);
            if (!edit) {
                self->SetState(result.truncated ? CompletionState::BudgetExhausted : CompletionState::Failed,
                    result.truncated ? "" : "Invalid completion edit.");
                return;
            }
            self->proposalSuffix_ = context.suffix;
            // A token-limited empty response is not a reusable decision to abstain.
            if (!result.truncated || !edit->empty()) self->cache_.put(std::move(context), *edit);
            self->proposal_.set(self->position_.container_id.v, self->position_.source_offset, std::move(*edit));
            self->SetState(self->Active() ? CompletionState::Ready
                : result.truncated ? CompletionState::BudgetExhausted : CompletionState::Empty);
            self->render_();
        });
    });
}
bool InlineCompletionController::Accept(bool word) {
    Validate();
    if (!Active()) return false;
    auto text = proposal_.text();
    auto count = word ? nextWord(text) : text.size();
    auto edit = folia::completion::prefix_edit(proposal_.edit(), count);
    folia::completion::Edit remaining{
        proposal_.edit().before.substr(edit.before.size()), std::u32string(text.substr(count))};
    const auto at = position_;
    Cancel(); accepting_ = true;
    auto command = folia::Command{.kind = folia::CommandKind::Paste, .text = std::move(edit.after),
        .paste_range = folia::TextSpan{at.container_id, {at.source_offset, at.source_offset + edit.before.size()}}};
    bool accepted = false;
    try { accepted = execute_(command); } catch (...) { accepting_ = false; throw; }
    accepting_ = false;
    position_ = session_.Selection().active; content_ = session_.ContentState();
    if (accepted) {
        if (auto context = session_.CaptureCompletionContext(budget_)) {
            proposalSuffix_ = context->suffix;
            if (context->suffix.starts_with(folia::cps_to_utf8(remaining.before)))
                proposal_.set(position_.container_id.v, position_.source_offset, std::move(remaining));
        }
        if (Active()) SetState(CompletionState::Ready);
        if (!Active()) Schedule();
    }
    render_(); return accepted;
}
void InlineCompletionController::Decorate(detail::EditorRenderFrame& frame) const {
    if (Active()) frame.completion = detail::EditorCompletionPreview{
        position_, proposal_.text(), generation_ + proposal_.generation(), proposal_.edit().before};
}
}
