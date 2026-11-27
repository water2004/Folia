#include "agent/Agent.h"
#include "providers/ModelAdapter.h"
#include "providers/StreamResponse.h"
#include "support/folia_test.hpp"

namespace {
    using namespace boost::ut;
    using namespace folia::ai;
    using namespace folia::ai::agent;

    ModelResult Answer(std::string text) {
        ModelResult response;
        response.message.content.push_back({ContentKind::Text, std::move(text)});
        response.usage.outputTokens = 100;
        return response;
    }

    suite agent_tests = [] {
        "agent correlates a completed batch and rejects tools outside the current grants"_test = [] {
            History history{{{MessageRole::User, "Find text"}}};
            Tool tool{{"read_document", "Read", Json::object()}, [](Json const& input, std::stop_token) {
                return Json{{"text", input.at("id")}};
            }};
            int steps = 0;
            auto request = [&](ModelRequest const& wire, std::stop_token, Progress const&) {
                expect(wire.tools.size() == 1_u);
                if (++steps == 1) {
                    ModelResult result;
                    result.finish = FinishReason::ToolCalls;
                    result.message.toolCalls = {{"one", "read_document", R"({"id":"permitted"})"},
                        {"two", "write_document", R"({"id":"denied"})"}};
                    return result;
                }
                const auto& allowed = wire.messages[wire.messages.size() - 2];
                const auto& denied = wire.messages.back();
                expect(allowed.toolCallId == "one");
                expect(denied.toolCallId == "two");
                expect(Json::parse(PlainText(allowed)).at("text") == "permitted");
                expect(Json::parse(PlainText(denied)).contains("error"));
                return Answer("**Result**");
            };
            auto reply = Run(history, Options{"fixture"}, std::span(&tool, 1), request, {}, {});
            expect(reply.markdown == "**Result**");
            expect(history.entries.size() == 1_u);
            expect(steps == 2_i);
        };

        "agent compacts prior history without changing the last question or its transcript"_test = [] {
            History history;
            for (int i = 0; i < 20; ++i) {
                history.entries.push_back({MessageRole::User, "Question " + std::to_string(i)});
                history.entries.push_back({MessageRole::Assistant, "Answer " + std::to_string(i)});
            }
            history.entries.push_back({MessageRole::User, "Latest question"});
            int steps = 0;
            auto request = [&](ModelRequest const& wire, std::stop_token, Progress const&) {
                if (++steps == 1) {
                    expect(wire.tools.empty());
                    expect(!wire.webSearch);
                    const auto input = Json::parse(PlainText(wire.messages.back()));
                    expect(input.at("transcript").size() > 20_u);
                    return Answer("Summary");
                }
                expect(PlainText(wire.messages.back()) == "Latest question");
                expect(PlainText(wire.messages[1]).ends_with("Summary"));
                return Answer("New answer");
            };
            expect(Run(history, Options{"fixture"}, {}, request, {}, {}).markdown == "New answer");
            expect(history.entries.size() == 41_u);
            expect(history.summarized > 20_u);
            expect(steps == 2_i);
        };

        "agent enforces tool repetition limits before dispatching repeated calls"_test = [] {
            History history{{{MessageRole::User, "Work"}}};
            int called = 0;
            Tool tool{{"read_document", "Read", Json::object()}, [&](Json const&, std::stop_token) {
                ++called;
                return Json::object();
            }};
            auto request = [index = 0](ModelRequest const&, std::stop_token, Progress const&) mutable {
                ModelResult result;
                result.finish = FinishReason::ToolCalls;
                result.message.toolCalls.push_back({std::to_string(++index), "read_document", "{}"});
                result.usage.outputTokens = 100;
                return result;
            };
            expect(throws([&] { (void)Run(history, Options{"fixture"}, std::span(&tool, 1), request, {}, {}); }));
            expect(called == 2_i);
        };

        "agent encodes hosted search by protocol and preserves citation metadata"_test = [] {
            ModelRequest request;
            request.model = "fixture";
            request.webSearch = true;
            request.messages.emplace_back().content.push_back({ContentKind::Text, "Search"});
            auto responses = BuildRequest(ApiProtocol::Responses, request);
            auto anthropic = BuildRequest(ApiProtocol::AnthropicMessages, request);
            expect(responses["tools"][0]["type"] == "web_search");
            expect(anthropic["tools"][0]["type"] == "web_search_20250305");
            auto result = ParseResponse(ApiProtocol::Responses, Json::parse(R"({
                "id":"response","status":"completed","output":[{"type":"web_search_call","id":"s1","status":"completed",
                "action":{"type":"search","queries":["document format"]}},{"type":"message","role":"assistant",
                "content":[{"type":"output_text","text":"Source","annotations":[{"type":"url_citation",
                "url":"https://example.test","title":"Example","start_index":0,"end_index":6}]}]}]})"));
            expect(result.searches.size() == 1_u && result.searches.front().target == "document format");
            expect(result.citations.size() == 1_u);
            if (!result.citations.empty()) expect(result.citations.front().url == "https://example.test");
        };

        "agent preserves partial streamed text and tool steps after interruption"_test = [] {
            History history{{{MessageRole::User, "Read my note"}}};
            Entry partial{MessageRole::Assistant};
            int steps = 0;
            auto request = [&](ModelRequest const&, std::stop_token, Progress const& show) -> ModelResult {
                if (++steps == 1) {
                    ModelResult result;
                    result.finish = FinishReason::ToolCalls;
                    result.message.toolCalls = {{"call", "read_document", "{}"}};
                    return result;
                }
                show(ProgressKind::ReasoningDelta, "Retrieved a source");
                show(ProgressKind::TextDelta, "First paragraph");
                throw std::runtime_error("Disconnected");
            };
            expect(throws([&] { Run(history, Options{"fixture"}, {}, request, {}, {}, &partial); }));
            expect(partial.markdown == "First paragraph" && partial.reasoning == "Retrieved a source");
            expect(partial.steps.size() == 1_u && partial.activities.size() == 1_u);
        };

        "agent replays a hosted Anthropic search pause without dispatching a local function"_test = [] {
            History history{{{MessageRole::User, "Look it up"}}};
            auto response = Json::parse(R"({"type":"message","role":"assistant","id":"m","stop_reason":"pause_turn",
                "usage":{"output_tokens":100},"content":[{"type":"server_tool_use","name":"web_search","id":"server-use","input":{"query":"example"}},
                {"type":"web_search_tool_result","tool_use_id":"server-use","content":[]}]})");
            auto steps = 0;
            auto request = [&](ModelRequest const& wire, std::stop_token, Progress const&) {
                if (++steps == 1) return ParseResponse(ApiProtocol::AnthropicMessages, response);
                auto replay = BuildRequest(ApiProtocol::AnthropicMessages, wire);
                expect(replay["messages"].back()["content"] == response["content"]);
                expect(replay["tools"].size() == 1_u);
                return Answer("Search finished");
            };
            auto reply = Run(history, Options{"fixture", {}, true}, {}, request, {}, {});
            expect(reply.markdown == "Search finished");
            expect(reply.activities.size() == 1_u && reply.activities.front().detail == "example");
            expect(steps == 2_i);
        };
    };
}
