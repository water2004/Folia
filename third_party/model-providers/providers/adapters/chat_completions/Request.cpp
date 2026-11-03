#include "../Adapters.h"
#include "Internal.h"

namespace fairy::providers {
RequestHeaders ChatCompletionsHeaders(std::string_view apiKey) {
    return {{"Authorization", "Bearer " + std::string(apiKey)}};
}
Json BuildChatCompletionsRequest(ModelRequest const& request) {
    ValidateRequest(request);
    if (!SupportsReasoningOptions(ApiProtocol::ChatCompletions, request.reasoning))
        throw ProtocolError("Reasoning options are unsupported for Chat Completions.");
    if (request.model.empty() || request.model.size() > 256 || request.messages.empty() ||
        request.messages.size() > MaxMessages || request.maxOutputTokens == 0 ||
        request.tools.size() > MaxToolCount)
        Invalid();
    Json messages = Json::array();
    for (auto const& m : request.messages) {
        Json out = {{"role", Role(m.role)}};
        if (m.role == MessageRole::Tool) {
            if (m.toolCallId.empty() || m.toolCallId.size() > 256 || !m.toolCalls.empty())
                Invalid();
            out["tool_call_id"] = m.toolCallId;
            if (m.content.size() != 1 || m.content[0].kind != ContentKind::Text)
                Invalid();
            out["content"] = m.content[0].text;
        } else {
            out["content"] = MessageContent(m);
            if (m.role == MessageRole::Assistant && !m.toolCalls.empty())
                out["tool_calls"] = MakeToolCalls(m.toolCalls);
            else if (!m.toolCalls.empty())
                Invalid();
            if (m.role == MessageRole::Assistant && m.providerState.is_object() &&
                m.providerState.contains("chat_message")) {
                auto const& replay = m.providerState["chat_message"];
                if (!replay.is_object())
                    Invalid();
                for (auto const& [key, value] : replay.items()) {
                    if (key == "role" || key == "content" || key == "tool_calls")
                        continue;
                    if (key.size() > 64 || (!value.is_string() && !value.is_null()))
                        Invalid();
                    if (value.is_string() && value.get<std::string>().size() > MaxTextBytes)
                        Invalid();
                    out[key] = value;
                }
            }
        }
        messages.push_back(std::move(out));
    }
    Json body = {{"model", request.model},
                 {"messages", std::move(messages)},
                 {"max_tokens", request.maxOutputTokens},
                 {"stream", request.stream}};
    if (!request.tools.empty()) {
        Json tools = Json::array();
        std::set<std::string> names;
        for (auto const& t : request.tools) {
            if (!ValidName(t.name) || !names.insert(t.name).second ||
                t.description.size() > MaxTextBytes || !t.inputSchema.is_object())
                Invalid();
            tools.push_back({{"type", "function"},
                             {"function",
                              {{"name", t.name},
                               {"description", t.description},
                               {"parameters", t.inputSchema}}}});
        }
        body["tools"] = std::move(tools);
        body["tool_choice"] = "auto";
    }
    if (request.stream)
        body["stream_options"] = {{"include_usage", true}};
    if (!request.reasoning.effort.empty())
        body["reasoning_effort"] = request.reasoning.effort;
    return body;
}
} // namespace fairy::providers
