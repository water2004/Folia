export module folia.ai.providers:validation;
import std;
import folia.ai.model;

export namespace folia::ai {
void ValidateRequest(ModelRequest const& request);
void ValidateResult(ModelResult const& result);
void ValidateReasoningText(std::string_view text);
// Readable text and summary share one bound, independently of opaque replay state.
void ValidateReasoningContent(ReasoningContent const& reasoning);
bool SupportsReasoningOptions(ApiProtocol protocol, ReasoningOptions const& options) noexcept;
} // namespace folia::ai
