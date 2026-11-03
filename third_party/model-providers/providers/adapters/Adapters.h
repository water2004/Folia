#pragma once

#include "../ModelAdapter.h"

// Internal protocol entry points. Public callers should use ModelAdapter.h.
namespace fairy::providers {
Json BuildChatCompletionsRequest(ModelRequest const& request);
RequestHeaders ChatCompletionsHeaders(std::string_view apiKey);
ModelResult ParseChatCompletionsResponse(Json const& response);
std::unique_ptr<IResponseDecoder> CreateChatCompletionsDecoder();

Json BuildResponsesRequest(ModelRequest const& request);
RequestHeaders ResponsesHeaders(std::string_view apiKey);
ModelResult ParseResponsesResponse(Json const& response);
std::unique_ptr<IResponseDecoder> CreateResponsesDecoder();

Json BuildAnthropicRequest(ModelRequest const& request);
RequestHeaders AnthropicHeaders(std::string_view apiKey);
ModelResult ParseAnthropicResponse(Json const& response);
std::unique_ptr<IResponseDecoder> CreateAnthropicDecoder();
} // namespace fairy::providers
