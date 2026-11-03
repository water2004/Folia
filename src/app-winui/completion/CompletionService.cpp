#include "pch.h"
#include "completion/CompletionService.h"
#include <wincred.h>
#include "completion/CompletionHttpTransport.h"
#include <winhttp.h>

namespace winrt::Folia {
namespace {
constexpr wchar_t CredentialName[] = L"Folia/InlineCompletion/APIKey";

}
std::string ValidateCompletionConfig(CompletionConfig const& config) {
    if (config.model.empty() || config.model.size() > 256) return "Choose a completion model (up to 256 bytes).";
    if (config.endpoint.size() > 2048 || config.endpoint.find_first_of("\r\n") != std::string::npos) return "Invalid completion endpoint.";
    if (config.protocol < 0 || config.protocol > 2) return "Unsupported completion protocol.";
    auto endpoint = winrt::to_hstring(config.endpoint);
    URL_COMPONENTS url{sizeof(URL_COMPONENTS)};
    url.dwHostNameLength = url.dwUserNameLength = url.dwPasswordLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(endpoint.c_str(), endpoint.size(), 0, &url)
        || url.dwUserNameLength || url.dwPasswordLength || config.endpoint.find('#') != std::string::npos)
        return "Invalid completion endpoint.";
    auto host = std::wstring(url.lpszHostName, url.dwHostNameLength);
    std::ranges::transform(host, host.begin(), [](wchar_t c) { return std::towlower(c); });
    if (url.nScheme != INTERNET_SCHEME_HTTPS && !(url.nScheme == INTERNET_SCHEME_HTTP
        && (host == L"localhost" || host == L"127.0.0.1" || host == L"[::1]" || host == L"::1")))
        return "Use HTTPS or a local HTTP endpoint.";
    return {};
}
std::wstring LoadCompletionKey() {
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(CredentialName, CRED_TYPE_GENERIC, 0, &credential)) {
        if (GetLastError() == ERROR_NOT_FOUND) return {};
        winrt::throw_last_error();
    }
    auto free = std::unique_ptr<CREDENTIALW, decltype(&CredFree)>(credential, &CredFree);
    return {reinterpret_cast<wchar_t const*>(credential->CredentialBlob), credential->CredentialBlobSize / sizeof(wchar_t)};
}
void StoreCompletionKey(std::wstring_view key) {
    if (key.empty()) {
        if (!CredDeleteW(CredentialName, CRED_TYPE_GENERIC, 0) && GetLastError() != ERROR_NOT_FOUND) winrt::throw_last_error();
        return;
    }
    if (key.size() * sizeof(wchar_t) > CRED_MAX_CREDENTIAL_BLOB_SIZE) throw std::runtime_error("API key is too long");
    CREDENTIALW credential{}; credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<wchar_t*>(CredentialName); credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.CredentialBlobSize = static_cast<DWORD>(key.size() * sizeof(wchar_t));
    credential.CredentialBlob = reinterpret_cast<BYTE*>(const_cast<wchar_t*>(key.data()));
    if (!CredWriteW(&credential, 0)) winrt::throw_last_error();
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
        return RequestCompletionHttp(config, prompt, stop, LoadCompletionKey());
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
