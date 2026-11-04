#include "providers/ModelAdapter.h"
#include "providers/StreamResponse.h"
#include "pch.h"
#include "completion/CompletionHttpTransport.h"
#include <winhttp.h>
import folia.markdown.utf;

namespace winrt::Folia {
namespace {
void check(BOOL ok) { if (!ok) throw std::runtime_error("Completion connection failed (" + std::to_string(GetLastError()) + ")"); }
std::wstring wide(std::string_view text) { return std::wstring(winrt::to_hstring(text)); }
struct HttpHandle {
    HINTERNET value{};
    ~HttpHandle() { if (value) WinHttpCloseHandle(value); }
};
// WinHTTP owns a shared reference until HANDLE_CLOSING, its final notification.
// Buffers and callback state therefore outlive cancellation and late callbacks.
struct AsyncRequest {
    std::mutex handles, mutex;
    std::condition_variable wake;
    HINTERNET handle{};
    DWORD status{}, bytes{}, error{};
    bool closing = false;
    std::string body;
    std::array<char, 4096> buffer;
    using Lifetime = std::shared_ptr<AsyncRequest>;
    static void CALLBACK Callback(HINTERNET, DWORD_PTR context, DWORD status, void* info, DWORD length) noexcept {
        if (!context) return;
        auto* lifetime = reinterpret_cast<Lifetime*>(context);
        auto state = *lifetime;
        {
            std::lock_guard lock(state->mutex);
            switch (status) {
            case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
            case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
            case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
                state->status = status; state->bytes = length; break;
            case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:
                state->status = status;
                if (length == sizeof(DWORD)) state->bytes = *static_cast<DWORD*>(info);
                else state->error = ERROR_INVALID_DATA;
                break;
            case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
                if (length >= sizeof(WINHTTP_ASYNC_RESULT)) state->error = static_cast<WINHTTP_ASYNC_RESULT*>(info)->dwError;
                else state->error = ERROR_INVALID_DATA;
                break;
            case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING: state->closing = true; break;
            default: break;
            }
        }
        state->wake.notify_one();
        if (status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING) delete lifetime;
    }
    void Close() noexcept {
        HINTERNET current{};
        { std::lock_guard lock(handles); current = std::exchange(handle, nullptr); }
        if (current) WinHttpCloseHandle(current);
    }
    template<class Operation> void Begin(Operation operation) {
        { std::lock_guard lock(mutex); status = 0; bytes = 0; }
        std::lock_guard lock(handles);
        if (!handle) throw std::runtime_error("Completion cancelled");
        check(operation(handle));
    }
    DWORD Wait(DWORD expected, std::chrono::milliseconds inactivity, std::stop_token stop) {
        std::unique_lock lock(mutex);
        if (!wake.wait_for(lock, inactivity, [&] { return status == expected || error || closing; }))
            throw std::runtime_error("Completion timed out");
        if (stop.stop_requested() || closing || error) throw std::runtime_error("Completion connection ended (" + std::to_string(error) + ")");
        return bytes;
    }
};
struct RequestOwner {
    std::shared_ptr<AsyncRequest> state;
    ~RequestOwner() { state->Close(); }
};
bool ReadHttp(CompletionConfig const& config, std::string const& urlText, wchar_t const* method,
    std::string body, std::stop_token stop, std::wstring key,
    std::function<bool(bool, std::string_view)> const& consume,
    std::chrono::milliseconds inactivity = std::chrono::seconds(15)) {
    using namespace fairy::providers;
    auto validation = ValidateCompletionConfig(config);
    if (!validation.empty()) throw std::runtime_error(validation);
    if (stop.stop_requested()) return false;
    auto endpoint = wide(urlText);
    URL_COMPONENTS url{sizeof(URL_COMPONENTS)};
    url.dwHostNameLength = url.dwUrlPathLength = url.dwExtraInfoLength = static_cast<DWORD>(-1);
    check(WinHttpCrackUrl(endpoint.c_str(), static_cast<DWORD>(endpoint.size()), 0, &url));
    HttpHandle session{WinHttpOpen(L"Folia inline completion", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC)};
    check(session.value != nullptr);
    if (inactivity.count() <= 0 || inactivity.count() > (std::numeric_limits<int>::max)())
        throw std::invalid_argument("Invalid completion inactivity timeout.");
    check(WinHttpSetTimeouts(session.value, 4000, 4000, 8000, static_cast<int>(inactivity.count())));
    HttpHandle connection{WinHttpConnect(session.value, std::wstring(url.lpszHostName, url.dwHostNameLength).c_str(), url.nPort, 0)};
    check(connection.value != nullptr);
    auto path = std::wstring(url.lpszUrlPath, url.dwUrlPathLength);
    path.append(url.lpszExtraInfo, url.dwExtraInfoLength);
    HttpHandle pending{WinHttpOpenRequest(connection.value, method, path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, url.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)};
    check(pending.value != nullptr);
    auto state = std::make_shared<AsyncRequest>();
    auto lifetime = std::make_unique<AsyncRequest::Lifetime>(state);
    auto context = reinterpret_cast<DWORD_PTR>(lifetime.get());
    check(WinHttpSetOption(pending.value, WINHTTP_OPTION_CONTEXT_VALUE, &context, sizeof(context)));
    if (WinHttpSetStatusCallback(pending.value, AsyncRequest::Callback,
        WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS | WINHTTP_CALLBACK_FLAG_HANDLES, 0) == WINHTTP_INVALID_STATUS_CALLBACK)
        throw std::runtime_error("Cannot initialize completion connection");
    lifetime.release(); // released by HANDLE_CLOSING
    state->handle = std::exchange(pending.value, nullptr);
    RequestOwner owner{state};
    std::stop_callback cancel(stop, [state] { state->Close(); });
    auto protocol = static_cast<ApiProtocol>(config.protocol);
    state->body = std::move(body);
    if (state->body.size() > 512 * 1024) throw std::runtime_error("Completion request exceeds limit");
    auto headers = BuildHeaders(protocol, winrt::to_string(key));
    SecureZeroMemory(key.data(), key.size() * sizeof(wchar_t));
    std::wstring headerText = L"Content-Type: application/json\r\nAccept: application/json, text/event-stream\r\n";
    for (auto const& [name, value] : headers) headerText += wide(name + ": " + value + "\r\n");
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    state->Begin([&](auto handle) { return WinHttpSetOption(handle, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect)); });
    state->Begin([&](auto handle) { return WinHttpSendRequest(handle, headerText.c_str(), static_cast<DWORD>(headerText.size()),
        state->body.data(), static_cast<DWORD>(state->body.size()), static_cast<DWORD>(state->body.size()), context); });
    state->Wait(WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE, inactivity, stop);
    SecureZeroMemory(headerText.data(), headerText.size() * sizeof(wchar_t));
    state->Begin([&](auto handle) { return WinHttpReceiveResponse(handle, nullptr); });
    state->Wait(WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE, inactivity, stop);
    DWORD status = 0, size = sizeof(status);
    state->Begin([&](auto handle) { return WinHttpQueryHeaders(handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX); });
    if (status != 200) throw std::runtime_error("Completion service returned HTTP " + std::to_string(status));
    wchar_t contentType[256]{}; size = sizeof(contentType);
    state->Begin([&](auto handle) { return WinHttpQueryHeaders(handle, WINHTTP_QUERY_CONTENT_TYPE,
        WINHTTP_HEADER_NAME_BY_INDEX, contentType, &size, WINHTTP_NO_HEADER_INDEX); });
    bool streaming = std::wstring_view(contentType).find(L"text/event-stream") != std::wstring_view::npos;
    std::size_t received = 0;
    while (!stop.stop_requested()) {
        state->Begin([&](auto handle) { return WinHttpQueryDataAvailable(handle, nullptr); });
        const auto available = state->Wait(WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE, inactivity, stop);
        state->Begin([&](auto handle) { return WinHttpReadData(handle, state->buffer.data(),
            (std::min)((std::max)(available, DWORD{1}), static_cast<DWORD>(state->buffer.size())), nullptr); });
        auto count = state->Wait(WINHTTP_CALLBACK_STATUS_READ_COMPLETE, inactivity, stop);
        if (!count) break;
        if ((received += count) > 1024 * 1024) throw std::runtime_error("Completion response exceeds limit");
        if (consume(streaming, {state->buffer.data(), count})) break;
    }
    return streaming;
}
}
CompletionResult RequestCompletionHttp(CompletionConfig const& config, folia::completion::Prompt const& prompt,
    std::stop_token stop, std::wstring key, std::chrono::milliseconds inactivityTimeout) {
    using namespace fairy::providers;
    ModelRequest model; model.model = config.model; model.maxOutputTokens = prompt.output_tokens; model.stream = true;
    model.reasoning.effort = config.reasoningEffort;
    auto add = [&](MessageRole role, std::string const& text) {
        Message message; message.role = role; message.content.push_back({ContentKind::Text, text}); model.messages.push_back(std::move(message));
    };
    add(MessageRole::System, prompt.instructions); add(MessageRole::User, prompt.context); add(MessageRole::User, prompt.cursor);
    auto protocol = static_cast<ApiProtocol>(config.protocol);
    StreamResponse decoder(protocol); std::string response;
    auto streaming = ReadHttp(config, ResolveCompletionEndpoint(config), L"POST", BuildRequest(protocol, model).dump(), stop,
        std::move(key), [&](bool streaming, std::string_view chunk) {
            if (streaming) { (void)decoder.Feed(chunk); return decoder.Complete(); }
            response.append(chunk); return false;
        }, inactivityTimeout);
    if (stop.stop_requested()) return {};
    auto result = streaming ? decoder.Finish() : ParseResponse(protocol, Json::parse(response));
    CompletionResult output;
    for (auto const& part : result.message.content) if (part.kind == ContentKind::Text) output.text += folia::utf8_to_cps(part.text);
    output.inputTokens = result.usage.inputTokens; output.outputTokens = result.usage.outputTokens;
    output.cachedInputTokens = result.usage.cachedInputTokens;
    output.truncated = result.finish == FinishReason::Truncated;
    return output;
}
std::vector<std::string> ParseCompletionReasoningEfforts(std::string_view json, std::string_view model) {
    if (json.size() > 1024 * 1024) throw std::runtime_error("Model metadata exceeds limit.");
    auto root = fairy::providers::Json::parse(json);
    auto found = root.find("data");
    if (found == root.end() || !found->is_array()) throw std::runtime_error("Invalid model metadata response.");
    for (auto const& item : *found) {
        if (!item.is_object() || item.value("id", std::string{}) != model) continue;
        auto effort = item.find("effort");
        if (effort == item.end()) return {};
        if (!effort->is_object()) throw std::runtime_error("Invalid reasoning metadata.");
        auto levels = effort->find("supported_levels");
        if (levels == effort->end()) return {};
        if (!levels->is_array() || levels->size() > 32) throw std::runtime_error("Invalid reasoning levels.");
        std::vector<std::string> result;
        for (auto const& level : *levels) {
            if (!level.is_string()) throw std::runtime_error("Invalid reasoning level.");
            auto name = level.get<std::string>();
            if (name.empty() || name.size() > 32 || !std::ranges::all_of(name, [](unsigned char c) {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
            })) throw std::runtime_error("Invalid reasoning level.");
            if (std::ranges::find(result, name) == result.end()) result.push_back(std::move(name));
        }
        return result;
    }
    throw std::runtime_error("Configured model was not found in the upstream model list.");
}
std::vector<std::string> FetchCompletionReasoningEfforts(CompletionConfig const& config, std::stop_token stop, std::wstring key) {
    std::string response;
    ReadHttp(config, ResolveCompletionModelsEndpoint(config), L"GET", {}, stop, std::move(key),
        [&](bool, std::string_view chunk) { response.append(chunk); return false; });
    if (stop.stop_requested()) return {};
    return ParseCompletionReasoningEfforts(response, config.model);
}
}
