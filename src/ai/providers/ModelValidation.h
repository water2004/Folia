#pragma once

#include "ModelTypes.h"
#include <string_view>

namespace folia::ai {
void ValidateRequest(ModelRequest const& request);
void ValidateResult(ModelResult const& result);
void ValidateReasoningText(std::string_view text);
// Readable text and summary share one bound, independently of opaque replay state.
void ValidateReasoningContent(ReasoningContent const& reasoning);
bool SupportsReasoningOptions(ApiProtocol protocol, ReasoningOptions const& options) noexcept;
constexpr bool SupportsOptionalHostedSearch(ApiProtocol protocol) noexcept {
    return protocol == ApiProtocol::Responses || protocol == ApiProtocol::AnthropicMessages;
}
} // namespace folia::ai
