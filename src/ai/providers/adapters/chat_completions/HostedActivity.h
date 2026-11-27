#pragma once

#include "../SearchAction.h"
#include <algorithm>

namespace folia::ai::chat_search {
inline void ReadAnnotations(Json const& message, ModelResult& result) {
    auto annotations = message.find("annotations");
    if (annotations == message.end() || !annotations->is_array()) return;
    for (auto const& item : *annotations) {
        auto citation = item.find("url_citation");
        if (!item.is_object() || item.value("type", "") != "url_citation" ||
            citation == item.end() || !citation->is_object()) continue;
        auto url = citation->value("url", "");
        if (url.empty() || result.citations.size() == 64 ||
            std::ranges::find(result.citations, url, &Citation::url) != result.citations.end()) continue;
        result.citations.push_back({citation->value("title", ""), std::move(url)});
    }
}

inline bool IsSearchCall(Json const& call) {
    return call.is_object() && call.value("type", "") == "deepseek_web_search";
}

inline void ReadSteps(Json const& message, ModelResult& result) {
    ReadAnnotations(message, result);
    auto steps = message.find("server_tool_calls");
    if (steps == message.end() || steps->is_null()) return;
    if (!steps->is_array() || steps->size() > 128 || steps->dump().size() > 4 * 1024 * 1024)
        throw ProtocolError("Invalid hosted tool history");
    result.message.providerState["chat_message"]["server_tool_calls"] = *steps;
    for (auto const& step : *steps) {
        auto calls = step.find("calls");
        if (calls == step.end() || !calls->is_array()) continue;
        for (auto const& call : *calls) {
            if (!IsSearchCall(call)) continue;
            auto id = call.value("id", "");
            if (id.empty() || result.searches.size() == 32 ||
                std::ranges::find(result.searches, id, &HostedSearch::id) != result.searches.end()) continue;
            result.searches.push_back({std::move(id), SearchTarget(call, "action"), call.value("status", "") != "completed"});
        }
    }
}

inline void ReadEvent(Json const& choice, std::vector<StreamEvent>& events) {
    auto notice = choice.find("server_tool_event");
    if (notice == choice.end() || !notice->is_object()) return;
    auto call = notice->find("call");
    if (call == notice->end() || !IsSearchCall(*call)) return;
    auto type = notice->value("type", "");
    if (type != "call_started" && type != "call_completed") return;
    auto id = call->value("id", "");
    if (id.empty()) return;
    auto kind = StreamEventKind::WebSearchStarted;
    if (type == "call_completed")
        kind = call->value("status", "") == "completed" ? StreamEventKind::WebSearchCompleted : StreamEventKind::WebSearchFailed;
    events.push_back({kind, SearchTarget(*call, "action"), std::move(id)});
}
}
