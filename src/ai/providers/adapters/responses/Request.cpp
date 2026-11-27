module folia.ai.providers;
import std;
import :dispatch;
import :responses;

namespace folia::ai {
using namespace responses;

RequestHeaders ResponsesHeaders(std::string_view apiKey) {
    return {{"Authorization", "Bearer " + std::string(apiKey)}};
}
Json BuildResponsesRequest(ModelRequest const& request, ProviderRequestOptions options) {
    ValidateRequest(request);
    if (!SupportsReasoningOptions(ApiProtocol::Responses, request.reasoning))
        throw ProtocolError("Reasoning options are unsupported for Responses.");
    Json input = Json::array();
    const bool mark = UsesCacheBoundaries(request, options, ApiProtocol::Responses);
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
        // The plain-text message form accepts assistant history without
        // labeling it as user input or fabricating a provider output item.
        if (message.role == MessageRole::Assistant &&
            std::ranges::all_of(message.content, [](auto const& part) { return part.kind == ContentKind::Text; })) {
            input.push_back({{"type", "message"}, {"role", role}, {"content", PlainText(message)}});
        } else {
            auto content = InputContent(message);
            if (mark) {
                MarkCacheBoundaries(content, message, "input_text");
                if (message.role == MessageRole::System) role = "developer";
            }
            input.push_back({{"type", "message"}, {"role", role}, {"content", std::move(content)}});
        }
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
    if (request.webSearch)
        tools.push_back(HostedSearchTool(ApiProtocol::Responses, options.search));
    if (SeparatePageTool(request.pages)) tools.push_back(PageTool(ApiProtocol::Responses, request.pages));
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
    if (request.webSearch || !request.tools.empty() || SeparatePageTool(request.pages))
        body["include"] = Json::array({"reasoning.encrypted_content"});
    return body;
}
} // namespace folia::ai
