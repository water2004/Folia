#include "ModelTypes.h"

#include <array>
#include <utility>

namespace fairy::providers {
namespace {
using ProtocolFailureEntry = std::pair<std::string_view, ProtocolFailure>;
constexpr std::array<ProtocolFailureEntry, 15> kProtocolFailures{{
    {"invalid_data", ProtocolFailure::InvalidData},
    {"invalid_stream_event", ProtocolFailure::InvalidStreamEvent},
    {"invalid_stream_json", ProtocolFailure::InvalidStreamJson},
    {"invalid_stream_field", ProtocolFailure::InvalidStreamField},
    {"trailing_stream_event", ProtocolFailure::TrailingStreamEvent},
    {"late_stream_event", ProtocolFailure::LateStreamEvent},
    {"duplicate_stream_event", ProtocolFailure::DuplicateStreamEvent},
    {"conflicting_stream_fragment", ProtocolFailure::ConflictingStreamFragment},
    {"invalid_stream_identity", ProtocolFailure::InvalidStreamIdentity},
    {"invalid_function_arguments", ProtocolFailure::InvalidFunctionArguments},
    {"conflicting_snapshot", ProtocolFailure::ConflictingSnapshot},
    {"invalid_terminal", ProtocolFailure::InvalidTerminal},
    {"incomplete_stream", ProtocolFailure::IncompleteStream},
    {"provider_reported_failure", ProtocolFailure::ProviderReportedFailure},
    {"stream_limit", ProtocolFailure::StreamLimit},
}};
} // namespace

std::string_view ProtocolFailureName(ProtocolFailure failure) noexcept {
    for (auto const& [name, code] : kProtocolFailures)
        if (code == failure)
            return name;
    return kProtocolFailures.front().first;
}

ProtocolFailure ParseProtocolFailure(std::string_view name) noexcept {
    for (auto const& [code, failure] : kProtocolFailures)
        if (code == name)
            return failure;
    return kProtocolFailures.front().second;
}

ProtocolError::ProtocolError(char const* message, ProtocolFailure failure)
    : std::runtime_error(message), failure_(failure) {}

ProtocolError::ProtocolError(std::string message, ProtocolFailure failure)
    : std::runtime_error(std::move(message)), failure_(failure) {}

ProtocolFailure ProtocolError::Failure() const noexcept {
    return failure_;
}

std::string ProtocolName(ApiProtocol protocol) {
    switch (protocol) {
    case ApiProtocol::ChatCompletions:
        return "chat-completions";
    case ApiProtocol::Responses:
        return "responses";
    case ApiProtocol::AnthropicMessages:
        return "anthropic-messages";
    }
    throw ProtocolError("Invalid API protocol");
}

ApiProtocol ParseProtocol(std::string const& name) {
    if (name == "chat-completions")
        return ApiProtocol::ChatCompletions;
    if (name == "responses")
        return ApiProtocol::Responses;
    if (name == "anthropic-messages")
        return ApiProtocol::AnthropicMessages;
    throw ProtocolError("Invalid API protocol");
}

std::string PlainText(Message const& message) {
    std::string text;
    for (auto const& part : message.content)
        if (part.kind == ContentKind::Text)
            text += part.text;
    return text;
}

} // namespace fairy::providers
