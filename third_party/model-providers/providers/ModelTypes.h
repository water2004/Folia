#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fairy::providers {
using Json = nlohmann::json;
enum class ApiProtocol { ChatCompletions, Responses, AnthropicMessages };
enum class ProtocolFailure {
    InvalidData,
    InvalidStreamEvent,
    InvalidStreamJson,
    InvalidStreamField,
    TrailingStreamEvent,
    LateStreamEvent,
    DuplicateStreamEvent,
    ConflictingStreamFragment,
    InvalidStreamIdentity,
    InvalidFunctionArguments,
    ConflictingSnapshot,
    InvalidTerminal,
    IncompleteStream,
    ProviderReportedFailure,
    StreamLimit
};
[[nodiscard]] std::string_view ProtocolFailureName(ProtocolFailure failure) noexcept;
[[nodiscard]] ProtocolFailure ParseProtocolFailure(std::string_view name) noexcept;
enum class MessageRole { System, User, Assistant, Tool };
enum class ContentKind { Text, Image };
enum class ReasoningKind { ProviderText, Summary };
struct ContentPart {
    ContentKind kind{ContentKind::Text};
    std::string text;
    std::string mediaType;
    std::string base64;
};
struct ToolCall {
    std::string id;
    std::string name;
    std::string arguments;
};
struct ReasoningContent {
    std::string text;
    std::string summary;
};
struct Message {
    MessageRole role{MessageRole::User};
    std::vector<ContentPart> content;
    std::vector<ToolCall> toolCalls;
    std::string toolCallId;
    // Protocol-owned replay state, never interpreted as host instructions.
    Json providerState = Json::object();
    // Provider-returned readable reasoning content for display only.
    ReasoningContent reasoning;
};
struct ToolDefinition {
    std::string name;
    std::string description;
    Json inputSchema = Json::object();
};
struct ReasoningOptions {
    std::string effort;
    bool summary{};
};
struct ModelRequest {
    std::string model;
    std::vector<Message> messages;
    std::vector<ToolDefinition> tools;
    std::uint32_t maxOutputTokens{2048};
    bool stream{true};
    ReasoningOptions reasoning;
};
struct Usage {
    std::optional<std::uint64_t> inputTokens;
    std::optional<std::uint64_t> outputTokens;
    std::optional<std::uint64_t> totalTokens;
    std::optional<std::uint64_t> cachedInputTokens;
};
enum class FinishReason { Completed, ToolCalls, Truncated, Refused };
struct ModelResult {
    Message message{MessageRole::Assistant, {}, {}, {}, Json::object()};
    Usage usage;
    FinishReason finish{FinishReason::Completed};
    std::string responseId;
};
enum class StreamEventKind { TextDelta, ReasoningDelta, ToolCallDelta, UsageUpdated };
struct StreamEvent {
    StreamEventKind kind{StreamEventKind::TextDelta};
    std::string text;
    std::string callId;
    std::string name;
    Usage usage;
    ReasoningKind reasoningKind{ReasoningKind::ProviderText};
};
class ProtocolError : public std::runtime_error {
  public:
    explicit ProtocolError(char const* message,
                           ProtocolFailure failure = ProtocolFailure::InvalidData);
    explicit ProtocolError(std::string message,
                           ProtocolFailure failure = ProtocolFailure::InvalidData);
    [[nodiscard]] ProtocolFailure Failure() const noexcept;

  private:
    ProtocolFailure failure_;
};
std::string ProtocolName(ApiProtocol protocol);
ApiProtocol ParseProtocol(std::string const& name);
std::string PlainText(Message const& message);
} // namespace fairy::providers
