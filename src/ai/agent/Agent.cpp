module folia.ai.agent;
import std;
import folia.ai.agent.context;
import folia.ai.agent.hosted_tools;
import folia.ai.providers;

namespace folia::ai::agent {
    namespace {
        constexpr std::size_t maxReceiptBytes = 12'000;
        Message Text(MessageRole role, std::string text) {
            Message message;
            message.role = role;
            message.content.push_back({ContentKind::Text, std::move(text)});
            return message;
        }

        void CheckStop(std::stop_token stop) {
            if (stop.stop_requested()) throw std::runtime_error("Cancelled");
        }

        std::string ToolPreview(Json const& body) {
            std::string display;
            for (auto key : {"error", "text"}) {
                if (auto it = body.find(key); it != body.end() && it->is_string())
                    display = it->get<std::string>();
            }
            for (auto key : {"matches", "headings", "entries", "tabs"}) {
                auto items = body.find(key);
                if (items == body.end() || !items->is_array()) continue;
                for (auto const& item : *items) {
                    for (auto field : {"snippet", "heading", "name", "title"}) {
                        auto it = item.find(field);
                        if (it == item.end() || !it->is_string()) continue;
                        if (!display.empty()) display += "\n";
                        if (item.contains("offset")) display += "@" + item.at("offset").dump() + "  ";
                        display += it->get<std::string>();
                    }
                    if (display.size() > 1024) break;
                }
            }
            if (display.empty()) display = body.dump();
            if (display.size() > 1024) {
                std::size_t end = 1024;
                while ((static_cast<unsigned char>(display[end]) & 0xc0) == 0x80) --end;
                display.resize(end);
                display += "…";
            }
            return display;
        }

        Message Receipt(ToolCall const& call, std::span<Tool const> tools, std::stop_token stop) {
            auto tool = std::ranges::find_if(tools, [&](Tool const& entry) { return entry.definition.name == call.name; });
            Json body;
            if (tool == tools.end()) body = {{"error", "Tool unavailable in this conversation"}};
            else {
                try {
                    body = tool->invoke(Json::parse(call.arguments), stop);
                } catch (std::exception const&) {
                    body = {{"error", "Tool could not complete the requested operation"}};
                }
            }
            auto json = body.dump(-1, ' ', false, Json::error_handler_t::replace);
            if (json.size() > maxReceiptBytes) json = R"({"error":"Result too large; request a smaller range"})";
            auto receipt = Text(MessageRole::Tool, std::move(json));
            receipt.toolCallId = call.id;
            return receipt;
        }
    }

    Entry Run(History& history, Options const& options, std::span<Tool const> tools,
              Request const& request, Progress const& progress, std::stop_token stop, Entry* partial) {
        CheckStop(stop);
        ModelRequest wire;
        wire.model = options.model;
        wire.cacheSession = options.cacheSession;
        wire.reasoning.effort = options.effort;

        Entry owned{MessageRole::Assistant};
        auto& reply = partial ? *partial : owned;
        std::vector<Tool> combined;
        if (NeedsIsolatedWebRequest(options, tools)) {
            combined.assign(tools.begin(), tools.end());
            if (options.webSearch) combined.push_back(PublicWebTool(WebTask::Search, options, request, reply.sources));
            if (options.pages != PageAccess::None) combined.push_back(PublicWebTool(WebTask::ReadPage, options, request, reply.sources));
            tools = combined;
        } else {
            wire.webSearch = options.webSearch.has_value();
            wire.pages = options.pages;
        }
        for (auto const& tool : tools) wire.tools.push_back(tool.definition);
        auto boundary = [&] { return StepEnd{reply.markdown.size(), reply.reasoning.size(), reply.activities.size()}; };
        auto notify = [&](ProgressKind kind, std::string_view value) {
            if (kind == ProgressKind::Compacting || kind == ProgressKind::Retrying) {
                auto marker = boundary();
                marker.notice = kind == ProgressKind::Compacting ? StepEnd::Notice::Compaction : StepEnd::Notice::Retry;
                if (kind == ProgressKind::Retrying) std::from_chars(value.data(), value.data() + value.size(), marker.attempt);
                reply.steps.push_back(marker);
            }
            if (progress) progress(kind, value);
        };
        wire.messages = ProjectHistory(history, options);
        auto historySize = wire.messages.size();
        std::set<std::string> seenCallIds;
        wire.maxOutputTokens = options.outputTokens;
        for (;;) {
            CheckStop(stop);
            FitContext(wire, historySize, history, options, request, notify, stop);
            if (progress) progress(ProgressKind::StepStarted, {});
            const auto textStart = reply.markdown.size(), reasoningStart = reply.reasoning.size();
            const auto firstMarker = reply.steps.size();
            auto lastKind = ProgressKind::StepStarted;
            std::size_t thoughtOffset = reasoningStart;
            std::optional<bool> bodyIsSummary;
            std::string streamedSummary;
            auto append = [&](ProgressKind kind, std::string_view value) {
                if (value.empty()) return;
                const bool text = kind == ProgressKind::TextDelta;
                auto& buffer = text ? reply.markdown : reply.reasoning;
                auto start = text ? textStart : reasoningStart;
                if (!text && lastKind != kind) thoughtOffset = buffer.size();
                if (start && buffer.size() == start) buffer += "\n\n";
                buffer.append(value);
                if (lastKind == kind && !reply.steps.empty() && reply.steps.back().notice == StepEnd::Notice::None)
                    reply.steps.back() = boundary();
                else reply.steps.push_back(boundary());
                lastKind = kind;
            };
            std::map<std::string, std::size_t> searches;
            auto hostedSearch = [&](Json update) {
                auto id = update.at("id").get<std::string>();
                auto [it, inserted] = searches.try_emplace(id, reply.activities.size());
                if (inserted) {
                    reply.activities.push_back({"web_search", {}, false, {}, true});
                    reply.steps.push_back(boundary());
                    lastKind = ProgressKind::HostedSearch;
                }
                auto& activity = reply.activities[it->second];
                auto operation = update.value("operation", "");
                const bool changedAction = !operation.empty() && activity.name != operation;
                if (changedAction) activity.name = std::move(operation);
                update["operation"] = activity.name;
                auto target = update.value("target", "");
                const bool changedTarget = !target.empty() && activity.target != target;
                if (!target.empty()) activity.target = std::move(target);
                else update["target"] = activity.target;
                bool done = update.value("done", false), failed = update.value("failed", false);
                if (!inserted && !changedAction && !changedTarget && !activity.pending && activity.failed == failed) return;
                activity.pending = !done;
                activity.failed = failed;
                if (done) activity.detail = activity.target;
                if (progress) progress(ProgressKind::HostedSearch, update.dump());
            };
            constexpr std::size_t captionBytes = 384;
            auto caption = [&](std::string_view summary) {
                auto count = (std::min)(captionBytes, summary.size());
                while (count && count < summary.size() && (static_cast<unsigned char>(summary[count]) & 0xC0) == 0x80) --count;
                summary = summary.substr(0, count);
                if (summary.empty()) return;
                auto& captions = reply.reasoningSummaries;
                if (captions.empty() || captions.back().offset != thoughtOffset) captions.push_back({thoughtOffset, {}});
                if (captions.back().text == summary) return;
                captions.back().text = summary;
                if (progress) progress(ProgressKind::ReasoningSummary, summary);
            };
            auto stream = [&](ProgressKind kind, std::string_view value) {
                if (kind == ProgressKind::SummaryDelta || kind == ProgressKind::ReasoningDelta) {
                    if (value.empty()) return;
                    bool summary = kind == ProgressKind::SummaryDelta;
                    if (!bodyIsSummary) bodyIsSummary = summary;
                    if (*bodyIsSummary == summary) {
                        append(ProgressKind::ReasoningDelta, value);
                        notify(ProgressKind::ReasoningDelta, value);
                    }
                    if (summary && streamedSummary.size() < captionBytes) {
                        streamedSummary.append(value);
                        caption(streamedSummary);
                    }
                    return;
                }
                if (kind == ProgressKind::HostedSearch) { hostedSearch(Json::parse(value)); return; }
                if (kind == ProgressKind::TextDelta) append(kind, value);
                notify(kind, value);
            };
            ModelResult result;
            try {
                result = request(wire, stop, stream);
            } catch (ContextWindowExceeded const&) {
                CheckStop(stop);
                if (!FitContext(wire, historySize, history, options, request, notify, stop, true)) throw;
                result = request(wire, stop, stream);
            }
            CheckStop(stop);
            ValidateResult(result);
            auto finishLane = [&](bool text, std::string_view final) {
                if (final.empty()) return;
                auto& buffer = text ? reply.markdown : reply.reasoning;
                auto start = text ? textStart : reasoningStart;
                auto deltaKind = text ? ProgressKind::TextDelta : ProgressKind::ReasoningDelta;
                auto already = buffer.size() == start ? std::string_view{} :
                    std::string_view(buffer).substr(start + (start ? 2 : 0));
                if (final.starts_with(already)) append(deltaKind, final.substr(already.size()));
                else {
                    // A terminal provider snapshot can revise its streamed preview.
                    auto prefix = start + (start ? 2 : 0);
                    buffer.replace(prefix, buffer.size() - prefix, final);
                    for (auto& marker : std::span(reply.steps).subspan(firstMarker)) {
                        auto& end = text ? marker.text : marker.reasoning;
                        end = std::min(end, buffer.size());
                    }
                    reply.steps.push_back(boundary());
                }
                if (progress) progress(text ? ProgressKind::TextSnapshot : ProgressKind::ReasoningSnapshot, final);
            };
            reply.usage = result.usage;
            if (progress && result.usage.inputTokens)
                progress(ProgressKind::InputUsage, std::to_string(*result.usage.inputTokens));
            for (auto const& search : result.searches)
                hostedSearch(Json{{"id", search.id}, {"target", search.target}, {"operation", search.operation}, {"done", true}, {"failed", search.failed}});
            auto const& reasoning = result.message.reasoning;
            finishLane(false, bodyIsSummary.value_or(reasoning.text.empty()) ? reasoning.summary : reasoning.text);
            if (!reasoning.summary.empty() && reasoning.summary != streamedSummary) caption(reasoning.summary);
            finishLane(true, PlainText(result.message));
            for (auto const& citation : result.citations)
                if (reply.sources.size() < 32 && std::ranges::find(reply.sources, citation.url, &Source::url) == reply.sources.end())
                    reply.sources.push_back({citation.title, citation.url});
            if (result.finish == FinishReason::Continue) {
                wire.messages.push_back(std::move(result.message));
                continue;
            }
            if (result.finish != FinishReason::ToolCalls) {
                reply.outputLimit = result.finish == FinishReason::Truncated;
                if (reply.markdown.empty() && !reply.outputLimit) throw std::runtime_error("The service returned no answer");
                return std::move(reply);
            }
            for (auto const& call : result.message.toolCalls)
                if (!seenCallIds.insert(call.id).second)
                    throw ProtocolError("The service reused a tool call ID");
            auto batch = result.message.toolCalls;
            wire.messages.push_back(std::move(result.message));
            for (auto const& call : batch) {
                CheckStop(stop);
                auto args = Json::parse(call.arguments);
                ToolActivity activity{call.name};
                auto descriptor = std::ranges::find(tools, call.name, [](Tool const& t) -> auto const& { return t.definition.name; });
                if (descriptor != tools.end() && descriptor->target) activity.target = descriptor->target(args);
                else if (args.contains("path") && args["path"].is_string()) activity.target = args["path"].get<std::string>();
                else if (args.contains("id") && args["id"].is_number_unsigned()) activity.target = "#" + args["id"].dump();
                if (auto query = args.find("query"); query != args.end() && query->is_string())
                    activity.target += (activity.target.empty() ? "" : " · ") + query->get<std::string>();
                auto description = call.name + " " + activity.target;
                if (progress) progress(ProgressKind::Reading, description);
                auto receipt = Receipt(call, tools, stop);
                auto body = Json::parse(PlainText(receipt), nullptr, false);
                activity.failed = body.contains("error");
                activity.detail = ToolPreview(body);
                if (progress) progress(ProgressKind::ToolResult, Json{{"action", description},
                    {"failed", activity.failed}, {"detail", activity.detail}}.dump());
                reply.activities.push_back(std::move(activity));
                wire.messages.push_back(std::move(receipt));
                reply.steps.push_back(boundary());
            }
        }
    }
}
