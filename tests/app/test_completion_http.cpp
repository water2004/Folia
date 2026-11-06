#include "pch.h"
#include "support/CompletionPromptSettings.h"
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
    "all protocols send one chronological document message with exact literal source"_test = [] {
        auto settings = LoadCompletionPromptSettings();
        settings.examples = {{"call(", ")", ";", "<insert>value</insert>"}};
        auto prompt = folia::completion::make_prompt({"nearby context\n\n", "x_", "} next\nmore"}, settings);
        for (int protocol = 0; protocol < 3; ++protocol) {
            Server server({}, 503);
            expect(throws([&] { (void)RequestCompletionHttp({true, server.baseUrl, "fixture", protocol}, prompt, {}, L"fixture-key"); }));
            auto body = server.request.substr(server.request.find("\r\n\r\n") + 4);
            auto json = winrt::Windows::Data::Json::JsonObject::Parse(winrt::to_hstring(body));
            expect(!json.HasKey(L"prompt_cache_options") && !body.contains("prompt_cache_breakpoint"));
            auto messages = json.GetNamedArray(protocol == 1 ? L"input" : L"messages");
            expect(messages.Size() == static_cast<unsigned>((protocol == 2 ? 1 : 2) + prompt.examples.size() * 2));
            auto user = messages.GetObjectAt(messages.Size() - 1);
            expect(user.GetNamedString(L"role") == L"user");
            auto content = user.GetNamedValue(L"content");
            auto text = content.ValueType() == winrt::Windows::Data::Json::JsonValueType::String
                ? content.GetString() : content.GetArray().GetObjectAt(0).GetNamedString(L"text");
            expect(winrt::to_string(text) == prompt.context + prompt.cursor);
            auto sampleAt = protocol == 2 ? 0u : 1u;
            expect(messages.GetObjectAt(sampleAt).GetNamedString(L"role") == L"user");
            auto sampleReply = messages.GetObjectAt(sampleAt + 1);
            expect(sampleReply.GetNamedString(L"role") == L"assistant");
            auto replyContent = sampleReply.GetNamedValue(L"content");
            if (protocol == 1) {
                expect(replyContent.ValueType() == winrt::Windows::Data::Json::JsonValueType::String);
                expect(content.GetArray().GetObjectAt(0).GetNamedString(L"type") == L"input_text");
            }
            auto replyText = replyContent.ValueType() == winrt::Windows::Data::Json::JsonValueType::String
                ? replyContent.GetString() : replyContent.GetArray().GetObjectAt(0).GetNamedString(L"text");
            expect(winrt::to_string(replyText) == prompt.examples[0].second);
        }
    };
    "explicit Responses cache marks stable context and preserves changing source in the same message"_test = [] {
        using winrt::Windows::Data::Json::JsonObject;
        auto captureRequest = [](folia::completion::Prompt const& prompt, int protocol) {
            Server server({}, 503);
            CompletionConfig config{true, server.baseUrl, "fixture", protocol};
            config.explicitPromptCache = true;
            expect(throws([&] { (void)RequestCompletionHttp(config, prompt, {}, L"fixture-key"); }));
            return server.request.substr(server.request.find("\r\n\r\n") + 4);
        };
        const auto before = U"Fixed earlier context.\n\nFirst line stays fixed.\n";
        auto promptFor = [&](std::u32string_view active) {
            auto source = std::u32string(before) + std::u32string(active) + U" next";
            return completion_tests::MakePrompt(folia::completion::capture(source, source.size() - 5, {}));
        };
        auto first = promptFor(U"x_");
        auto second = promptFor(U"x_{n");
        auto firstRequest = JsonObject::Parse(winrt::to_hstring(captureRequest(first, 1)));
        auto secondRequest = JsonObject::Parse(winrt::to_hstring(captureRequest(second, 1)));
        auto firstInput = firstRequest.GetNamedArray(L"input");
        auto secondInput = secondRequest.GetNamedArray(L"input");
        expect(firstInput.Size() == first.examples.size() * 2 + 2);
        expect(secondInput.Size() == firstInput.Size());
        for (unsigned index = 0; index + 1 < firstInput.Size(); ++index)
            expect(bool(firstInput.GetObjectAt(index).Stringify() == secondInput.GetObjectAt(index).Stringify()));
        auto parts = [](JsonObject const& request) {
            expect(request.GetNamedObject(L"prompt_cache_options").GetNamedString(L"mode") == L"explicit");
            auto input = request.GetNamedArray(L"input");
            auto user = input.GetObjectAt(input.Size() - 1);
            expect(user.GetNamedString(L"role") == L"user");
            auto content = user.GetNamedArray(L"content");
            expect(content.Size() == 2_u);
            return content;
        };
        auto a = parts(firstRequest);
        auto b = parts(secondRequest);
        expect(bool(a.GetObjectAt(0).Stringify() == b.GetObjectAt(0).Stringify()));
        expect(a.GetObjectAt(0).GetNamedObject(L"prompt_cache_breakpoint").GetNamedString(L"mode") == L"explicit");
        expect(a.GetObjectAt(0).GetNamedString(L"type") == L"input_text");
        expect(winrt::to_string(a.GetObjectAt(0).GetNamedString(L"text")) == first.context);
        expect(winrt::to_string(a.GetObjectAt(1).GetNamedString(L"text")) == first.cursor);
        expect(winrt::to_string(b.GetObjectAt(1).GetNamedString(L"text")) == second.cursor);
        expect(first.cursor != second.cursor);
        expect(!a.GetObjectAt(1).HasKey(L"prompt_cache_breakpoint"));
        for (int protocol : {0, 2}) {
            auto other = captureRequest(first, protocol);
            expect(!other.contains("prompt_cache_options") && !other.contains("prompt_cache_breakpoint"));
        }
        first.context.clear();
        auto withoutStableContext = captureRequest(first, 1);
        expect(withoutStableContext.contains("prompt_cache_options") && !withoutStableContext.contains("prompt_cache_breakpoint"));
    };
    "completion timeout counts inactivity rather than total stream duration"_test = [] {
        const auto inactivity = std::chrono::milliseconds(400);
        std::string reasoning;
        for (int i = 0; i < 10; ++i)
            reasoning += R"(data: {"choices":[{"index":0,"delta":{"reasoning_content":"thinking"},"finish_reason":null}]})" "\n\n";
        Server server(reasoning + response, 200, false, "text/event-stream", std::chrono::milliseconds(60), 60);
        const auto begin = std::chrono::steady_clock::now();
        auto result = RequestCompletionHttp({true, server.baseUrl, "fixture", 0}, completion_tests::MakePrompt({}), {}, L"fixture-key", inactivity);
        expect(bool(result.text == U" hello"));
        expect(std::chrono::steady_clock::now() - begin > inactivity * 2);
    };
    "completion terminal event completes without waiting for the connection to close"_test = [] {
        Server server(response, 200, true);
        const auto begin = std::chrono::steady_clock::now();
        auto result = RequestCompletionHttp({true, server.baseUrl, "fixture", 0}, completion_tests::MakePrompt({}), {}, L"fixture-key", std::chrono::milliseconds(400));
        expect(bool(result.text == U" hello"));
        expect(std::chrono::steady_clock::now() - begin < std::chrono::seconds(1));
    };
    "completion connection still times out when no response data arrives"_test = [] {
        Server server({}, 200, true);
        const auto begin = std::chrono::steady_clock::now();
        expect(throws([&] { (void)RequestCompletionHttp({true, server.baseUrl, "fixture", 0}, completion_tests::MakePrompt({}), {}, L"fixture-key", std::chrono::milliseconds(200)); }));
        expect(std::chrono::steady_clock::now() - begin < std::chrono::seconds(1));
    };
    "OpenAI model discovery never infers reasoning capabilities from IDs or private fields"_test = [] {
        for (int protocol : {0, 1}) {
            auto page = ParseCompletionModels(R"({"data":[{"id":"reasoning-high-model","owned_by":"fixture",
                "effort":{"supported_levels":["low","high"]},"capabilities":{"effort":{"supported":true,"high":{"supported":true}}}}]})", protocol);
            expect(page.models.size() == 1_u);
            expect(page.models[0].id == "reasoning-high-model");
            expect(page.models[0].reasoningEfforts.empty());
            expect(ParseCompletionModels(R"({"data":[{"id":"fixture"}]})", protocol).models[0].reasoningEfforts.empty());
            expect(ParseCompletionModels(R"({"data":[]})", protocol).models.empty());
        }
    };
    "completion model discovery does not require a selected model or valid generation budgets"_test = [] {
        for (int protocol = 0; protocol < 3; ++protocol) {
            Server server(R"({"data":[{"id":"fixture","effort":{"supported_levels":["low","high","max"]}}]})", 200, false, "application/json");
            CompletionConfig config{true, server.baseUrl + "/gateway/v2/?rev=1%2F2", "", protocol};
            config.outputTokens = 0;
            auto models = FetchCompletionModels(config, {}, L"fixture-key");
            expect(models.size() == 1_u);
            expect(models[0].reasoningEfforts.empty());
            expect(server.request.starts_with("GET /gateway/v2/models?rev=1%2F2 HTTP/1.1\r\n"));
            expect(server.request.find("Before cursor") == std::string::npos);
            expect(server.request.find(protocol == 2 ? "x-api-key: fixture-key" : "Authorization: Bearer fixture-key") != std::string::npos);
        }
    };
    "completion paths depend on protocol and explicit base only for every hostname"_test = [] {
        for (auto host : {"https://api.deepseek.com", "https://api.example.com", "https://API.DEEPSEEK.COM:443"})
            for (auto prefix : {"", "/v1", "/anthropic", "/anthropic/v1"})
                for (int protocol = 0; protocol < 3; ++protocol) {
                    auto base = std::string(host) + prefix + "/?rev=2";
                    auto resolvedBase = std::string(host) + (std::string_view(prefix).empty() ? "/v1" : prefix);
                    expect(ResolveCompletionModelsEndpoint({true, base, "", protocol}) == resolvedBase + "/models?rev=2");
                    constexpr std::array<std::string_view, 3> paths{"/chat/completions", "/responses", "/messages"};
                    expect(ResolveCompletionEndpoint({true, base, "", protocol}) == resolvedBase + std::string(paths[protocol]) + "?rev=2");
                }
    };
    "Anthropic model discovery reads only explicitly supported capabilities"_test = [] {
        auto page = ParseCompletionModels(R"({"data":[{"id":"fixture","display_name":"Fixture Claude","capabilities":{
            "effort":{"supported":true,"low":{"supported":true},"medium":{"supported":false},"max":{"supported":true}}}}],
            "has_more":true,"last_id":"model/a+b"})", 2);
        expect(page.next == "model/a+b"); expect(page.models[0].name == "Fixture Claude");
        expect(page.models[0].reasoningEfforts == std::vector<std::string>{"low", "max"});
        expect(ParseCompletionModels(R"({"data":[{"id":"a"},{"id":"a"}]})", 0).models.size() == 1_u);
        for (auto json : {R"({"data":[{}]})", R"({"data":[{"id":42}]})", R"({"data":{}})",
             R"({"data":[],"has_more":true})", R"({"data":[{"id":"a"}],"has_more":true})"})
            expect(throws([&] { (void)ParseCompletionModels(json, 2); }));
    };
    "completion model metadata cancellation releases a pending receive"_test = [] {
        Server server({}, 200, true, "application/json"); std::stop_source stop;
        auto result = std::async(std::launch::async, [&] {
            try { (void)FetchCompletionModels({true, server.baseUrl, "", 0}, stop.get_token(), L"fixture-key"); }
            catch (std::exception const&) {} return true;
        });
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!server.received && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
        expect(server.received.load()); stop.request_stop();
        expect(result.wait_for(std::chrono::milliseconds(1000)) == std::future_status::ready); expect(result.get());
    };
    "completion discovery follows declared pagination with encoded cursors and stable model identities"_test = [] {
        Server server(R"({"data":[{"id":"first"}],"has_more":true,"last_id":"model/a+b"})", 200, false,
            "application/json", {}, 37, {R"({"data":[{"id":"first"},{"id":"second","capabilities":{"effort":{"supported":true,"high":{"supported":true}}}}],"has_more":false})"});
        auto models = FetchCompletionModels({true, server.baseUrl + "/v1?rev=2", "", 2}, {}, L"fixture-key");
        expect(models.size() == 2_u); expect(models[0].id == "first" && models[1].id == "second");
        expect(models[1].reasoningEfforts == std::vector<std::string>{"high"});
        expect(server.requests.size() == 2_u);
        expect(server.requests[1].starts_with("GET /v1/models?rev=2&after_id=model%2Fa%2Bb HTTP/1.1\r\n"));
        expect(server.requests[1].find("x-api-key: fixture-key") != std::string::npos);
    };
    "completion discovery rejects repeated pagination instead of looping"_test = [] {
        const auto page = R"({"data":[{"id":"a"}],"has_more":true,"last_id":"same"})";
        Server server(page, 200, false, "application/json", {}, 37, {page});
        expect(throws([&] { (void)FetchCompletionModels({true, server.baseUrl, "", 2}, {}, L"fixture-key"); }));
        expect(server.requests.size() == 2_u);
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
        config.reasoningEffort = "custom_effort-2"; expect(ValidateCompletionConfig(config).empty());
        config.reasoningEffort = "bad value"; expect(!ValidateCompletionConfig(config).empty());
        config.reasoningEffort = ""; config.contextKB = 65; expect(!ValidateCompletionConfig(config).empty());
        config.contextKB = 7; config.outputTokens = 4097; expect(!ValidateCompletionConfig(config).empty());
    };
    "completion transport forwards reasoning effort and output limits for each protocol"_test = [] {
        for (int protocol = 0; protocol < 3; ++protocol) {
            Server server({}, 503);
            CompletionConfig config{true, server.baseUrl, "fixture", protocol}; config.reasoningEffort = "custom_effort-2"; config.outputTokens = 512;
            bool failed = false;
            try { (void)RequestCompletionHttp(config, completion_tests::MakePrompt({}, folia::completion::make_budget(16000, 512)), {}, L""); }
            catch (std::exception const&) { failed = true; }
            expect(failed);
            auto field = protocol == 0 ? "\"reasoning_effort\":\"custom_effort-2\""
                : protocol == 1 ? "\"reasoning\":{\"effort\":\"custom_effort-2\"}" : "\"output_config\":{\"effort\":\"custom_effort-2\"}";
            expect(server.request.find(field) != std::string::npos);
            expect((server.request.find("\"reasoning_effort\"") != std::string::npos) == (protocol == 0));
            expect((server.request.find("\"output_config\"") != std::string::npos) == (protocol == 2));
            expect(server.request.find(protocol == 1 ? "\"max_output_tokens\":512" : "\"max_tokens\":512") != std::string::npos);
        }
    };
    "completion service default omits all effort fields on every protocol"_test = [] {
        for (int protocol = 0; protocol < 3; ++protocol) {
            Server server({}, 503);
            expect(throws([&] { (void)RequestCompletionHttp({true, server.baseUrl, "fixture", protocol},
                completion_tests::MakePrompt({}), {}, L"fixture-key"); }));
            for (auto field : {"\"reasoning_effort\"", "\"reasoning\"", "\"output_config\"", "\"thinking\""})
                expect(server.request.find(field) == std::string::npos);
        }
    };
    "completion transport preserves token-limit termination"_test = [] {
        auto limited = response; limited.replace(limited.find("stop"), 4, "length");
        Server server(limited);
        auto result = RequestCompletionHttp({true, server.baseUrl, "fixture", 0}, completion_tests::MakePrompt({}), {}, L"");
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
            return RequestCompletionHttp({true, server.baseUrl, "fixture", 0}, completion_tests::MakePrompt({}), {}, L"fixture-key");
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
                completion_tests::MakePrompt({}), {}, L""); }
            catch (std::exception const& error) { failed = std::string(error.what()).find("503") != std::string::npos; }
            expect(failed);
            expect(server.request.starts_with("POST /gateway/v2" + paths[protocol] + "?rev=1%2F2 HTTP/1.1\r\n"));
        }
    };
    "completion HTTP async cancellation completes before receive timeout"_test = [] {
        Server server({}, 200, true); std::stop_source stop;
        auto result = std::async(std::launch::async, [&] {
            try { (void)RequestCompletionHttp({true, server.baseUrl, "fixture", 0}, completion_tests::MakePrompt({}), stop.get_token(), L""); }
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
        try { (void)RequestCompletionHttp({true, server.baseUrl, "fixture", 0}, completion_tests::MakePrompt({}), {}, L""); }
        catch (std::exception const& error) { failed = std::string(error.what()).find("503") != std::string::npos; }
        expect(failed);
        expect(!ValidateCompletionConfig({true, "http://localhost:80@remote.example/", "fixture", 0}).empty());
        expect(!ValidateCompletionConfig({true, "http://remote.example/", "fixture", 0}).empty());
    };
    "completion HTTP reports structured error details without echoing the response message"_test = [] {
        for (auto const& [body, expected] : std::array<std::pair<std::string, std::string>, 3>{
            std::pair{R"({"error":{"type":"invalid_request_error","code":"invalid_value","param":"input[2].content[0].type","message":"private fixture text"}})",
                " (type: invalid_request_error, code: invalid_value, param: input[2].content[0].type)"},
            std::pair{R"({"error":{"code":null,"param":"invalid field","message":"private fixture text"}})", ""},
            std::pair{std::string(8193, ' '), ""},
        }) {
            Server server(body, 400, false, "application/json");
            std::string error;
            try { (void)RequestCompletionHttp({true, server.baseUrl, "fixture", 1}, completion_tests::MakePrompt({}), {}, L"fixture-key"); }
            catch (std::exception const& failure) { error = failure.what(); }
            expect(error == "Completion service returned HTTP 400" + expected);
        }
    };
};
}
