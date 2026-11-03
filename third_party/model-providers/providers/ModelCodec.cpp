#include "ModelCodec.h"

#include "ModelValidation.h"

#include <initializer_list>
#include <limits>

namespace fairy::providers {
namespace {
std::string RoleName(MessageRole role) {
    switch (role) {
    case MessageRole::System:
        return "system";
    case MessageRole::User:
        return "user";
    case MessageRole::Assistant:
        return "assistant";
    case MessageRole::Tool:
        return "tool";
    }
    throw ProtocolError("Invalid message role");
}

MessageRole ReadRole(std::string const& role) {
    if (role == "system")
        return MessageRole::System;
    if (role == "user")
        return MessageRole::User;
    if (role == "assistant")
        return MessageRole::Assistant;
    if (role == "tool")
        return MessageRole::Tool;
    throw ProtocolError("Invalid message role");
}

char const* ReasoningKindName(ReasoningKind kind) {
    switch (kind) {
    case ReasoningKind::ProviderText:
        return "text";
    case ReasoningKind::Summary:
        return "summary";
    }
    throw ProtocolError("Invalid reasoning kind");
}

ReasoningKind ReadReasoningKind(Json const& value) {
    if (!value.is_string())
        throw ProtocolError("Invalid reasoning kind");
    auto kind = value.get<std::string>();
    if (kind == "text")
        return ReasoningKind::ProviderText;
    if (kind == "summary")
        return ReasoningKind::Summary;
    throw ProtocolError("Invalid reasoning kind");
}

std::uint32_t ReadPositiveUint32(Json const& value) {
    std::uint64_t tokens{};
    if (value.is_number_unsigned()) {
        tokens = value.get<std::uint64_t>();
    } else if (value.is_number_integer()) {
        auto signedTokens = value.get<std::int64_t>();
        if (signedTokens <= 0)
            throw ProtocolError("Invalid output limit");
        tokens = static_cast<std::uint64_t>(signedTokens);
    } else {
        throw ProtocolError("Invalid output limit");
    }
    if (tokens == 0 || tokens > std::numeric_limits<std::uint32_t>::max())
        throw ProtocolError("Invalid output limit");
    return static_cast<std::uint32_t>(tokens);
}

Json MessageJson(Message const& message, bool includeReasoning) {
    Json value{{"role", RoleName(message.role)},
               {"content", Json::array()},
               {"toolCalls", Json::array()},
               {"toolCallId", message.toolCallId},
               {"providerState", message.providerState}};
    if (includeReasoning &&
        (!message.reasoning.text.empty() || !message.reasoning.summary.empty())) {
        ValidateReasoningContent(message.reasoning);
        value["reasoning"] = {{"text", message.reasoning.text},
                              {"summary", message.reasoning.summary}};
    }
    for (auto const& part : message.content)
        value["content"].push_back({{"kind", part.kind == ContentKind::Text ? "text" : "image"},
                                    {"text", part.text},
                                    {"mediaType", part.mediaType},
                                    {"base64", part.base64}});
    for (auto const& call : message.toolCalls)
        value["toolCalls"].push_back(
            {{"id", call.id}, {"name", call.name}, {"arguments", call.arguments}});
    return value;
}

Message ReadMessage(Json const& value) {
    Message message;
    message.role = ReadRole(value.at("role").get<std::string>());
    for (auto const& item : value.at("content")) {
        auto kind = item.at("kind").get<std::string>();
        if (kind != "text" && kind != "image")
            throw ProtocolError("Invalid content kind");
        message.content.push_back({kind == "text" ? ContentKind::Text : ContentKind::Image,
                                   item.at("text").get<std::string>(),
                                   item.at("mediaType").get<std::string>(),
                                   item.at("base64").get<std::string>()});
    }
    for (auto const& item : value.at("toolCalls"))
        message.toolCalls.push_back({item.at("id").get<std::string>(),
                                     item.at("name").get<std::string>(),
                                     item.at("arguments").get<std::string>()});
    message.toolCallId = value.at("toolCallId").get<std::string>();
    message.providerState = value.at("providerState");
    if (!message.providerState.is_object())
        throw ProtocolError("Invalid provider replay state");
    if (value.contains("reasoning")) {
        auto const& reasoning = value["reasoning"];
        if (!reasoning.is_object() || reasoning.size() != 2 || !reasoning.contains("text") ||
            !reasoning["text"].is_string() || !reasoning.contains("summary") ||
            !reasoning["summary"].is_string())
            throw ProtocolError("Invalid reasoning text");
        message.reasoning.text = reasoning["text"].get<std::string>();
        message.reasoning.summary = reasoning["summary"].get<std::string>();
    }
    return message;
}

Json UsageJson(Usage const& usage) {
    Json value = Json::object();
    if (usage.inputTokens)
        value["input"] = *usage.inputTokens;
    if (usage.outputTokens)
        value["output"] = *usage.outputTokens;
    if (usage.totalTokens)
        value["total"] = *usage.totalTokens;
    if (usage.cachedInputTokens)
        value["cached"] = *usage.cachedInputTokens;
    return value;
}

Usage ReadUsage(Json const& value) {
    Usage usage;
    auto read = [&value](char const* key) -> std::optional<std::uint64_t> {
        if (!value.contains(key))
            return {};
        if (!value[key].is_number_unsigned() && !value[key].is_number_integer())
            throw ProtocolError("Invalid token usage");
        if (value[key].is_number_integer() && value[key].get<std::int64_t>() < 0)
            throw ProtocolError("Negative token usage");
        return value[key].get<std::uint64_t>();
    };
    usage.inputTokens = read("input");
    usage.outputTokens = read("output");
    usage.totalTokens = read("total");
    usage.cachedInputTokens = read("cached");
    return usage;
}

std::string EventKindName(StreamEventKind kind) {
    switch (kind) {
    case StreamEventKind::TextDelta:
        return "text_delta";
    case StreamEventKind::ReasoningDelta:
        return "reasoning_delta";
    case StreamEventKind::ToolCallDelta:
        return "tool_call_delta";
    case StreamEventKind::UsageUpdated:
        return "usage_updated";
    }
    throw ProtocolError("Invalid stream event kind");
}

StreamEventKind ReadEventKind(std::string const& kind) {
    if (kind == "text_delta")
        return StreamEventKind::TextDelta;
    if (kind == "reasoning_delta")
        return StreamEventKind::ReasoningDelta;
    if (kind == "tool_call_delta")
        return StreamEventKind::ToolCallDelta;
    if (kind == "usage_updated")
        return StreamEventKind::UsageUpdated;
    throw ProtocolError("Invalid stream event kind");
}

void ValidateEventText(std::string const& text, std::size_t limit, char const* diagnostic) {
    if (text.size() > limit || text.find('\0') != std::string::npos)
        throw ProtocolError(diagnostic);
}

void ValidateStreamEvent(StreamEvent const& event) {
    switch (event.kind) {
    case StreamEventKind::TextDelta:
        if (event.reasoningKind != ReasoningKind::ProviderText)
            throw ProtocolError("Unexpected reasoning kind in text event");
        ValidateEventText(event.text, 1024u * 1024u, "Invalid stream text delta");
        if (!event.callId.empty() || !event.name.empty() || event.usage.inputTokens ||
            event.usage.outputTokens || event.usage.totalTokens || event.usage.cachedInputTokens)
            throw ProtocolError("Unexpected fields in text stream event");
        return;
    case StreamEventKind::ReasoningDelta:
        if (event.reasoningKind != ReasoningKind::ProviderText &&
            event.reasoningKind != ReasoningKind::Summary)
            throw ProtocolError("Invalid reasoning kind");
        ValidateReasoningText(event.text);
        if (!event.callId.empty() || !event.name.empty() || event.usage.inputTokens ||
            event.usage.outputTokens || event.usage.totalTokens || event.usage.cachedInputTokens)
            throw ProtocolError("Unexpected fields in text stream event");
        return;
    case StreamEventKind::ToolCallDelta:
        if (event.reasoningKind != ReasoningKind::ProviderText)
            throw ProtocolError("Unexpected reasoning kind in tool event");
        ValidateEventText(event.text, 1024u * 1024u, "Invalid tool call delta");
        ValidateEventText(event.callId, 256, "Invalid tool call identity delta");
        ValidateEventText(event.name, 64, "Invalid tool name delta");
        if (event.usage.inputTokens || event.usage.outputTokens || event.usage.totalTokens ||
            event.usage.cachedInputTokens)
            throw ProtocolError("Unexpected usage in tool stream event");
        return;
    case StreamEventKind::UsageUpdated:
        if (!event.text.empty() || !event.callId.empty() || !event.name.empty() ||
            event.reasoningKind != ReasoningKind::ProviderText)
            throw ProtocolError("Unexpected fields in usage stream event");
        return;
    }
    throw ProtocolError("Invalid stream event kind");
}

bool HasOnlyFields(Json const& value, std::initializer_list<char const*> fields) {
    if (!value.is_object() || value.size() != fields.size())
        return false;
    for (auto const* field : fields)
        if (!value.contains(field))
            return false;
    return true;
}

bool HasOnlyUsageFields(Json const& value) {
    if (!value.is_object() || value.size() > 4)
        return false;
    for (auto const& [name, field] : value.items()) {
        (void)field;
        if (name != "input" && name != "output" && name != "total" && name != "cached")
            return false;
    }
    return true;
}
} // namespace

Json ToJson(ModelRequest const& request) {
    Json value{{"model", request.model},
               {"maxOutputTokens", request.maxOutputTokens},
               {"stream", request.stream},
               {"messages", Json::array()},
               {"tools", Json::array()}};
    if (!request.reasoning.effort.empty() || request.reasoning.summary)
        value["reasoning"] = {{"effort", request.reasoning.effort},
                              {"summary", request.reasoning.summary}};
    for (auto const& message : request.messages)
        value["messages"].push_back(MessageJson(message, false));
    for (auto const& tool : request.tools)
        value["tools"].push_back({{"name", tool.name},
                                  {"description", tool.description},
                                  {"inputSchema", tool.inputSchema}});
    return value;
}

ModelRequest RequestFromJson(Json const& value) {
    try {
        ModelRequest request;
        request.model = value.at("model").get<std::string>();
        request.maxOutputTokens = ReadPositiveUint32(value.at("maxOutputTokens"));
        request.stream = value.at("stream").get<bool>();
        if (value.contains("reasoning")) {
            auto const& reasoning = value["reasoning"];
            if (!reasoning.is_object() || reasoning.size() != 2 || !reasoning.contains("effort") ||
                !reasoning["effort"].is_string() || !reasoning.contains("summary") ||
                !reasoning["summary"].is_boolean())
                throw ProtocolError("Invalid reasoning options");
            request.reasoning.effort = reasoning["effort"].get<std::string>();
            request.reasoning.summary = reasoning["summary"].get<bool>();
        }
        if (!value.at("messages").is_array() || !value.at("tools").is_array())
            throw ProtocolError("Invalid request arrays");
        for (auto const& message : value.at("messages"))
            request.messages.push_back(ReadMessage(message));
        for (auto const& tool : value.at("tools"))
            request.tools.push_back({tool.at("name").get<std::string>(),
                                     tool.at("description").get<std::string>(),
                                     tool.at("inputSchema")});
        ValidateRequest(request);
        return request;
    } catch (Json::exception const&) {
        throw ProtocolError("Invalid request envelope");
    }
}

Json ToJson(ModelResult const& result) {
    return {{"message", MessageJson(result.message, true)},
            {"usage", UsageJson(result.usage)},
            {"finish", static_cast<int>(result.finish)},
            {"responseId", result.responseId}};
}

ModelResult ResultFromJson(Json const& value) {
    try {
        ModelResult result;
        result.message = ReadMessage(value.at("message"));
        result.usage = ReadUsage(value.at("usage"));
        auto finish = value.at("finish").get<int>();
        if (finish < 0 || finish > 3)
            throw ProtocolError("Invalid finish reason");
        result.finish = static_cast<FinishReason>(finish);
        result.responseId = value.at("responseId").get<std::string>();
        ValidateResult(result);
        return result;
    } catch (Json::exception const&) {
        throw ProtocolError("Invalid response envelope");
    }
}

Json ToJson(StreamEvent const& event) {
    ValidateStreamEvent(event);
    Json value{{"kind", EventKindName(event.kind)}};
    switch (event.kind) {
    case StreamEventKind::TextDelta:
        value["text"] = event.text;
        break;
    case StreamEventKind::ReasoningDelta:
        value["text"] = event.text;
        value["reasoningKind"] = ReasoningKindName(event.reasoningKind);
        break;
    case StreamEventKind::ToolCallDelta:
        value["callId"] = event.callId;
        value["name"] = event.name;
        value["text"] = event.text;
        break;
    case StreamEventKind::UsageUpdated:
        value["usage"] = UsageJson(event.usage);
        break;
    }
    try {
        if (value.dump().size() > 1024u * 1024u)
            throw ProtocolError("Stream event size limit exceeded");
    } catch (Json::exception const&) {
        throw ProtocolError("Invalid stream event encoding");
    }
    return value;
}

StreamEvent StreamEventFromJson(Json const& value) {
    try {
        if (!value.is_object() || !value.contains("kind") || !value["kind"].is_string())
            throw ProtocolError("Invalid stream event envelope");
        const auto kind = ReadEventKind(value["kind"].get<std::string>());
        StreamEvent event;
        event.kind = kind;
        switch (kind) {
        case StreamEventKind::TextDelta:
            if (!HasOnlyFields(value, {"kind", "text"}) || !value["text"].is_string())
                throw ProtocolError("Invalid text stream event");
            event.text = value["text"].get<std::string>();
            break;
        case StreamEventKind::ReasoningDelta:
            if ((!HasOnlyFields(value, {"kind", "text"}) &&
                 !HasOnlyFields(value, {"kind", "text", "reasoningKind"})) ||
                !value["text"].is_string())
                throw ProtocolError("Invalid reasoning stream event");
            if (value.contains("reasoningKind"))
                event.reasoningKind = ReadReasoningKind(value["reasoningKind"]);
            event.text = value["text"].get<std::string>();
            break;
        case StreamEventKind::ToolCallDelta:
            if (!HasOnlyFields(value, {"kind", "callId", "name", "text"}) ||
                !value["callId"].is_string() || !value["name"].is_string() ||
                !value["text"].is_string())
                throw ProtocolError("Invalid tool stream event");
            event.callId = value["callId"].get<std::string>();
            event.name = value["name"].get<std::string>();
            event.text = value["text"].get<std::string>();
            break;
        case StreamEventKind::UsageUpdated:
            if (!HasOnlyFields(value, {"kind", "usage"}) || !HasOnlyUsageFields(value["usage"]))
                throw ProtocolError("Invalid usage stream event");
            event.usage = ReadUsage(value["usage"]);
            break;
        }
        if (value.dump().size() > 1024u * 1024u)
            throw ProtocolError("Stream event size limit exceeded");
        ValidateStreamEvent(event);
        return event;
    } catch (Json::exception const&) {
        throw ProtocolError("Invalid stream event envelope");
    }
}

} // namespace fairy::providers
