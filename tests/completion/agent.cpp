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

        "agent grants the configured output allowance to every request"_test = [] {
            History history{{{MessageRole::User, "Read the sections"}}};
            Options options{"fixture"};
            options.outputTokens = 64'000;
            unsigned calls = 0;
            auto request = [&](ModelRequest const& wire, std::stop_token, Progress const&) {
                expect(wire.maxOutputTokens == 64'000_u);
                auto answer = Answer(++calls == 3 ? "Done" : "");
                answer.usage.outputTokens = 62'000;
                if (calls < 3) {
                    answer.finish = FinishReason::ToolCalls;
                    answer.message.toolCalls.push_back({std::to_string(calls), "read", "{}"});
                }
                return answer;
            };
            expect(Run(history, options, {}, request, {}, {}).markdown == "Done");
            expect(calls == 3_u);
            auto truncated = [](auto const&, auto, auto const&) {
                auto answer = Answer("");
                answer.message.reasoning.text = "Partial thought";
                answer.finish = FinishReason::Truncated;
                return answer;
            };
            auto partial = Run(history, options, {}, truncated, {}, {});
            expect(partial.outputLimit && partial.markdown.empty() && !partial.reasoning.empty());
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

        "agent applies a configured thinking switch independently of hosted search"_test = [] {
            ModelRequest wire;
            wire.model = "gateway-alias";
            wire.messages.push_back({MessageRole::User, {{ContentKind::Text, "Hello"}}});
            ProviderRequestOptions adapter{.explicitThinkingToggle=true};
            wire.reasoning.effort = "none";
            for (auto protocol : {ApiProtocol::AnthropicMessages, ApiProtocol::ChatCompletions}) {
                auto body = BuildRequest(protocol, wire, adapter);
                expect(body["thinking"]["type"] == "disabled");
                expect(!body.contains("output_config") && !body.contains("reasoning_effort"));
            }
            expect(!BuildRequest(ApiProtocol::ChatCompletions, wire).contains("thinking"));
            wire.reasoning.effort = "high";
            auto body = BuildRequest(ApiProtocol::ChatCompletions, wire, adapter);
            expect(body["thinking"]["type"] == "enabled" && body["reasoning_effort"] == "high");
            body = BuildRequest(ApiProtocol::Responses, wire, adapter);
            expect(!body.contains("thinking") && body["reasoning"]["effort"] == "high");
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

        "agent interleaves hosted searches and text once across streaming and the terminal snapshot"_test = [] {
            History history{{{MessageRole::User, "Find a source"}}};
            auto request = [](ModelRequest const&, std::stop_token, Progress const& show) {
                show(ProgressKind::TextDelta, "Before");
                show(ProgressKind::HostedSearch, R"({"id":"s1","target":"","done":false})");
                show(ProgressKind::HostedSearch, R"({"id":"s1","target":"example","done":true})");
                show(ProgressKind::TextDelta, "After");
                auto result = Answer("BeforeAfter");
                result.searches.push_back({"s1", "example", false});
                return result;
            };
            auto reply = Run(history, Options{"fixture"}, {}, request, {}, {});
            expect(reply.markdown == "BeforeAfter" && reply.activities.size() == 1_u);
            expect(reply.activities.front().target == "example" && !reply.activities.front().pending);
            expect(fatal(reply.steps.size() == 3_u));
            expect(reply.steps[0].text == 6_u && reply.steps[0].tools == 0_u);
            expect(reply.steps[1].text == 6_u && reply.steps[1].tools == 1_u);
            expect(reply.steps[2].text == 11_u && reply.steps[2].tools == 1_u);
        };

        "agent only labels provider supplied summaries and requests no summarization"_test = [] {
            auto turn = [](bool withSummary) {
                History history{{{MessageRole::User, "Read"}}};
                auto request = [&](ModelRequest const& wire, auto, Progress const& show) {
                    expect(!wire.reasoning.summary);
                    show(ProgressKind::ReasoningDelta, "detailed reasoning");
                    if (withSummary) show(ProgressKind::SummaryDelta, "Upstream label");
                    show(ProgressKind::TextDelta, "Answer");
                    auto response = Answer("Answer");
                    response.message.reasoning.text = "detailed reasoning";
                    if (withSummary) response.message.reasoning.summary = "Upstream label";
                    return response;
                };
                return Run(history, Options{"fixture"}, {}, request, {}, {});
            };
            auto raw = turn(false), labeled = turn(true);
            expect(raw.reasoningSummaries.empty() && raw.reasoning == "detailed reasoning");
            expect(fatal(labeled.reasoningSummaries.size() == 1_u));
            expect(labeled.reasoningSummaries[0].offset == 0_u && labeled.reasoningSummaries[0].text == "Upstream label");
            expect(labeled.reasoning == raw.reasoning);
        };

        "agent receives live hosted search lifecycle from Responses and Messages"_test = [] {
            auto responses = CreateStreamDecoder(ApiProtocol::Responses);
            auto added = responses->OnEvent({"response.output_item.added",
                R"({"output_index":0,"item":{"id":"s","type":"web_search_call","status":"in_progress"}})"});
            auto done = responses->OnEvent({"response.output_item.done",
                R"({"output_index":0,"item":{"id":"s","type":"web_search_call","status":"completed","action":{"query":"reading"}}})"});
            expect(fatal(added.size() == 1_u && done.size() == 1_u));
            expect(added.front().kind == StreamEventKind::WebSearchStarted && added.front().callId == "s");
            expect(done.front().kind == StreamEventKind::WebSearchCompleted && done.front().text == "reading");
            auto messages = CreateStreamDecoder(ApiProtocol::AnthropicMessages);
            messages->OnEvent({"message_start", R"({"message":{"id":"m","type":"message","role":"assistant"}})"});
            auto started = messages->OnEvent({"content_block_start", R"({"index":0,"content_block":{
                "id":"s","type":"server_tool_use","name":"web_search","input":{"query":"reading"}}})"});
            messages->OnEvent({"content_block_stop", R"({"index":0})"});
            auto failed = messages->OnEvent({"content_block_start", R"({"index":1,"content_block":{
                "type":"web_search_tool_result","tool_use_id":"s","content":{"type":"web_search_tool_result_error"}}})"});
            expect(fatal(started.size() == 1_u && failed.size() == 1_u));
            expect(started.front().kind == StreamEventKind::WebSearchStarted && started.front().text == "reading");
            expect(failed.front().kind == StreamEventKind::WebSearchFailed && failed.front().callId == "s");
        };

        "agent reads Chat Completions hosted search events, citations and replay"_test = [] {
            auto chat = CreateStreamDecoder(ApiProtocol::ChatCompletions);
            Json call{{"id","srv-search"},{"type","deepseek_web_search"},{"action",{{"queries",{"public docs","updates"}}}}};
            auto chunk = [](Json choice) {
                choice["index"] = 0;
                return SseEvent{{}, Json{{"choices",Json::array({std::move(choice)})}}.dump()};
            };
            auto event = [&](char const* type) {
                return chat->OnEvent(chunk({{"delta",Json::object()},
                    {"server_tool_event",{{"type",type},{"step",0},{"call",call}}}}));
            };
            auto start = event("call_started");
            call["status"] = "completed";
            auto done = event("call_completed");
            expect(fatal(start.size() == 1_u && done.size() == 1_u));
            expect(start.front().text == "public docs · updates" && start.front().kind == StreamEventKind::WebSearchStarted);
            expect(done.front().kind == StreamEventKind::WebSearchCompleted);
            Json item{{"type","url_citation"},{"url_citation",{{"url","https://example.test/source"},{"title","Source"}}}};
            Json steps = Json::array({{{"calls",Json::array({call})}}});
            Json message{{"role","assistant"},{"content","Answer"},{"server_tool_calls",steps},{"annotations",{item,item}}};
            chat->OnEvent(chunk({{"delta",message},{"finish_reason","stop"}}));
            chat->OnEvent({{}, "[DONE]"});
            Json full{{"choices",Json::array({{{"index",0},{"message",message},{"finish_reason","stop"}}})}};
            for (auto result : {chat->Finish(), ParseResponse(ApiProtocol::ChatCompletions, full)}) {
                expect(fatal(result.searches.size() == 1_u && result.citations.size() == 1_u));
                expect(result.searches.front().id == "srv-search" && !result.searches.front().failed);
                expect(result.citations.front().url == "https://example.test/source");
                ModelRequest next;
                next.model = "fixture";
                next.messages.push_back(result.message);
                expect(BuildRequest(ApiProtocol::ChatCompletions, next)["messages"][0]["server_tool_calls"] == steps);
            }
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
            expect(!partial.steps.empty() && partial.steps.back().text == partial.markdown.size() && partial.activities.size() == 1_u);
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
