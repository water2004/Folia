#pragma once
#include <string_view>

namespace folia::ai {
enum class ApiProtocol { ChatCompletions, Responses, AnthropicMessages };
enum class CacheDialect { Automatic, Custom, OpenAI, Anthropic, OpenRouter, RoutingKey };

bool SupportsCacheBreakpoints(CacheDialect dialect, ApiProtocol protocol, std::string_view model,
                                     bool explicitOptIn = false) noexcept;
}
