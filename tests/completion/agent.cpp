#include "support/folia_test.hpp"

import folia.ai.agent;
import folia.ai.providers;

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

        "agent forwards a complete directory page with long names and its continuation offset"_test = [] {
            History history{{{MessageRole::User, "List the workspace"}}};
            std::string namePrefix;
            for (int i = 0; i < 160; ++i) namePrefix += "长";
            Json entries = Json::array();
            for (int i = 0; i < 40; ++i)
                entries.push_back({{"name", namePrefix + std::to_string(i) + ".md"}, {"directory", false}});
            const Json page{{"entries", std::move(entries)}, {"next_offset", 40}};
            expect(page.dump().size() > 12'000_u);

            Tool list{{"list_workspace", "List a page of workspace entries", Json::object()},
                [&](Json const&, std::stop_token) { return page; }};
            auto request = [&](ModelRequest const& wire, std::stop_token, Progress const&) {
                auto const& message = wire.messages.back();
                if (message.role == MessageRole::User) {
                    ModelResult result;
                    result.finish = FinishReason::ToolCalls;
                    result.message.toolCalls = {{"directory-page", "list_workspace", "{}"}};
                    return result;
                }
                expect(message.toolCallId == "directory-page");
                expect(Json::parse(PlainText(message)) == page);
                return Answer("Done");
            };
            auto reply = Run(history, Options{"fixture"}, std::span(&list, 1), request, {}, {});
            expect(reply.markdown == "Done");
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
                history.entries.push_back({MessageRole::Assistant, std::string(140, 'a') + std::to_string(i)});
            }
            history.entries.push_back({MessageRole::User, "Latest question"});
            int steps = 0;
            auto request = [&](ModelRequest const& wire, std::stop_token, Progress const&) {
                ++steps;
                if (wire.cacheSession.empty()) {
                    expect(wire.tools.empty());
                    expect(!wire.webSearch);
                    const auto input = Json::parse(PlainText(wire.messages.back()));
                    expect(!input.at("transcript").empty());
                    return Answer("Summary");
                }
                expect(PlainText(wire.messages.back()) == "Latest question");
                expect(PlainText(wire.messages[1]).ends_with("Summary"));
                return Answer("New answer");
            };
            Options options{"fixture"};
            options.contextTokens = 1500;
            options.cacheSession = "session";
            expect(Run(history, options, {}, request, {}, {}).markdown == "New answer");
            expect(history.entries.size() == 41_u);
            expect(history.summarized > 20_u);
            expect(steps > 1_i);
        };

        "short turns keep their cache prefix when web access changes"_test = [] {
            History history;
            for (unsigned i = 0; i < 100; ++i) {
                history.entries.push_back({MessageRole::User, std::to_string(i)});
                history.entries.push_back({MessageRole::Assistant, "OK"});
            }
            history.entries.push_back({MessageRole::User, "Continue"});
            Options options{"fixture"};
            options.cacheSession = "same-conversation";
            std::vector<std::string> prefix;
            auto reply = [&](ModelRequest const& wire, auto, auto const&) {
                expect(wire.cacheSession == options.cacheSession);
                expect(wire.messages.size() == 202_u && wire.messages.front().content.front().cacheBoundary);
                auto texts = wire.messages | std::views::transform([](auto const& m) { return PlainText(m); })
                    | std::ranges::to<std::vector<std::string>>();
                if (prefix.empty()) prefix = texts;
                else expect(texts == prefix);
                return Answer("OK");
            };
            Run(history, options, {}, reply, {}, {});
            options.webSearch = WebSearchDialect::Standard;
            Run(history, options, {}, reply, {}, {});
            expect(history.summarized == 0_u);
        };

        "an upstream context rejection reduces history and preserves the pending tool pair"_test = [] {
            History history{{{MessageRole::User, "Prior"}, {MessageRole::Assistant, "Decision"}, {MessageRole::User, "Current"}}};
            Options options{"fixture"};
            options.cacheSession = "same-session";
            unsigned invocations = 0, requests = 0, summaries = 0;
            Tool tool{{"read", "Read a range", Json::object()}, [&](Json const&, auto) {
                ++invocations;
                return Json{{"result", "the active range"}};
            }};
            auto request = [&](ModelRequest const& wire, auto, auto const&) {
                if (wire.cacheSession.empty()) {
                    ++summaries;
                    return Answer("Prior decision");
                }
                ++requests;
                if (requests == 1) {
                    auto answer = Answer("");
                    answer.finish = FinishReason::ToolCalls;
                    answer.message.toolCalls = {{"read-1", "read", "{}"}};
                    return answer;
                }
                if (requests == 2) throw ContextWindowExceeded("context_length_exceeded");
                expect(wire.messages.back().toolCallId == "read-1");
                expect(wire.messages[wire.messages.size() - 2].toolCalls.front().id == "read-1");
                expect(PlainText(wire.messages[1]).ends_with("Prior decision"));
                expect(PlainText(wire.messages[2]) == "Current");
                return Answer("Done");
            };
            expect(Run(history, options, std::span(&tool, 1), request, {}, {}).markdown == "Done");
            expect(invocations == 1_u && requests == 3_u && summaries == 1_u);
        };

        "manual compaction keeps the transcript and checkpoints only complete turns"_test = [] {
            History history{{{MessageRole::User, "Remember page seven"}, {MessageRole::Assistant, "Recorded"},
                             {MessageRole::User, "Next question"}}};
            int requests = 0;
            auto summarize = [&](ModelRequest const& wire, std::stop_token, Progress const&) {
                ++requests;
                expect(wire.tools.empty() && !wire.webSearch);
                expect(PlainText(wire.messages.back()).find("page seven") != std::string::npos);
                expect(PlainText(wire.messages.back()).find("Next question") == std::string::npos);
                return Answer("Page seven");
            };
            expect(Compact(history, Options{"fixture"}, summarize, {}));
            expect(history.summarized == 2_u && history.entries.size() == 3_u);
            expect(!Compact(history, Options{"fixture"}, summarize, {}));
            expect(requests == 1_i);
            auto next = [](ModelRequest const& wire, std::stop_token, Progress const&) {
                expect(PlainText(wire.messages[1]).find("Page seven") != std::string::npos);
                expect(PlainText(wire.messages.back()) == "Next question");
                return Answer("Done");
            };
            expect(Run(history, Options{"fixture"}, {}, next, {}, {}).markdown == "Done");
            history.entries.push_back({MessageRole::Assistant, "Done"});
            std::stop_source cancel;
            auto stopped = [&](auto const&, auto, auto const&) { cancel.request_stop(); return Answer("discard"); };
            expect(throws([&] { Compact(history, Options{"fixture"}, stopped, cancel.get_token()); }));
            expect(history.summary == "Page seven" && history.summarized == 2_u);
        };

        "agent completes long tool sequences including repeated arguments and a server pause"_test = [] {
            History history{{{MessageRole::User, "Work"}}};
            constexpr int toolRounds = 12;
            int requests = 0, invocations = 0;
            Tool tool{{"read_document", "Read", Json::object()}, [&](Json const&, std::stop_token) {
                ++invocations;
                return Json::object();
            }};
            auto request = [&](ModelRequest const& wire, std::stop_token, Progress const&) {
                if (requests > 0 && requests <= toolRounds) {
                    expect(wire.messages[wire.messages.size() - 2].toolCallId == std::to_string(requests * 2 - 1));
                    expect(wire.messages.back().toolCallId == std::to_string(requests * 2));
                }
                ++requests;
                if (requests == toolRounds + 1) {
                    auto paused = Answer("Still working");
                    paused.finish = FinishReason::Continue;
                    return paused;
                }
                if (requests > toolRounds + 1) {
                    expect(PlainText(wire.messages.back()) == "Still working");
                    return Answer("Done");
                }
                ModelResult result;
                result.finish = FinishReason::ToolCalls;
                result.message.toolCalls = {
                    {std::to_string(requests * 2 - 1), "read_document", "{}"},
                    {std::to_string(requests * 2), "read_document", "{}"}};
                return result;
            };
            auto reply = Run(history, Options{"fixture"}, std::span(&tool, 1), request, {}, {});
            expect(reply.markdown == "Still working\n\nDone");
            expect(reply.activities.size() == 24_u);
            expect(invocations == 24_i && requests == 14_i);
        };

        "agent rejects a reused tool call ID before dispatching the batch"_test = [] {
            History history{{{MessageRole::User, "Work"}}};
            int requests = 0, invocations = 0;
            Tool tool{{"read_document", "Read", Json::object()}, [&](Json const&, std::stop_token) {
                ++invocations;
                return Json::object();
            }};
            auto request = [&](ModelRequest const&, std::stop_token, Progress const&) {
                if (++requests > 2) return Answer("Done");
                ModelResult result;
                result.finish = FinishReason::ToolCalls;
                if (requests == 2) result.message.toolCalls.push_back({"second", "read_document", "{}"});
                result.message.toolCalls.push_back({"first", "read_document", "{}"});
                return result;
            };
            expect(throws([&] { (void)Run(history, Options{"fixture"}, std::span(&tool, 1), request, {}, {}); }));
            expect(invocations == 1_i && requests == 2_i);
        };

        "stopping an agent prevents the next queued tool and model request"_test = [] {
            History history{{{MessageRole::User, "Work"}}};
            std::stop_source cancel;
            int requests = 0, invocations = 0;
            Tool tool{{"read_document", "Read", Json::object()}, [&](Json const&, std::stop_token) {
                ++invocations;
                cancel.request_stop();
                return Json::object();
            }};
            auto request = [&](ModelRequest const&, std::stop_token, Progress const&) {
                if (++requests > 1) return Answer("Done");
                ModelResult result;
                result.finish = FinishReason::ToolCalls;
                result.message.toolCalls = {{"first", "read_document", "{}"}, {"second", "read_document", "{}"}};
                return result;
            };
            expect(throws([&] { (void)Run(history, Options{"fixture"}, std::span(&tool, 1), request, {}, cancel.get_token()); }));
            expect(invocations == 1_i && requests == 1_i);
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

        "agent encodes independent URL tools and replays page activity through the model transport"_test = [] {
            ModelRequest wire;
            wire.model = "configured-alias";
            wire.messages.emplace_back().content.push_back({ContentKind::Text, "Read a URL"});
            wire.pages = PageAccess::WithSearch;
            wire.webSearch = true;
            auto bundled = BuildRequest(ApiProtocol::Responses, wire);
            expect(bundled["tools"].size() == 1_u && bundled["tools"][0]["type"] == "web_search");
            wire.webSearch = false;
            for (auto protocol : {ApiProtocol::Responses, ApiProtocol::ChatCompletions, ApiProtocol::AnthropicMessages}) {
                wire.pages = PageAccess::DeepSeek;
                auto payload = BuildRequest(protocol, RequestFromJson(ToJson(wire)));
                expect(payload["tools"].size() == 1_u && payload["tools"][0]["type"] == "deepseek_read_url");
                if (protocol == ApiProtocol::AnthropicMessages) expect(payload["tools"][0]["name"] == "deepseek_read_url");
            }
            wire.pages = PageAccess::Anthropic;
            expect(BuildRequest(ApiProtocol::AnthropicMessages, wire)["tools"][0]["type"] == "web_fetch_20250910");
            wire.pages = PageAccess::OpenRouter;
            for (auto protocol : {ApiProtocol::Responses, ApiProtocol::ChatCompletions})
                expect(BuildRequest(protocol, wire)["tools"][0]["type"] == "openrouter:web_fetch");
            auto result = ParseResponse(ApiProtocol::Responses, Json::parse(R"({
                "id":"r","status":"completed","output":[{"type":"web_search_call","id":"page","status":"completed",
                "action":{"type":"open_page","url":"https://example.org/"},"page":{"url":"https://example.org/","title":"Example"}},
                {"type":"message","role":"assistant","content":[{"type":"output_text","text":"Example"}]}]})"));
            auto copy = ResultFromJson(ToJson(result));
            expect(copy.searches.size() == 1_u && copy.searches.front().operation == "web_fetch" && copy.citations.size() == 1_u);
            StreamEvent notice{StreamEventKind::WebSearchCompleted, "https://example.org/", "page", "web_fetch"};
            expect(StreamEventFromJson(ToJson(notice)).name == "web_fetch");
            auto denied = ParseResponse(ApiProtocol::AnthropicMessages, Json::parse(R"({"type":"message","id":"r","role":"assistant",
                "stop_reason":"end_turn","usage":{},"content":[{"type":"server_tool_use","id":"f","name":"web_fetch","input":{"url":"https://example.org/"}},
                {"type":"web_fetch_tool_result","tool_use_id":"f","content":{"type":"web_fetch_tool_result_error","error_code":"url_not_allowed"}},
                {"type":"text","text":"Unable to read"}]})"));
            expect(denied.searches.size() == 1_u && denied.searches[0].failed && denied.searches[0].operation == "web_fetch");
        };

        "agent can read a URL without granting search or sharing local context in the isolated request"_test = [] {
            Tool local{{"list_open_tabs","List tabs",{{"type","object"}}}, [](auto const&, auto) { return Json::array(); }};
            Options options{"alias", "none"};
            options.pages = PageAccess::DeepSeek;
            History history{{{MessageRole::User, "private draft"}, {MessageRole::Assistant, "Reviewed"}, {MessageRole::User, "Read https://example.org/"}}};
            unsigned requests = 0;
            auto send = [&](ModelRequest const& wire, auto, Progress const&) {
                ++requests;
                expect(!wire.webSearch);
                if (wire.pages != PageAccess::None) {
                    expect(wire.tools.empty() && wire.messages.size() == 2_u && PlainText(wire.messages.back()) == "https://example.org/");
                    auto fetched = Answer("Example");
                    fetched.searches.push_back({"f","https://example.org/",false,"web_fetch"});
                    fetched.citations.push_back({"Example", "https://example.org/"});
                    return fetched;
                }
                expect(wire.tools.size() == 2_u && wire.tools.back().name == "web_fetch");
                if (requests == 1) {
                    auto call = Answer("");
                    call.finish = FinishReason::ToolCalls;
                    call.message.toolCalls.push_back({"call","web_fetch",R"({"url":"https://example.org/"})"});
                    return call;
                }
                return Answer("An example page.");
            };
            auto answer = Run(history, options, std::span(&local,1), send, {}, {});
            expect(requests == 3_u && answer.activities.size() == 1_u && answer.activities.front().name == "web_fetch");
            expect(answer.sources.size() == 1_u && answer.sources.front().url == "https://example.org/");
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

        "agent keeps local tools and selected reasoning when the search format requires an isolated query"_test = [] {
            Tool local{{"list_open_tabs","List tabs",{{"type","object"}}}, [](auto const&, auto) { return Json::array(); }};
            Options options{"proxy-alias", "none", WebSearchDialect::DeepSeek};
            History history{{{MessageRole::User, "A local private draft"}, {MessageRole::Assistant, "Reviewed"}, {MessageRole::User,"Find public docs"}}};
            unsigned requests = 0;
            auto request = [&](ModelRequest const& wire, auto, Progress const&) {
                ++requests;
                expect(wire.reasoning.effort == "none");
                if (wire.webSearch) {
                    expect(wire.tools.empty() && wire.messages.size() == 2_u);
                    expect(PlainText(wire.messages.back()) == "public Python documentation");
                    auto result = Answer("Official docs.");
                    result.searches.push_back({"external", "Python", false});
                    result.citations.push_back({"Python", "https://docs.python.org/"});
                    return result;
                }
                expect(wire.tools.size() == 2_u && wire.tools.back().name == "web_search");
                if (requests == 1) {
                    auto result = Answer("");
                    result.finish = FinishReason::ToolCalls;
                    result.message.toolCalls.push_back({"s", "web_search", R"({"query":"public Python documentation"})"});
                    return result;
                }
                expect(PlainText(wire.messages.back()).find("https://docs.python.org/") != std::string::npos);
                return Answer("Docs");
            };
            auto turn = Run(history, options, std::span(&local, 1), request, {}, {});
            expect(requests == 3_u && turn.markdown == "Docs");
            expect(turn.sources.size() == 1_u && turn.sources.front().title == "Python");
            expect(turn.activities.size() == 1_u && !turn.activities.front().failed);
            options.effort = "high";
            auto together = [&](ModelRequest const& wire, auto, Progress const&) {
                expect(wire.webSearch && wire.tools.size() == 1_u && wire.reasoning.effort == "high");
                return Answer("Together");
            };
            expect(Run(history, options, std::span(&local, 1), together, {}, {}).markdown == "Together");
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
            auto reply = Run(history, Options{"fixture", {}, WebSearchDialect::Standard}, {}, request, {}, {});
            expect(reply.markdown == "Search finished");
            expect(reply.activities.size() == 1_u && reply.activities.front().detail == "example");
            expect(steps == 2_i);
        };
    };
}
