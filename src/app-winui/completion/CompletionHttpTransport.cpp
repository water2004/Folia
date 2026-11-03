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
    DWORD Wait(DWORD expected, std::chrono::steady_clock::time_point deadline, std::stop_token stop) {
        std::unique_lock lock(mutex);
        if (!wake.wait_until(lock, deadline, [&] { return status == expected || error || closing; }))
            throw std::runtime_error("Completion timed out");
        if (stop.stop_requested() || closing || error) throw std::runtime_error("Completion connection ended (" + std::to_string(error) + ")");
        return bytes;
    }
};
struct RequestOwner {
    std::shared_ptr<AsyncRequest> state;
    ~RequestOwner() { state->Close(); }
};
}
CompletionResult RequestCompletionHttp(CompletionConfig const& config, folia::completion::Prompt const& prompt,
    std::stop_token stop, std::wstring key) {
    using namespace fairy::providers;
    auto validation = ValidateCompletionConfig(config);
    if (!validation.empty()) throw std::runtime_error(validation);
    if (stop.stop_requested()) return {};
    auto endpoint = wide(ResolveCompletionEndpoint(config));
    URL_COMPONENTS url{sizeof(URL_COMPONENTS)};
    url.dwHostNameLength = url.dwUrlPathLength = url.dwExtraInfoLength = static_cast<DWORD>(-1);
    check(WinHttpCrackUrl(endpoint.c_str(), static_cast<DWORD>(endpoint.size()), 0, &url));
    HttpHandle session{WinHttpOpen(L"Folia inline completion", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC)};
    check(session.value != nullptr);
    check(WinHttpSetTimeouts(session.value, 4000, 4000, 8000, 8000));
    HttpHandle connection{WinHttpConnect(session.value, std::wstring(url.lpszHostName, url.dwHostNameLength).c_str(), url.nPort, 0)};
    check(connection.value != nullptr);
    auto path = std::wstring(url.lpszUrlPath, url.dwUrlPathLength);
    path.append(url.lpszExtraInfo, url.dwExtraInfoLength);
    HttpHandle pending{WinHttpOpenRequest(connection.value, L"POST", path.c_str(), nullptr,
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
    ModelRequest model; model.model = config.model; model.maxOutputTokens = prompt.output_tokens; model.stream = true;
    auto add = [&](MessageRole role, std::string const& text) {
        Message message; message.role = role; message.content.push_back({ContentKind::Text, text}); model.messages.push_back(std::move(message));
    };
    add(MessageRole::System, prompt.instructions); add(MessageRole::User, prompt.context); add(MessageRole::User, prompt.cursor);
    auto protocol = static_cast<ApiProtocol>(config.protocol);
    state->body = BuildRequest(protocol, model).dump();
    if (state->body.size() > 48 * 1024) throw std::runtime_error("Completion request exceeds limit");
    auto headers = BuildHeaders(protocol, winrt::to_string(key));
    SecureZeroMemory(key.data(), key.size() * sizeof(wchar_t));
    std::wstring headerText = L"Content-Type: application/json\r\nAccept: text/event-stream\r\n";
    for (auto const& [name, value] : headers) headerText += wide(name + ": " + value + "\r\n");
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    state->Begin([&](auto handle) { return WinHttpSetOption(handle, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect)); });
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    state->Begin([&](auto handle) { return WinHttpSendRequest(handle, headerText.c_str(), static_cast<DWORD>(headerText.size()),
        state->body.data(), static_cast<DWORD>(state->body.size()), static_cast<DWORD>(state->body.size()), context); });
    state->Wait(WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE, deadline, stop);
    SecureZeroMemory(headerText.data(), headerText.size() * sizeof(wchar_t));
    state->Begin([&](auto handle) { return WinHttpReceiveResponse(handle, nullptr); });
    state->Wait(WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE, deadline, stop);
    DWORD status = 0, size = sizeof(status);
    state->Begin([&](auto handle) { return WinHttpQueryHeaders(handle, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX); });
    if (status != 200) throw std::runtime_error("Completion service returned HTTP " + std::to_string(status));
    wchar_t contentType[256]{}; size = sizeof(contentType);
    state->Begin([&](auto handle) { return WinHttpQueryHeaders(handle, WINHTTP_QUERY_CONTENT_TYPE,
        WINHTTP_HEADER_NAME_BY_INDEX, contentType, &size, WINHTTP_NO_HEADER_INDEX); });
    bool streaming = std::wstring_view(contentType).find(L"text/event-stream") != std::wstring_view::npos;
    StreamResponse decoder(protocol); std::string response; std::size_t received = 0;
    while (!stop.stop_requested()) {
        state->Begin([&](auto handle) { return WinHttpReadData(handle, state->buffer.data(), static_cast<DWORD>(state->buffer.size()), nullptr); });
        auto count = state->Wait(WINHTTP_CALLBACK_STATUS_READ_COMPLETE, deadline, stop);
        if (!count) break;
        if ((received += count) > 128 * 1024) throw std::runtime_error("Completion response exceeds limit");
        if (streaming) (void)decoder.Feed({state->buffer.data(), count});
        else response.append(state->buffer.data(), count);
    }
    if (stop.stop_requested()) return {};
    auto result = streaming ? decoder.Finish() : ParseResponse(protocol, Json::parse(response));
    CompletionResult output;
    for (auto const& part : result.message.content) if (part.kind == ContentKind::Text) output.text += folia::utf8_to_cps(part.text);
    output.inputTokens = result.usage.inputTokens; output.outputTokens = result.usage.outputTokens;
    output.cachedInputTokens = result.usage.cachedInputTokens;
    return output;
}
}
