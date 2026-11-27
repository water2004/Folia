#include "providers/ModelAdapter.h"
#include "providers/StreamResponse.h"
#include <windows.h>
#include <wincred.h>
#include <winhttp.h>
#include <winrt/base.h>
#include "windows/ai/ModelHttpClient.h"

namespace folia::platform::models {
void ValidateUrl(std::string const& base) {
    if (base.size() > 2048 || base.find_first_of("\r\n") != std::string::npos)
        throw std::invalid_argument("Invalid completion base URL.");
    auto baseUrl = winrt::to_hstring(base);
    URL_COMPONENTS url{sizeof(URL_COMPONENTS)};
    url.dwHostNameLength = url.dwUserNameLength = url.dwPasswordLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(baseUrl.c_str(), baseUrl.size(), 0, &url) || !url.dwHostNameLength
        || url.dwUserNameLength || url.dwPasswordLength || base.find('#') != std::string::npos)
        throw std::invalid_argument("Invalid completion base URL.");
    auto host = std::wstring(url.lpszHostName, url.dwHostNameLength);
    std::ranges::transform(host, host.begin(), [](wchar_t c) { return std::towlower(c); });
    if (url.nScheme != INTERNET_SCHEME_HTTPS && !(url.nScheme == INTERNET_SCHEME_HTTP
        && (host == L"localhost" || host == L"127.0.0.1" || host == L"[::1]" || host == L"::1")))
        throw std::invalid_argument("Use HTTPS or a local HTTP base URL.");
}
static std::string ResolveBase(std::string const& base, std::string_view suffix) {
    auto queryAt = base.find('?');
    auto path = base.substr(0, queryAt);
    auto query = queryAt == std::string::npos ? std::string{} : base.substr(queryAt);
    while (path.ends_with('/')) path.pop_back();
    if (path.find('/', path.find("://") + 3) == std::string::npos) path += "/v1";
    path += suffix;
    path += query;
    if (path.size() > 2048) throw std::invalid_argument("Completion base URL is too long.");
    return path;
}
std::string Endpoint(std::string const& base, ai::ApiProtocol protocol, Route route, bool fullUrl) {
    constexpr std::array<std::string_view, 3> paths{"/chat/completions", "/responses", "/messages"};
    auto index = static_cast<std::size_t>(protocol);
    if (index >= paths.size()) throw std::invalid_argument("Unsupported API protocol.");
    ValidateUrl(base);
    if (!fullUrl) return ResolveBase(base, route == Route::Catalog ? "/models" : paths[index]);
    if (route == Route::Model) return base;
    auto queryAt = base.find('?');
    auto path = std::string_view(base).substr(0, queryAt);
    if (!path.ends_with(paths[index]))
        throw std::invalid_argument("Set a model list URL for this custom request endpoint.");
    return std::string(path.substr(0, path.size() - paths[index].size())) + "/models" +
        (queryAt == base.npos ? std::string{} : base.substr(queryAt));
}
std::wstring LoadKey(std::wstring const& target) {
    PCREDENTIALW credential = nullptr;
    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential)) {
        if (GetLastError() == ERROR_NOT_FOUND) return {};
        winrt::throw_last_error();
    }
    auto free = std::unique_ptr<CREDENTIALW, decltype(&CredFree)>(credential, &CredFree);
    return {reinterpret_cast<wchar_t const*>(credential->CredentialBlob), credential->CredentialBlobSize / sizeof(wchar_t)};
}
void StoreKey(std::wstring const& target, std::wstring_view key) {
    if (key.empty()) {
        if (!CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0) && GetLastError() != ERROR_NOT_FOUND) winrt::throw_last_error();
        return;
    }
    if (key.size() * sizeof(wchar_t) > CRED_MAX_CREDENTIAL_BLOB_SIZE) throw std::runtime_error("API key is too long");
    CREDENTIALW credential{}; credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<wchar_t*>(target.c_str()); credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.CredentialBlobSize = static_cast<DWORD>(key.size() * sizeof(wchar_t));
    credential.CredentialBlob = reinterpret_cast<BYTE*>(const_cast<wchar_t*>(key.data()));
    if (!CredWriteW(&credential, 0)) winrt::throw_last_error();
}

namespace {
struct TransientNetworkError : std::runtime_error {
    explicit TransientNetworkError(std::string message, unsigned delay = 0)
        : std::runtime_error(std::move(message)), retryAfter(delay) {}
    unsigned retryAfter;
};
[[noreturn]] void ConnectionError(DWORD code) {
    const auto message = "Model connection failed (" + std::to_string(code) + ")";
    switch (code) {
    case ERROR_WINHTTP_TIMEOUT: case ERROR_WINHTTP_NAME_NOT_RESOLVED: case ERROR_WINHTTP_CANNOT_CONNECT:
    case ERROR_WINHTTP_CONNECTION_ERROR: case ERROR_WINHTTP_RESEND_REQUEST:
        throw TransientNetworkError(message);
    default: throw std::runtime_error(message);
    }
}
void check(BOOL ok) { if (!ok) ConnectionError(GetLastError()); }
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
            throw TransientNetworkError("Model connection timed out");
        if (stop.stop_requested() || closing) throw std::runtime_error("Cancelled");
        if (error) ConnectionError(error);
        return bytes;
    }
};
struct RequestOwner {
    std::shared_ptr<AsyncRequest> state;
    ~RequestOwner() { state->Close(); }
};
std::string HttpErrorDetails(std::string_view body) {
    auto json = folia::ai::Json::parse(body, nullptr, false);
    if (!json.is_object()) return {};
    auto error = json.find("error");
    if (error == json.end() || !error->is_object()) return {};
    // Only show short identifiers. A service's free-form message can echo
    // credentials or document text from the request.
    std::string details;
    for (auto field : {"type", "code", "param"}) {
        auto value = error->find(field);
        if (value == error->end() || !value->is_string()) continue;
        auto const& identifier = value->get_ref<std::string const&>();
        if (identifier.empty() || identifier.size() > 80 || !std::ranges::all_of(identifier, [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
                || c == '_' || c == '-' || c == '.' || c == '[' || c == ']';
        })) continue;
        details += details.empty() ? " (" : ", ";
        details += std::string(field) + ": " + identifier;
    }
    if (!details.empty()) details += ')';
    return details;
}
std::string ReadHttpErrorDetails(AsyncRequest& state, std::chrono::milliseconds timeout, std::stop_token stop) {
    constexpr std::size_t maxBytes = 8192;
    const auto deadline = std::chrono::steady_clock::now() + (std::min)(timeout, std::chrono::milliseconds(1000));
    std::string body;
    try {
        while (!stop.stop_requested() && body.size() <= maxBytes) {
            auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
            if (remaining.count() <= 0) break;
            auto capacity = (std::min)(state.buffer.size(), maxBytes + 1 - body.size());
            state.Begin([&](auto handle) { return WinHttpReadData(handle, state.buffer.data(), static_cast<DWORD>(capacity), nullptr); });
            auto count = state.Wait(WINHTTP_CALLBACK_STATUS_READ_COMPLETE, remaining, stop);
            if (!count) return HttpErrorDetails(body);
            body.append(state.buffer.data(), count);
        }
    } catch (std::runtime_error const&) {
        // Keep the original HTTP status when an error body is unavailable.
    }
    return {};
}
bool Exchange(ai::ApiProtocol protocol, std::string const& urlText, wchar_t const* method,
    std::string body, std::stop_token stop, std::wstring key,
    std::function<bool(bool, std::string_view)> const& consume,
    std::chrono::milliseconds inactivity) {
    if (stop.stop_requested()) return false;
    auto endpoint = wide(urlText);
    URL_COMPONENTS url{sizeof(URL_COMPONENTS)};
    url.dwHostNameLength = url.dwUrlPathLength = url.dwExtraInfoLength = static_cast<DWORD>(-1);
    check(WinHttpCrackUrl(endpoint.c_str(), static_cast<DWORD>(endpoint.size()), 0, &url));
    HttpHandle session{WinHttpOpen(L"Folia", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
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
    state->body = std::move(body);
    if (state->body.size() > 11 * 1024 * 1024) throw std::runtime_error("Completion request exceeds limit");
    auto headers = ai::BuildHeaders(protocol, winrt::to_string(key));
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
    if (status != 200) {
        auto details = status >= 400 ? ReadHttpErrorDetails(*state, inactivity, stop) : std::string{};
        auto message = "Completion service returned HTTP " + std::to_string(status) + details;
        if (status == 408 || status == 429 || status == 500 || status == 502 || status == 503 || status == 504) {
            DWORD seconds = 0, length = sizeof(seconds);
            // The optional header can be absent or an HTTP date instead of a delta.
            state->Begin([&](auto handle) {
                WinHttpQueryHeaders(handle, WINHTTP_QUERY_RETRY_AFTER | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &seconds, &length, WINHTTP_NO_HEADER_INDEX);
                return TRUE;
            });
            throw TransientNetworkError(message, seconds);
        }
        throw std::runtime_error(message);
    }
    wchar_t contentType[256]{}; size = sizeof(contentType);
    state->Begin([&](auto handle) { return WinHttpQueryHeaders(handle, WINHTTP_QUERY_CONTENT_TYPE,
        WINHTTP_HEADER_NAME_BY_INDEX, contentType, &size, WINHTTP_NO_HEADER_INDEX); });
    bool streaming = std::wstring_view(contentType).find(L"text/event-stream") != std::wstring_view::npos;
    // Each SSE delta carries a JSON envelope. Bound encoded traffic separately from a single JSON result.
    auto responseLimit = streaming ? 16u * 1024u * 1024u : 1024u * 1024u;
    std::size_t received = 0;
    while (!stop.stop_requested()) {
        state->Begin([&](auto handle) { return WinHttpQueryDataAvailable(handle, nullptr); });
        const auto available = state->Wait(WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE, inactivity, stop);
        state->Begin([&](auto handle) { return WinHttpReadData(handle, state->buffer.data(),
            (std::min)((std::max)(available, DWORD{1}), static_cast<DWORD>(state->buffer.size())), nullptr); });
        auto count = state->Wait(WINHTTP_CALLBACK_STATUS_READ_COMPLETE, inactivity, stop);
        if (!count) break;
        if ((received += count) > responseLimit) throw std::runtime_error("Model response exceeds limit");
        if (consume(streaming, {state->buffer.data(), count})) break;
    }
    return streaming;
}
}
ai::ModelResult Request(std::string const& baseUrl, ai::ApiProtocol protocol, ai::Json const& request, std::wstring key,
    std::stop_token stop, TextDelta const& delta, std::chrono::milliseconds inactivity, bool fullUrl, StreamObserver const& observe) {
    using namespace folia::ai;
    StreamResponse decoder(protocol);
    std::string response;
    const auto streaming = Exchange(protocol, Endpoint(baseUrl, protocol, Route::Model, fullUrl), L"POST", request.dump(), stop,
        std::move(key), [&](bool streaming, std::string_view chunk) {
            if (!streaming) {
                response.append(chunk);
                return false;
            }
            auto events = decoder.Feed(chunk);
            for (auto const& event : events) {
                if (observe) observe(event);
                if (delta && event.kind == StreamEventKind::TextDelta) delta(event.text);
            }
            return decoder.Complete();
        }, inactivity);
    if (stop.stop_requested()) return {};
    if ((streaming && !decoder.Complete()) || (!streaming && response.empty()))
        throw TransientNetworkError("The connection closed before the model finished its response.");
    return streaming ? decoder.Finish() : ParseResponse(protocol, Json::parse(response));
}

ai::ModelResult Request(std::string const& baseUrl, ai::ApiProtocol protocol, ai::ModelRequest const& request,
                        std::wstring key, std::stop_token stop, TextDelta const& delta, bool fullUrl, StreamObserver const& observe) {
    return Request(baseUrl, protocol, ai::BuildRequest(protocol, request), std::move(key), stop, delta, std::chrono::seconds(90), fullUrl, observe);
}

ai::ModelResult RequestChat(std::string const& address, ai::ApiProtocol protocol, ai::ModelRequest const& wire,
    std::wstring key, std::stop_token stop, bool fullUrl, StreamObserver const& observe, std::function<void(unsigned)> const& onRetry) {
    auto prepared = ai::BuildRequest(protocol, wire);
    for (unsigned attempt = 0; ; ++attempt) {
        if (stop.stop_requested()) throw std::runtime_error("Cancelled");
        bool started = false;
        try {
            return Request(address, protocol, prepared, key, stop, {}, std::chrono::seconds(90), fullUrl,
                [&](ai::StreamEvent const& event) {
                    if (event.kind != ai::StreamEventKind::UsageUpdated) started = true;
                    if (observe) observe(event);
                });
        } catch (TransientNetworkError const& error) {
            if (started || stop.stop_requested() || attempt >= 2 || error.retryAfter > 30) throw;
            if (onRetry) onRetry(attempt + 1);
            auto delay = error.retryAfter ? std::chrono::milliseconds(error.retryAfter * 1000) :
                std::chrono::milliseconds(500u << attempt);
            std::mutex mutex;
            std::unique_lock lock(mutex);
            std::condition_variable_any{}.wait_for(lock, stop, delay, [] { return false; });
        }
    }
}

std::string ReadCatalogPage(ai::ApiProtocol protocol, std::string const& endpoint, std::wstring key, std::stop_token stop) {
    std::string response;
    Exchange(protocol, endpoint, L"GET", {}, stop, std::move(key), [&](bool, std::string_view text) {
        response.append(text);
        return false;
    }, std::chrono::seconds(15));
    return response;
}
}
