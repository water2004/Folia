#include "ModelAdapter.h"
#include "adapters/Adapters.h"
namespace fairy::providers {
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
Json BuildRequest(ApiProtocol protocol, ModelRequest const& request) {
    // Each protocol entry point owns validation; dispatch does not traverse the request again.
    switch (protocol) {
    case ApiProtocol::ChatCompletions:
        return BuildChatCompletionsRequest(request);
    case ApiProtocol::Responses:
        return BuildResponsesRequest(request);
    case ApiProtocol::AnthropicMessages:
        return BuildAnthropicRequest(request);
    }
    throw ProtocolError("Unsupported API protocol");
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
} // namespace fairy::providers
