#include "providers/ModelValidation.h"
#include <windows.h>
#include <winrt/base.h>
#include "windows/completion/CompletionService.h"
#include "windows/completion/CompletionHttpTransport.h"
#include "windows/ai/ModelHttpClient.h"

import std;

namespace folia::platform::completion {
std::string ResolveCompletionEndpoint(CompletionConfig const& config) {
    return models::Endpoint(config.baseUrl, static_cast<ai::ApiProtocol>(config.protocol), models::Route::Model, config.fullUrl);
}
std::string ResolveCompletionModelsEndpoint(CompletionConfig const& config) {
    if (!config.catalogUrl.empty()) {
        models::ValidateUrl(config.catalogUrl);
        return config.catalogUrl;
    }
    return models::Endpoint(config.baseUrl, static_cast<ai::ApiProtocol>(config.protocol), models::Route::Catalog, config.fullUrl);
}
std::string ValidateCompletionService(CompletionConfig const& config) {
    try {
        (void)ResolveCompletionEndpoint(config);
        if (!config.catalogUrl.empty()) models::ValidateUrl(config.catalogUrl);
    }
    catch (std::exception const& error) { return error.what(); }
    catch (winrt::hresult_error const&) { return "Invalid completion base URL."; }
    return {};
}
std::string ValidateCompletionConfig(CompletionConfig const& config) {
    if (auto error = ValidateCompletionService(config); !error.empty()) return error;
    if (config.model.empty() || config.model.size() > 128 || config.model.find_first_of("\r\n") != std::string::npos)
        return "Choose a completion model (up to 128 bytes, without line breaks).";
    if (!folia::ai::SupportsReasoningOptions(static_cast<folia::ai::ApiProtocol>(config.protocol), {config.reasoningEffort}))
        return "Reasoning effort is unsupported for the selected protocol.";
    if (config.eagerness < folia::completion::Eagerness::Conservative || config.eagerness > folia::completion::Eagerness::Proactive)
        return "Invalid completion eagerness.";
    if (config.contextKB < 1 || config.contextKB > folia::completion::max_context_bytes / 1000
        || config.outputTokens < 16 || config.outputTokens > folia::completion::max_output_tokens)
        return "Completion context must be 1–64 KB and output must be 16–4096 tokens.";
    return {};
}
struct CompletionService::State {
    struct Job { CompletionConfig config; folia::completion::Prompt prompt; Deliver deliver; std::stop_source stop; };
    std::mutex mutex;
    std::condition_variable wake;
    std::optional<Job> pending;
    std::stop_source active;
    Transport transport;
    bool closing = false;
};
CompletionService::CompletionService(Transport transport) : state_(std::make_shared<State>()) {
    state_->transport = transport ? std::move(transport) : [](auto const& config, auto const& prompt, auto stop) {
        return RequestCompletionHttp(config, prompt, stop, {});
    };
    std::thread([state = state_] {
        while (true) {
            std::optional<State::Job> job;
            {
                std::unique_lock lock(state->mutex);
                state->wake.wait(lock, [&] { return state->closing || state->pending.has_value(); });
                if (state->closing) return;
                job = std::move(state->pending); state->pending.reset(); state->active = job->stop;
            }
            CompletionResult result;
            try { result = state->transport(job->config, job->prompt, job->stop.get_token()); }
            catch (std::exception const& e) { result.error = e.what(); }
            catch (...) { result.error = "Completion failed"; }
            if (!job->stop.stop_requested()) try { job->deliver(std::move(result)); } catch (...) {}
        }
    }).detach(); // Worker owns state; UI shutdown never joins a network wait.
}
CompletionService::~CompletionService() {
    Cancel();
    { std::lock_guard lock(state_->mutex); state_->closing = true; }
    state_->wake.notify_one();
}
void CompletionService::Cancel() {
    std::stop_source stop;
    { std::lock_guard lock(state_->mutex); state_->pending.reset(); stop = state_->active; }
    stop.request_stop();
}
void CompletionService::Submit(CompletionConfig config, folia::completion::Prompt prompt, Deliver deliver) {
    Cancel();
    { std::lock_guard lock(state_->mutex);
      state_->pending = State::Job{std::move(config), std::move(prompt), std::move(deliver), {}}; }
    state_->wake.notify_one();
}
}
