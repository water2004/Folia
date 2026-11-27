module folia.ai.providers:dispatch;
import std;
import folia.ai.model;
import :protocol;

// Dispatch entry points shared by the three protocol adapters.
namespace folia::ai {
void ApplyPromptCache(Json& body, ModelRequest const& request, ProviderRequestOptions options, ApiProtocol protocol);
void MarkCacheBoundaries(Json& content, Message const& message, char const* textType);
inline bool UsesCacheBoundaries(ModelRequest const& request, ProviderRequestOptions options, ApiProtocol protocol) {
    return (!request.cacheSession.empty() || request.stablePrefixOnly) &&
           SupportsCacheBreakpoints(options.cache, protocol, request.model, request.stablePrefixOnly);
}
Json BuildChatCompletionsRequest(ModelRequest const& request, ProviderRequestOptions options);
RequestHeaders ChatCompletionsHeaders(std::string_view apiKey);
ModelResult ParseChatCompletionsResponse(Json const& response);
std::unique_ptr<IResponseDecoder> CreateChatCompletionsDecoder();

Json BuildResponsesRequest(ModelRequest const& request, ProviderRequestOptions options);
RequestHeaders ResponsesHeaders(std::string_view apiKey);
ModelResult ParseResponsesResponse(Json const& response);
std::unique_ptr<IResponseDecoder> CreateResponsesDecoder();

Json BuildAnthropicRequest(ModelRequest const& request, ProviderRequestOptions options);
RequestHeaders AnthropicHeaders(std::string_view apiKey);
ModelResult ParseAnthropicResponse(Json const& response);
std::unique_ptr<IResponseDecoder> CreateAnthropicDecoder();
} // namespace folia::ai
