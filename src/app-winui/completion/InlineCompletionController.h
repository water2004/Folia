#pragma once
#include "completion/CompletionService.h"
#include "editor/session/EditorSession.h"

namespace winrt::Folia {
enum class CompletionState { Idle, Requesting, Ready, Empty, BudgetExhausted, Failed };
class InlineCompletionController : public std::enable_shared_from_this<InlineCompletionController> {
public:
    using Execute = std::function<bool(folia::Command const&)>;
    InlineCompletionController(EditorSession&, CompletionConfig,
        Microsoft::UI::Dispatching::DispatcherQueue, Execute, std::function<void()> render,
        std::function<void(CompletionState, std::string const&)> status, CompletionService::Transport transport = {});
    ~InlineCompletionController();
    void Edited(folia::TextSelection before, folia::Command const&);
    void Validate();
    void Cancel();
    void SetFocused(bool);
    void CompositionChanged(bool);
    bool Accept(bool word);
    bool Active() const { return proposal_.active(); }
    void Decorate(detail::EditorRenderFrame&) const;
private:
    void Schedule();
    void Request();
    void SetState(CompletionState, std::string const& error = {});
    EditorSession& session_;
    CompletionConfig config_;
    folia::completion::Budget budget_;
    Microsoft::UI::Dispatching::DispatcherQueue queue_;
    Microsoft::UI::Dispatching::DispatcherQueueTimer timer_{nullptr};
    std::unique_ptr<CompletionService> service_;
    Execute execute_;
    std::function<void()> render_;
    std::function<void(CompletionState, std::string const&)> status_;
    CompletionState state_ = CompletionState::Idle;
    folia::completion::Proposal proposal_;
    folia::completion::Cache cache_;
    std::string proposalSuffix_;
    folia::TextPosition position_{};
    std::uint64_t content_ = 0, generation_ = 0;
    bool focused_ = false, accepting_ = false, composing_ = false;
    std::chrono::steady_clock::time_point nextRequest_{};
};
}
