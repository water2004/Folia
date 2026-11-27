#pragma once
#include "PromptCache.h"
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace folia::ai {
inline constexpr std::size_t maxRequestMessages = 512;
struct ContextWindowExceeded : std::runtime_error { using std::runtime_error::runtime_error; };
using Json = nlohmann::json;
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
    bool cacheBoundary = false;
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
enum class WebSearchDialect { Standard, DeepSeek, OpenRouter };
struct ProviderRequestOptions {
    WebSearchDialect search = WebSearchDialect::Standard;
    bool explicitThinkingToggle = false;
    CacheDialect cache = CacheDialect::Automatic;
};

enum class PageAccess { None, WithSearch, DeepSeek, Anthropic, OpenRouter };

struct ModelRequest {
    std::string model;
    std::vector<Message> messages;
    std::vector<ToolDefinition> tools;
    std::uint32_t maxOutputTokens{2048};
    bool stream{true};
    ReasoningOptions reasoning;
    bool webSearch = false;
    PageAccess pages = PageAccess::None;
    // Conversation requests allow a rolling cache boundary; inline prediction
    // can opt into writing only its stable document prefix.
    std::string cacheSession;
    bool stablePrefixOnly = false;
};
struct Usage {
    std::optional<std::uint64_t> inputTokens;
    std::optional<std::uint64_t> outputTokens;
    std::optional<std::uint64_t> totalTokens;
    std::optional<std::uint64_t> cachedInputTokens;
};
enum class FinishReason { Completed, ToolCalls, Truncated, Refused, Continue };
struct Citation {
    std::string title;
    std::string url;
};
struct HostedSearch {
    std::string id, target;
    bool failed = false;
    std::string operation = "web_search";
};
struct ModelResult {
    Message message{MessageRole::Assistant, {}, {}, {}, Json::object()};
    Usage usage;
    FinishReason finish{FinishReason::Completed};
    std::string responseId;
    std::vector<Citation> citations;
    std::vector<HostedSearch> searches;
};
enum class StreamEventKind { TextDelta, ReasoningDelta, ToolCallDelta, UsageUpdated, WebSearchStarted, WebSearchCompleted, WebSearchFailed };
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
} // namespace folia::ai
