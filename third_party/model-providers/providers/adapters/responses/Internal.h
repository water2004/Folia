#pragma once
#include "../../JsonParsing.h"
#include "../../ModelAdapter.h"
#include "../../ModelValidation.h"
#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace fairy::providers {
namespace {
[[noreturn]] void Bad(char const* message, ProtocolFailure failure = ProtocolFailure::InvalidData) {
    throw ProtocolError(message, failure);
}
std::string String(Json const& value, char const* key) {
    auto it = value.find(key);
    return it != value.end() && it->is_string() ? it->get<std::string>() : std::string{};
}
std::uint64_t Unsigned(Json const& value, char const* key) {
    auto it = value.find(key);
    if (it == value.end() || !it->is_number_integer())
        Bad("Responses numeric field is invalid.", ProtocolFailure::InvalidStreamField);
    try {
        if (it->is_number_unsigned())
            return it->get<std::uint64_t>();
        auto number = it->get<std::int64_t>();
        if (number < 0)
            Bad("Responses numeric field is negative.", ProtocolFailure::InvalidStreamField);
        return static_cast<std::uint64_t>(number);
    } catch (ProtocolError const&) {
        throw;
    } catch (...) {
        Bad("Responses numeric field is out of range.", ProtocolFailure::InvalidStreamField);
    }
}
std::string RequiredString(Json const& value, char const* key) {
    auto it = value.find(key);
    if (it == value.end() || !it->is_string())
        Bad("Responses string field is invalid.", ProtocolFailure::InvalidStreamField);
    return it->get<std::string>();
}
std::optional<std::uint64_t> OptionalUsageCount(Json const& usage, char const* key) {
    auto it = usage.find(key);
    if (it == usage.end())
        return {};
    return Unsigned(usage, key);
}
Json ParseFunctionArguments(std::string const& arguments) {
    Json parsed;
    try {
        parsed = ParseJson(arguments);
    } catch (...) {
        Bad("Responses function arguments are invalid JSON.",
            ProtocolFailure::InvalidFunctionArguments);
    }
    if (!parsed.is_object())
        Bad("Responses function arguments must be a JSON object.",
            ProtocolFailure::InvalidFunctionArguments);
    return parsed;
}
Json InputContent(Message const& message) {
    Json content = Json::array();
    for (auto const& part : message.content) {
        if (part.kind == ContentKind::Text)
            content.push_back({{"type", "input_text"}, {"text", part.text}});
        else {
            auto mime = part.mediaType.empty() ? "image/png" : part.mediaType;
            content.push_back(
                {{"type", "input_image"},
                 {"image_url", std::string("data:") + mime + ";base64," + part.base64}});
        }
    }
    if (content.empty())
        content.push_back({{"type", "input_text"}, {"text", ""}});
    return content;
}
void AddResponseOutput(ModelResult& result, Json const& response, bool incomplete) {
    if (!response.contains("output") || !response["output"].is_array())
        Bad("Responses response has invalid output.");
    result.message.role = MessageRole::Assistant;
    result.message.providerState["responses_output"] = response["output"];
    std::string responseSummary;
    std::string responseText;
    for (auto const& item : response["output"]) {
        if (!item.is_object())
            Bad("Responses output item is invalid.");
        auto type = String(item, "type");
        if (type == "message") {
            if (item.contains("content") && item["content"].is_array())
                for (auto const& part : item["content"]) {
                    auto partType = String(part, "type");
                    if (partType == "output_text" && part.contains("text") &&
                        part["text"].is_string())
                        result.message.content.push_back(
                            {ContentKind::Text, part["text"].get<std::string>(), {}, {}});
                    else if (partType == "refusal" && part.contains("refusal") &&
                             part["refusal"].is_string())
                        result.message.content.push_back(
                            {ContentKind::Text, part["refusal"].get<std::string>(), {}, {}});
                }
        } else if (type == "reasoning") {
            auto summary = item.find("summary");
            if (summary != item.end()) {
                if (!summary->is_array())
                    Bad("Responses reasoning summary is invalid.");
                for (auto const& part : *summary) {
                    if (!part.is_object())
                        Bad("Responses reasoning summary part is invalid.");
                    if (String(part, "type") == "summary_text") {
                        auto text = RequiredString(part, "text");
                        if (text.size() > 1024u * 1024u - responseSummary.size())
                            Bad("Responses reasoning summary exceeds limits.");
                        responseSummary += text;
                    }
                }
            }
            auto content = item.find("content");
            if (content != item.end()) {
                if (!content->is_array())
                    Bad("Responses reasoning content is invalid.");
                for (auto const& part : *content) {
                    if (!part.is_object())
                        Bad("Responses reasoning content part is invalid.");
                    if (String(part, "type") == "reasoning_text") {
                        auto text = RequiredString(part, "text");
                        if (text.size() > 1024u * 1024u - responseText.size())
                            Bad("Responses reasoning text exceeds limits.");
                        responseText += text;
                    }
                }
            }
        } else if (type == "function_call") {
            auto id = String(item, "call_id");
            auto name = String(item, "name");
            auto arguments = String(item, "arguments");
            if (id.empty() || name.empty() || arguments.empty()) {
                if (!incomplete)
                    Bad("Responses function call is incomplete.");
                continue;
            }
            try {
                (void)ParseFunctionArguments(arguments);
            } catch (...) {
                if (incomplete)
                    continue;
                throw;
            }
            result.message.toolCalls.push_back({id, name, arguments});
        }
    }
    result.message.reasoning.text = std::move(responseText);
    result.message.reasoning.summary = std::move(responseSummary);
    if (incomplete)
        result.message.toolCalls.clear();
}
FinishReason IncompleteFinish(Json const& response) {
    auto const details = response.find("incomplete_details");
    if (details == response.end() || details->is_null())
        Bad("Responses response did not complete.", ProtocolFailure::ProviderReportedFailure);
    if (!details->is_object() || details->size() > 1 ||
        (details->size() == 1 && !details->contains("reason")))
        Bad("Responses incomplete details are malformed.", ProtocolFailure::InvalidTerminal);
    auto const reason = details->find("reason");
    if (reason == details->end() || reason->is_null())
        Bad("Responses response did not complete.", ProtocolFailure::ProviderReportedFailure);
    if (!reason->is_string())
        Bad("Responses incomplete reason is malformed.", ProtocolFailure::InvalidTerminal);
    auto const& value = reason->get_ref<std::string const&>();
    if (value == "max_output_tokens")
        return FinishReason::Truncated;
    if (value == "content_filter")
        return FinishReason::Refused;
    Bad("Responses response did not complete.", ProtocolFailure::ProviderReportedFailure);
}
Usage ParseUsage(Json const& response) {
    Usage usage;
    auto it = response.find("usage");
    if (it == response.end() || it->is_null())
        return usage;
    if (!it->is_object())
        Bad("Responses usage is malformed.");
    usage.inputTokens = OptionalUsageCount(*it, "input_tokens");
    usage.outputTokens = OptionalUsageCount(*it, "output_tokens");
    usage.totalTokens = OptionalUsageCount(*it, "total_tokens");
    auto details = it->find("input_tokens_details");
    if (details != it->end()) {
        if (!details->is_object())
            Bad("Responses usage details are malformed.");
        usage.cachedInputTokens = OptionalUsageCount(*details, "cached_tokens");
    }
    return usage;
}
ModelResult ParseTerminal(Json const& response, bool incomplete) {
    auto const status = String(response, "status");
    if ((status != "completed" && status != "incomplete") || incomplete != (status == "incomplete"))
        Bad("Responses terminal status is invalid.");
    ModelResult result;
    result.responseId = String(response, "id");
    result.finish = incomplete ? IncompleteFinish(response) : FinishReason::Completed;
    result.usage = ParseUsage(response);
    AddResponseOutput(result, response, incomplete);
    if (!incomplete && !result.message.toolCalls.empty())
        result.finish = FinishReason::ToolCalls;
    if (!incomplete)
        for (auto const& item : response["output"])
            if (item.is_object() && item.contains("content") && item["content"].is_array())
                for (auto const& part : item["content"])
                    if (String(part, "type") == "refusal")
                        result.finish = FinishReason::Refused;
    if (!result.message.toolCalls.empty()) {
        auto callsOnly = result;
        callsOnly.finish = FinishReason::ToolCalls;
        ValidateResult(callsOnly);
    }
    if (incomplete || result.finish == FinishReason::Refused)
        result.message.toolCalls.clear();
    ValidateResult(result);
    return result;
} // namespace
} // namespace
} // namespace fairy::providers
