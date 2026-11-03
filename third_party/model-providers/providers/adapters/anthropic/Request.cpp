#include "../Adapters.h"
#include "Internal.h"

namespace fairy::providers {
RequestHeaders AnthropicHeaders(std::string_view apiKey) {
    return {{"x-api-key", std::string(apiKey)}, {"anthropic-version", "2023-06-01"}};
}
Json BuildAnthropicRequest(ModelRequest const& request) {
    ValidateRequest(request);
    if (!SupportsReasoningOptions(ApiProtocol::AnthropicMessages, request.reasoning))
        throw ProtocolError("Reasoning options are unsupported for Anthropic Messages.");
    Json out = {{"model", request.model},
                {"max_tokens", request.maxOutputTokens},
                {"stream", request.stream}};
    Json system = Json::array(), messages = Json::array();
    for (auto const& m : request.messages) {
        if (m.role == MessageRole::System) {
            for (auto const& p : m.content)
                if (p.kind == ContentKind::Text)
                    system.push_back(TextBlock(p.text));
            continue;
        }
        if (m.role == MessageRole::Tool) {
            Json result = {
                {"type", "tool_result"}, {"tool_use_id", m.toolCallId}, {"content", PlainText(m)}};
            messages.push_back(Json{{"role", "user"}, {"content", Json::array({result})}});
            continue;
        }
        Json content = Json::array();
        if (m.role == MessageRole::Assistant && m.providerState.is_object() &&
            m.providerState.contains("anthropic_content")) {
            auto const& raw = m.providerState["anthropic_content"];
            if (!raw.is_array())
                Bad();
            content = raw;
        } else {
            for (auto const& p : m.content) {
                if (p.kind == ContentKind::Text)
                    content.push_back(TextBlock(p.text));
                else
                    content.push_back(Json{{"type", "image"},
                                           {"source", Json{{"type", "base64"},
                                                           {"media_type", p.mediaType},
                                                           {"data", p.base64}}}});
            }
            if (m.role == MessageRole::Assistant)
                for (auto const& call : m.toolCalls) {
                    auto input = ParseObject(call.arguments);
                    content.push_back(Json{{"type", "tool_use"},
                                           {"id", call.id},
                                           {"name", call.name},
                                           {"input", std::move(input)}});
                }
        }
        messages.push_back(Json{{"role", RoleName(m.role)}, {"content", std::move(content)}});
    }
    if (!system.empty())
        out["system"] = std::move(system);
    out["messages"] = std::move(messages);
    if (!request.tools.empty()) {
        out["tools"] = Json::array();
        for (auto const& t : request.tools)
            out["tools"].push_back(Json{
                {"name", t.name}, {"description", t.description}, {"input_schema", t.inputSchema}});
    }
    if (!request.reasoning.effort.empty())
        out["output_config"] = {{"effort", request.reasoning.effort}};
    // Explicit opt-in targets the adaptive-thinking API, not legacy token-budget thinking.
    // Compatible services/models may reject it; preserve their failure rather than guessing.
    if (request.reasoning.summary)
        out["thinking"] = {{"type", "adaptive"}, {"display", "summarized"}};
    return out;
}

} // namespace fairy::providers
