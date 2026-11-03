#pragma once
import std;
import folia.completion;

namespace winrt::Folia {
struct CompletionConfig {
    bool enabled = false;
    std::string endpoint, model;
    int protocol = 0; // Chat Completions, Responses, Anthropic Messages
    bool operator==(CompletionConfig const&) const = default;
};
struct CompletionResult {
    std::u32string text;
    std::string error;
    std::optional<std::uint64_t> inputTokens, outputTokens, cachedInputTokens;
};
// Full endpoint URL; no model-specific endpoint guessing.
std::string ValidateCompletionConfig(CompletionConfig const&);
std::wstring LoadCompletionKey();
void StoreCompletionKey(std::wstring_view key);
class CompletionService {
public:
    using Deliver = std::function<void(CompletionResult)>;
    using Transport = std::function<CompletionResult(CompletionConfig const&, folia::completion::Prompt const&, std::stop_token)>;
    explicit CompletionService(Transport transport = {});
    ~CompletionService();
    CompletionService(CompletionService const&) = delete;
    CompletionService& operator=(CompletionService const&) = delete;
    void Submit(CompletionConfig, folia::completion::Prompt, Deliver);
    void Cancel();
private:
    struct State;
    std::shared_ptr<State> state_;
};
}
