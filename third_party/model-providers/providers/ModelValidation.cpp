#include "ModelValidation.h"

#include "JsonParsing.h"

#include <algorithm>
#include <iterator>
#include <set>

namespace fairy::providers {
namespace {
constexpr std::size_t MaxReasoningBytes = 1024u * 1024u;
bool ValidEffort(std::string_view effort) {
    // Effort values are supplied by model metadata or explicitly by the user.
    // The service validates its vocabulary; the adapter validates the token.
    return effort.size() <= 32 && std::all_of(effort.begin(), effort.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    });
}
bool Identifier(std::string const& name) {
    return !name.empty() && name.size() <= 64 &&
           std::all_of(name.begin(), name.end(), [](unsigned char c) {
               return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                      c == '_' || c == '-';
           });
}
} // namespace

void ValidateReasoningText(std::string_view text) {
    if (text.size() > MaxReasoningBytes || text.find('\0') != std::string_view::npos)
        throw ProtocolError("Invalid reasoning text");
    try {
        (void)Json(std::string(text)).dump();
    } catch (Json::exception const&) {
        throw ProtocolError("Invalid reasoning text");
    }
}

void ValidateReasoningContent(ReasoningContent const& reasoning) {
    ValidateReasoningText(reasoning.text);
    ValidateReasoningText(reasoning.summary);
    if (reasoning.text.size() > MaxReasoningBytes - reasoning.summary.size())
        throw ProtocolError("Invalid reasoning text");
}

bool SupportsReasoningOptions(ApiProtocol protocol, ReasoningOptions const& options) noexcept {
    if (!ValidEffort(options.effort))
        return false;
    switch (protocol) {
    case ApiProtocol::ChatCompletions:
        return !options.summary;
    case ApiProtocol::Responses:
        return true;
    case ApiProtocol::AnthropicMessages:
        return true;
    }
    return false;
}

void ValidateRequest(ModelRequest const& request) {
    if (request.model.empty() || request.model.size() > 128 ||
        request.model.find_first_of("\r\n") != std::string::npos || request.messages.empty() ||
        request.messages.size() > 128 || request.tools.size() > 32 || request.maxOutputTokens == 0)
        throw ProtocolError("Invalid request limits");
    if (!ValidEffort(request.reasoning.effort))
        throw ProtocolError("Invalid reasoning effort");
    std::size_t bytes{};
    for (auto const& message : request.messages) {
        if (message.content.size() > 32 || message.toolCalls.size() > 32 ||
            !message.providerState.is_object())
            throw ProtocolError("Invalid message limits");
        ValidateReasoningContent(message.reasoning);
        if (message.role == MessageRole::Tool &&
            (message.toolCallId.empty() || message.toolCallId.size() > 256))
            throw ProtocolError("Tool result requires a call identity");
        for (auto const& part : message.content) {
            bytes += part.text.size() + part.base64.size();
            if (part.kind == ContentKind::Image &&
                (part.base64.empty() || part.base64.size() > 8u * 1024u * 1024u ||
                 (part.mediaType != "image/png" && part.mediaType != "image/jpeg" &&
                  part.mediaType != "image/webp" && part.mediaType != "image/gif")))
                throw ProtocolError("Invalid image content");
        }
        for (auto const& call : message.toolCalls) {
            if (call.id.empty() || call.id.size() > 256 || !Identifier(call.name) ||
                call.arguments.size() > 262144 || !ParseJson(call.arguments).is_object())
                throw ProtocolError("Invalid tool call");
        }
        bytes += message.providerState.dump().size();
    }
    if (bytes > 10u * 1024u * 1024u)
        throw ProtocolError("Request context size limit exceeded");
    std::set<std::string> names;
    for (auto const& tool : request.tools)
        if (!Identifier(tool.name) || !tool.inputSchema.is_object() ||
            tool.description.size() > 4096 || tool.inputSchema.dump().size() > 65536 ||
            !names.insert(tool.name).second)
            throw ProtocolError("Invalid tool definition");
}

void ValidateResult(ModelResult const& result) {
    ValidateReasoningContent(result.message.reasoning);
    if (result.message.role != MessageRole::Assistant ||
        PlainText(result.message).size() > 1024u * 1024u || result.message.toolCalls.size() > 32)
        throw ProtocolError("Invalid response limits");
    if (!result.message.toolCalls.empty() && result.finish != FinishReason::ToolCalls)
        throw ProtocolError("Unexpected tool calls in completed text response");
    if (result.finish == FinishReason::ToolCalls && result.message.toolCalls.empty())
        throw ProtocolError("Missing tool calls");
    std::set<std::string> ids;
    for (auto const& call : result.message.toolCalls)
        if (call.id.empty() || call.id.size() > 256 || !Identifier(call.name) ||
            call.arguments.size() > 262144 || !ids.insert(call.id).second ||
            !ParseJson(call.arguments).is_object())
            throw ProtocolError("Incomplete or invalid tool arguments");
}

} // namespace fairy::providers
