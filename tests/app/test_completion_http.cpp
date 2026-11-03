#include "pch.h"
#include "support/folia_test.hpp"
#include "completion/CompletionHttpTransport.h"
#include "support/CompletionHttpServer.h"

namespace {
using namespace boost::ut;
using namespace winrt::Folia;
using completion_tests::Server;
std::string const response = R"(data: {"choices":[{"index":0,"delta":{"content":" hello"},"finish_reason":null}]})" "\n\n"
    R"(data: {"choices":[{"index":0,"delta":{},"finish_reason":"stop"}]})" "\n\n"
    "data: [DONE]\n\n";
suite completion_http_tests = [] {
    "completion reasoning metadata comes from the selected upstream model in declared order"_test = [] {
        auto levels = ParseCompletionReasoningEfforts(R"({"data":[{"id":"other","effort":{"supported_levels":["medium"]}},
            {"id":"fixture","effort":{"supported_levels":["low","max","low"]}}]})", "fixture");
        expect(levels == std::vector<std::string>{"low", "max"});
        expect(ParseCompletionReasoningEfforts(R"({"data":[{"id":"fixture"}]})", "fixture").empty());
        expect(throws([] { (void)ParseCompletionReasoningEfforts(R"({"data":[]})", "missing"); }));
        expect(throws([] { (void)ParseCompletionReasoningEfforts(R"({"data":[{"id":"fixture","effort":{"supported_levels":[42]}}]})", "fixture"); }));
    };
    "completion reasoning fetch uses a cancellable models GET without document content"_test = [] {
        Server server(R"({"data":[{"id":"fixture","effort":{"supported_levels":["low","high","max"]}}]})", 200, false, "application/json");
        CompletionConfig config{true, server.baseUrl + "/gateway/v2/?rev=1%2F2", "fixture", 1};
        auto levels = FetchCompletionReasoningEfforts(config, {}, L"fixture-key");
        expect(levels == std::vector<std::string>{"low", "high", "max"});
        expect(server.request.starts_with("GET /gateway/v2/models?rev=1%2F2 HTTP/1.1\r\n"));
        expect(server.request.find("Before cursor") == std::string::npos);
    };
    "completion model metadata cancellation releases a pending receive"_test = [] {
        Server server({}, 200, true, "application/json"); std::stop_source stop;
        auto result = std::async(std::launch::async, [&] {
            try { (void)FetchCompletionReasoningEfforts({true, server.baseUrl, "fixture", 0}, stop.get_token(), L"fixture-key"); }
            catch (std::exception const&) {} return true;
        });
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!server.received && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
        expect(server.received.load()); stop.request_stop();
        expect(result.wait_for(std::chrono::milliseconds(1000)) == std::future_status::ready); expect(result.get());
    };
    "completion validates protocol reasoning choices and configurable request budgets"_test = [] {
        for (int protocol = 0; protocol < 3; ++protocol) {
            std::vector<std::string> efforts = protocol == 2 ? std::vector<std::string>{"", "low", "medium", "high", "xhigh", "max"}
                : std::vector<std::string>{"", "none", "minimal", "low", "medium", "high", "xhigh", "max"};
            expect(!efforts.empty() && efforts.front().empty());
            for (auto const& effort : efforts) {
                CompletionConfig config{true, "https://service.example/v1", "fixture", protocol};
                config.reasoningEffort = effort; config.contextKB = 64; config.outputTokens = 4096;
                expect(ValidateCompletionConfig(config).empty());
            }
        }
        CompletionConfig config{true, "https://service.example", "fixture", 2};
        config.reasoningEffort = "none"; expect(!ValidateCompletionConfig(config).empty());
        config.reasoningEffort = ""; config.contextKB = 65; expect(!ValidateCompletionConfig(config).empty());
        config.contextKB = 7; config.outputTokens = 4097; expect(!ValidateCompletionConfig(config).empty());
    };
    "completion transport forwards reasoning effort and output limits for each protocol"_test = [] {
        for (int protocol = 0; protocol < 3; ++protocol) {
            Server server({}, 503);
            CompletionConfig config{true, server.baseUrl, "fixture", protocol}; config.reasoningEffort = "high"; config.outputTokens = 512;
            bool failed = false;
            try { (void)RequestCompletionHttp(config, folia::completion::make_prompt({}, folia::completion::make_budget(16000, 512)), {}, L""); }
            catch (std::exception const&) { failed = true; }
            expect(failed);
            auto field = protocol == 0 ? "\"reasoning_effort\":\"high\"" : "\"effort\":\"high\"";
            expect(server.request.find(field) != std::string::npos);
            expect(server.request.find(protocol == 1 ? "\"max_output_tokens\":512" : "\"max_tokens\":512") != std::string::npos);
        }
    };
    "completion transport preserves token-limit termination"_test = [] {
        auto limited = response; limited.replace(limited.find("stop"), 4, "length");
        Server server(limited);
        auto result = RequestCompletionHttp({true, server.baseUrl, "fixture", 0}, folia::completion::make_prompt({}), {}, L"");
        expect(result.truncated); expect(result.text == U" hello");
    };
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
