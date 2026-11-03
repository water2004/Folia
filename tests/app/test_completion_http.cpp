#include "pch.h"
#include <winsock.h>
#include "support/folia_test.hpp"
#include "completion/CompletionHttpTransport.h"

namespace {
using namespace boost::ut;
using namespace winrt::Folia;
struct Server {
    SOCKET listener = INVALID_SOCKET;
    std::atomic<SOCKET> client = INVALID_SOCKET;
    std::thread thread;
    std::string baseUrl, request;
    std::atomic<bool> received = false;
    Server(std::string body, int status = 200, bool hold = false) {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data)) throw std::runtime_error("test socket init");
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) || listen(listener, 1)) throw std::runtime_error("test socket bind");
        int length = sizeof(address); getsockname(listener, reinterpret_cast<sockaddr*>(&address), &length);
        baseUrl = "http://127.0.0.1:" + std::to_string(ntohs(address.sin_port));
        thread = std::thread([this, body = std::move(body), status, hold] {
            auto socket = accept(listener, nullptr, nullptr); client = socket;
            if (socket == INVALID_SOCKET) return;
            int timeout = 2000;
            setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
            setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
            std::array<char, 4096> buffer;
            std::size_t bodySize = 0;
            while (request.size() < 64 * 1024) {
                auto count = recv(socket, buffer.data(), static_cast<int>(buffer.size()), 0); if (count <= 0) break;
                request.append(buffer.data(), count);
                auto end = request.find("\r\n\r\n");
                if (end != std::string::npos) {
                    auto header = request.find("Content-Length:");
                    if (header != std::string::npos) bodySize = std::stoul(request.substr(header + 15));
                    if (request.size() >= end + 4 + bodySize) break;
                }
            }
            received = true;
            auto response = "HTTP/1.1 " + std::to_string(status) + " OK\r\nContent-Type: text/event-stream\r\nContent-Length: "
                + std::to_string(hold ? 10000 : body.size()) + "\r\nConnection: close\r\n\r\n" + body;
            for (std::size_t at = 0; at < response.size();) {
                auto sent = send(socket, response.data() + at, static_cast<int>((std::min)(std::size_t{37}, response.size() - at)), 0);
                if (sent <= 0) break; at += sent;
            }
            if (hold) (void)recv(socket, buffer.data(), 1, 0);
            if (client.exchange(INVALID_SOCKET) == socket) closesocket(socket);
        });
    }
    ~Server() {
        closesocket(listener);
        if (auto socket = client.exchange(INVALID_SOCKET); socket != INVALID_SOCKET) { shutdown(socket, 2); closesocket(socket); }
        if (thread.joinable()) thread.join(); WSACleanup();
    }
};
std::string const response = R"(data: {"choices":[{"index":0,"delta":{"content":" hello"},"finish_reason":null}]})" "\n\n"
    R"(data: {"choices":[{"index":0,"delta":{},"finish_reason":"stop"}]})" "\n\n"
    "data: [DONE]\n\n";
suite completion_http_tests = [] {
    "completion base URL derives protocol paths and preserves base prefixes and queries"_test = [] {
        struct Example { std::string base, prefix, query; };
        for (auto example : {
            Example{"https://service.example", "https://service.example/v1", ""},
            Example{"https://service.example///", "https://service.example/v1", ""},
            Example{"https://service.example/v1/", "https://service.example/v1", ""},
            Example{"https://service.example/api/v2///?rev=a%2Fb", "https://service.example/api/v2", "?rev=a%2Fb"},
            Example{"https://service.example?rev=/x/", "https://service.example/v1", "?rev=/x/"},
            Example{"http://[::1]:8080/v1", "http://[::1]:8080/v1", ""}}) {
            std::array<std::string, 3> paths{"/chat/completions", "/responses", "/messages"};
            for (int protocol = 0; protocol < paths.size(); ++protocol)
                expect(ResolveCompletionEndpoint({true, example.base, "fixture", protocol}) == example.prefix + paths[protocol] + example.query);
        }
        for (auto base : {"", "https:///", "https://service.example/v1#fragment", "https://service.example/\n"})
            expect(!ValidateCompletionConfig({true, base, "fixture", 0}).empty());
        expect(!ValidateCompletionConfig({true, "https://service.example", "fixture", 3}).empty());
    };
    "completion HTTP parses fragmented loopback response with bounded body"_test = [] {
        Server server(response);
        auto result = std::async(std::launch::async, [&] {
            return RequestCompletionHttp({true, server.baseUrl, "fixture", 0}, folia::completion::make_prompt({}), {}, L"fixture-key");
        });
        expect(result.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
        expect(result.get().text == U" hello");
        expect(server.request.starts_with("POST /v1/chat/completions HTTP/1.1\r\n"));
        expect(server.request.size() < 8192_u);
        expect(server.request.find("application/json") != std::string::npos);
        expect(server.request.find("\"max_tokens\":128") != std::string::npos);
    };
    "completion HTTP uses the derived path for all three protocols"_test = [] {
        std::array<std::string, 3> paths{"/chat/completions", "/responses", "/messages"};
        for (int protocol = 0; protocol < paths.size(); ++protocol) {
            Server server({}, 503);
            bool failed = false;
            try { (void)RequestCompletionHttp({true, server.baseUrl + "/gateway/v2/?rev=1%2F2", "fixture", protocol},
                folia::completion::make_prompt({}), {}, L""); }
            catch (std::exception const& error) { failed = std::string(error.what()).find("503") != std::string::npos; }
            expect(failed);
            expect(server.request.starts_with("POST /gateway/v2" + paths[protocol] + "?rev=1%2F2 HTTP/1.1\r\n"));
        }
    };
    "completion HTTP async cancellation completes before receive timeout"_test = [] {
        Server server({}, 200, true); std::stop_source stop;
        auto result = std::async(std::launch::async, [&] {
            try { (void)RequestCompletionHttp({true, server.baseUrl, "fixture", 0}, folia::completion::make_prompt({}), stop.get_token(), L""); }
            catch (std::exception const&) {} return true;
        });
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!server.received && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
        expect(server.received.load()); stop.request_stop();
        expect(result.wait_for(std::chrono::milliseconds(1000)) == std::future_status::ready);
        expect(result.get());
    };
    "completion HTTP surfaces service errors without accepting a suggestion"_test = [] {
        Server server({}, 503);
        bool failed = false;
        try { (void)RequestCompletionHttp({true, server.baseUrl, "fixture", 0}, folia::completion::make_prompt({}), {}, L""); }
        catch (std::exception const& error) { failed = std::string(error.what()).find("503") != std::string::npos; }
        expect(failed);
        expect(!ValidateCompletionConfig({true, "http://localhost:80@remote.example/", "fixture", 0}).empty());
        expect(!ValidateCompletionConfig({true, "http://remote.example/", "fixture", 0}).empty());
    };
};
}
