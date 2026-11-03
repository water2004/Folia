#pragma once

#include "ModelTypes.h"
#include <string_view>

namespace fairy::providers {
void ValidateRequest(ModelRequest const& request);
void ValidateResult(ModelResult const& result);
void ValidateReasoningText(std::string_view text);
// Readable text and summary share one bound, independently of opaque replay state.
void ValidateReasoningContent(ReasoningContent const& reasoning);
bool SupportsReasoningOptions(ApiProtocol protocol, ReasoningOptions const& options) noexcept;
} // namespace fairy::providers
