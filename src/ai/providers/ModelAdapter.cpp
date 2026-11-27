module folia.ai.providers;
import std;
import :dispatch;

namespace folia::ai {
RequestHeaders BuildHeaders(ApiProtocol protocol, std::string_view apiKey) {
    switch (protocol) {
    case ApiProtocol::ChatCompletions:
        return ChatCompletionsHeaders(apiKey);
    case ApiProtocol::Responses:
        return ResponsesHeaders(apiKey);
    case ApiProtocol::AnthropicMessages:
        return AnthropicHeaders(apiKey);
    }
    throw ProtocolError("Unsupported API protocol");
}
Json BuildRequest(ApiProtocol protocol, ModelRequest const& request, ProviderRequestOptions options) {
    // Each protocol entry point owns validation; dispatch does not traverse the request again.
    Json body;
    switch (protocol) {
    case ApiProtocol::ChatCompletions:
        body = BuildChatCompletionsRequest(request, options); break;
    case ApiProtocol::Responses:
        body = BuildResponsesRequest(request, options); break;
    case ApiProtocol::AnthropicMessages:
        body = BuildAnthropicRequest(request, options); break;
    default: throw ProtocolError("Unsupported API protocol");
    }
    ApplyPromptCache(body, request, options, protocol);
    return body;
}
ModelResult ParseResponse(ApiProtocol protocol, Json const& response) {
    try {
        switch (protocol) {
        case ApiProtocol::ChatCompletions:
            return ParseChatCompletionsResponse(response);
        case ApiProtocol::Responses:
            return ParseResponsesResponse(response);
        case ApiProtocol::AnthropicMessages:
            return ParseAnthropicResponse(response);
        default:
            throw ProtocolError("Unsupported API protocol");
        }
    } catch (Json::exception const&) {
        throw ProtocolError("Invalid provider response");
    }
}
std::unique_ptr<IResponseDecoder> CreateStreamDecoder(ApiProtocol protocol) {
    switch (protocol) {
    case ApiProtocol::ChatCompletions:
        return CreateChatCompletionsDecoder();
    case ApiProtocol::Responses:
        return CreateResponsesDecoder();
    case ApiProtocol::AnthropicMessages:
        return CreateAnthropicDecoder();
    }
    throw ProtocolError("Unsupported API protocol");
}
} // namespace folia::ai
