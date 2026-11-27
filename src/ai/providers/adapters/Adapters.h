#pragma once

#include "../ModelAdapter.h"

// Internal protocol entry points. Public callers should use ModelAdapter.h.
namespace folia::ai {
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
