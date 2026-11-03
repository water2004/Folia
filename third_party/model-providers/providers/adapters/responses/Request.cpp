#include "../Adapters.h"
#include "Internal.h"

namespace fairy::providers {
RequestHeaders ResponsesHeaders(std::string_view apiKey) {
    return {{"Authorization", "Bearer " + std::string(apiKey)}};
}
Json BuildResponsesRequest(ModelRequest const& request) {
    ValidateRequest(request);
    if (!SupportsReasoningOptions(ApiProtocol::Responses, request.reasoning))
        throw ProtocolError("Reasoning options are unsupported for Responses.");
    Json input = Json::array();
    for (auto const& message : request.messages) {
        if (message.role == MessageRole::Tool) {
            input.push_back({{"type", "function_call_output"},
                             {"call_id", message.toolCallId},
                             {"output", PlainText(message)}});
            continue;
        }
        if (message.role == MessageRole::Assistant && message.providerState.is_object() &&
            message.providerState.contains("responses_output")) {
            auto const& prior = message.providerState["responses_output"];
            if (!prior.is_array())
                Bad("Responses replay state is invalid.");
            for (auto const& item : prior)
                input.push_back(item);
            continue;
        }
        auto role = message.role == MessageRole::System      ? "system"
                    : message.role == MessageRole::Assistant ? "assistant"
                                                             : "user";
        input.push_back({{"type", "message"}, {"role", role}, {"content", InputContent(message)}});
        if (message.role == MessageRole::Assistant)
            for (auto const& call : message.toolCalls) {
                if (call.id.empty() || call.name.empty())
                    Bad("Invalid function call in request history.");
                (void)ParseFunctionArguments(call.arguments);
                input.push_back({{"type", "function_call"},
                                 {"call_id", call.id},
                                 {"name", call.name},
                                 {"arguments", call.arguments}});
            }
    }
    Json tools = Json::array();
    // Preserve Fairy's common JSON Schema contract, including genuinely optional fields.
    // Responses otherwise attempts strict normalization; authoritative validation is local.
    for (auto const& tool : request.tools)
        tools.push_back({{"type", "function"},
                         {"name", tool.name},
                         {"description", tool.description},
                         {"strict", false},
                         {"parameters", tool.inputSchema}});
    Json body = {{"model", request.model},
                 {"input", std::move(input)},
                 {"tools", std::move(tools)},
                 {"max_output_tokens", request.maxOutputTokens},
                 {"store", false},
                 {"stream", request.stream}};
    if (!request.reasoning.effort.empty())
        body["reasoning"]["effort"] = request.reasoning.effort;
    if (request.reasoning.summary)
        body["reasoning"]["summary"] = "auto";
    return body;
}
} // namespace fairy::providers
